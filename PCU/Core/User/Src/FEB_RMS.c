#include "FEB_RMS.h"
#include "FEB_ADC.h"
#include "FEB_RMS_Config.h"
#include "FEB_Debug.h"
#include "main.h"

/* Safe min macro with proper parentheses */
#define min(x1, x2) (((x1) < (x2)) ? (x1) : (x2))

/* Global RMS control data */
RMS_CONTROL RMS_CONTROL_MESSAGE;
APPS_DataTypeDef APPS_Data;
extern Brake_DataTypeDef Brake_Data;
bool DRIVE_STATE;

void FEB_RMS_Setup(void) {
	RMS_CONTROL_MESSAGE.enabled = 0;
	RMS_CONTROL_MESSAGE.torque = 0.0;
	LOG_I(TAG_RMS, "RMS control initialized");
}

void FEB_RMS_Process(void) {
	if (!RMS_CONTROL_MESSAGE.enabled){
		RMS_CONTROL_MESSAGE.enabled = 1;
		LOG_I(TAG_RMS, "RMS enabled");
	}

	DRIVE_STATE = true;
}

void FEB_RMS_Disable(void) {
	RMS_CONTROL_MESSAGE.enabled = 0;
	LOG_W(TAG_RMS, "RMS disabled");

	DRIVE_STATE = false;
}

/**
 * @brief Calculate current derating factor based on pack voltage
 *
 * To prevent pack voltage from dropping below 400V (~2.85V/cell for 140S),
 * we derate the peak current limit as voltage approaches the minimum threshold.
 *
 * Based on empirical data:
 * - At 510V, commanding 65A causes ~62V drop
 * - This suggests pack resistance ≈ 1 Ohm
 * - Linear interpolation between (460V, 100% current) and (410V, 16.7% current)
 *
 * @return Current derating multiplier (0.167 to 1.0)
 */
float FEB_Get_Peak_Current_Delimiter() {
	// Convert RMS voltage format: decivolts with 50V offset
	// Formula: actual_voltage = (HV_Bus_Voltage - 50) / 10
	float accumulator_voltage = (RMS_MESSAGE.HV_Bus_Voltage - 50.0f) / 10.0f;

	// Start derating when voltage = MIN_PACK_VOLTAGE_V + expected_drop_at_peak_current
	// With R_acc = 1Ω and PEAK_CURRENT = 60A: start_derating = 400V + 60V = 460V
	float start_derating_voltage = MIN_PACK_VOLTAGE_V + PEAK_CURRENT;

	// Above derating threshold: allow full current
	if (accumulator_voltage > start_derating_voltage) {
		return 1.0f;
	}

	// Below minimum safe voltage: limit to 10A (16.7% of 60A)
	if (accumulator_voltage <= 410.0f) {
		LOG_W(TAG_RMS, "Low pack voltage: %.1fV, limiting to 10A", accumulator_voltage);
		return (10.0f / PEAK_CURRENT);
	}

	// Linear interpolation between limits
	// y = mx + b where m = (y1 - y0) / (x1 - x0)
	float slope = ((10.0f / PEAK_CURRENT) - 1.0f) / (410.0f - start_derating_voltage);
	float derater = slope * (accumulator_voltage - start_derating_voltage) + 1.0f;

	LOG_D(TAG_RMS, "Voltage derating: %.1fV -> %.1f%% current", accumulator_voltage, derater * 100.0f);

	return derater;
}

/**
 * @brief Calculate maximum allowable motor torque based on speed and voltage
 *
 * Implements power limiting to protect accumulator and comply with FSAE rules.
 * Uses constant torque at low speeds, transitions to constant power at high speeds.
 *
 * @return Maximum torque in tenths of Nm (e.g., 2300 = 230.0 Nm)
 */
float FEB_RMS_GetMaxTorque(void){
	float motor_speed = RMS_MESSAGE.Motor_Speed * RPM_TO_RAD_S;
	float peak_current_limited = PEAK_CURRENT * FEB_Get_Peak_Current_Delimiter();

	// Cap power to peak_current * MIN_PACK_VOLTAGE_V (e.g., 60A * 400V = 24kW)
	float power_capped = peak_current_limited * MIN_PACK_VOLTAGE_V;

	// Select torque limit based on pack voltage
	uint16_t minimum_torque = MAX_TORQUE;
	if (BMS_MESSAGE.voltage < LOW_PACK_VOLTAGE) {
		minimum_torque = MAX_TORQUE_LOW_V;
		LOG_W(TAG_RMS, "Low pack voltage detected, reducing max torque to %d", minimum_torque);
	}

	// Below minimum speed threshold: use constant torque mode
	// Prevents division by zero and handles stopped/negative rotation
	if (motor_speed < MIN_MOTOR_SPEED_RAD_S) {
		LOG_D(TAG_RMS, "Low motor speed: %.1f rad/s, using constant torque: %d", motor_speed, minimum_torque);
		return minimum_torque;
	}

	// Above minimum speed: limit by power (constant power mode)
	float maxTorque = min(minimum_torque, (power_capped) / motor_speed);

	LOG_D(TAG_RMS, "Max torque: %.1f Nm (speed: %.1f rad/s, power: %.1f W)", maxTorque / 10.0f, motor_speed, power_capped);

	return maxTorque;
}

/**
 * @brief Main torque control function - reads sensors and commands motor
 *
 * Implements FSAE EV.5.6 and EV.5.7 safety rules:
 * - Cuts torque if brake is pressed while throttle > 25%
 * - Enforces APPS plausibility checks
 * - Requires pedal release to clear faults
 *
 * Called periodically from main loop
 */
void FEB_RMS_Torque(void){

	// Read latest sensor data
	FEB_ADC_GetAPPSData(&APPS_Data);
	FEB_ADC_GetBrakeData(&Brake_Data);

	// Cut torque if brake pressed, plausibility fault, or not in drive state
	if (Brake_Data.brake_position > BRAKE_POSITION_THRESHOLD ||
	    !APPS_Data.plausible ||
	    !Brake_Data.plausible ||
	    !DRIVE_STATE) {
		if (Brake_Data.brake_position > BRAKE_POSITION_THRESHOLD) {
			LOG_W(TAG_RMS, "Brake pressed (%.1f%%), cutting torque", Brake_Data.brake_position);
		}
		if (!APPS_Data.plausible) {
			LOG_E(TAG_RMS, "APPS implausible, cutting torque");
		}
		if (!Brake_Data.plausible) {
			LOG_E(TAG_RMS, "Brake sensor implausible, cutting torque");
		}
		if (!DRIVE_STATE) {
			LOG_W(TAG_RMS, "Not in drive state, cutting torque");
		}
		APPS_Data.acceleration = 0.0f;
	}

	// Reset plausibility if pedals are released
	if (APPS_Data.position1 < 5.0f && APPS_Data.position2 < 5.0f && Brake_Data.brake_position < 15.0f) {
		if (!APPS_Data.plausible || !Brake_Data.plausible) {
			LOG_I(TAG_RMS, "Pedals released, resetting plausibility flags");
		}
		APPS_Data.plausible = true;
		Brake_Data.plausible = true;
	}

	// Calculate commanded torque: acceleration (0-100%) * max_torque
	// Convert percentage to fraction: multiply by 0.01, not 0.1
	RMS_CONTROL_MESSAGE.torque = 0.01f * APPS_Data.acceleration * FEB_RMS_GetMaxTorque();

	LOG_D(TAG_RMS, "Torque command: %.1f Nm (APPS: %.1f%%, Enabled: %d)", 
	      RMS_CONTROL_MESSAGE.torque / 10.0f, APPS_Data.acceleration, RMS_CONTROL_MESSAGE.enabled);

	// Transmit torque command to RMS motor controller
	FEB_CAN_RMS_Transmit_UpdateTorque(RMS_CONTROL_MESSAGE.torque, RMS_CONTROL_MESSAGE.enabled);
}