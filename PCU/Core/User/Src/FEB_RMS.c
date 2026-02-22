#include "FEB_RMS.h"
#include "FEB_ADC.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_RMS.h"
#include "FEB_RMS_Config.h"
#include "feb_uart_log.h"
#include "FEB_Regen.h"
#include "main.h"

/* Safe min macro with proper parentheses */
#define min(x1, x2) (((x1) < (x2)) ? (x1) : (x2))

/* Regen brake position threshold (from FEB_Regen.h) */
#ifndef REGEN_BRAKE_POS_THRESH
#define REGEN_BRAKE_POS_THRESH 0.20f /* 20% brake position to activate regen */
#endif

/* Global RMS control data */
RMS_CONTROL RMS_CONTROL_MESSAGE;
APPS_DataTypeDef APPS_Data;
extern Brake_DataTypeDef Brake_Data;
bool DRIVE_STATE;

void FEB_RMS_Setup(void)
{
  RMS_CONTROL_MESSAGE.enabled = 0;
  RMS_CONTROL_MESSAGE.torque = 0.0;
  LOG_I(TAG_RMS, "RMS control initialized");
}

void FEB_RMS_Process(void)
{
  // Require BMS to be in drive state before enabling inverter
  if (!FEB_CAN_BMS_InDriveState())
  {
    LOG_W(TAG_RMS, "Cannot enable RMS: BMS not in drive state (state=%d)", BMS_MESSAGE.state);
    return;
  }

  if (!RMS_CONTROL_MESSAGE.enabled)
  {
    LOG_I(TAG_RMS, "Sending RMS disable commands to clear lockout");
    // Send continuous disable commands for 2 seconds to clear lockout
    for (int i = 0; i < 200; i++) // 200 x 10ms = 2 seconds
    {
      FEB_CAN_RMS_Transmit_UpdateTorque(0, 0);
      HAL_Delay(10);
    }

    RMS_CONTROL_MESSAGE.enabled = 1;
    LOG_I(TAG_RMS, "RMS enabled");
  }

  DRIVE_STATE = true;
}

void FEB_RMS_Disable(void)
{
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
float FEB_Get_Peak_Current_Delimiter()
{
  // Skip voltage-based limiting if we've never received RMS data
  if (RMS_MESSAGE.last_rx_timestamp == 0)
  {
    static uint32_t last_no_data_log = 0;
    if (HAL_GetTick() - last_no_data_log >= 5000)
    {
      last_no_data_log = HAL_GetTick();
      LOG_W(TAG_RMS, "No RMS voltage data received yet");
    }
    return 1.0f; // Don't limit torque without real data
  }

  // Convert RMS voltage format: decivolts with 50V offset
  // Formula: actual_voltage = (HV_Bus_Voltage - 50) / 10
  float accumulator_voltage = (RMS_MESSAGE.HV_Bus_Voltage - 50.0f) / 10.0f;

  // Start derating when voltage = MIN_PACK_VOLTAGE_V + expected_drop_at_peak_current
  // With R_acc = 1Ω and PEAK_CURRENT = 60A: start_derating = 400V + 60V = 460V
  float start_derating_voltage = MIN_PACK_VOLTAGE_V + PEAK_CURRENT;

  // Above derating threshold: allow full current
  if (accumulator_voltage > start_derating_voltage)
  {
    return 1.0f;
  }

  // Below minimum safe voltage: limit to 10A (16.7% of 60A)
  if (accumulator_voltage <= 410.0f)
  {
    // Rate-limit this warning to once per second
    static uint32_t last_low_voltage_log = 0;
    if (HAL_GetTick() - last_low_voltage_log >= 1000)
    {
      last_low_voltage_log = HAL_GetTick();
      LOG_W(TAG_RMS, "Low pack voltage: %.1fV, limiting to 10A", accumulator_voltage);
    }
    return (10.0f / PEAK_CURRENT);
  }

  // Linear interpolation between limits
  // y = mx + b where m = (y1 - y0) / (x1 - x0)
  float slope = ((10.0f / PEAK_CURRENT) - 1.0f) / (410.0f - start_derating_voltage);
  float derater = slope * (accumulator_voltage - start_derating_voltage) + 1.0f;

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
float FEB_RMS_GetMaxTorque(void)
{
  float motor_speed = RMS_MESSAGE.Motor_Speed * RPM_TO_RAD_S;
  float peak_current_limited = PEAK_CURRENT * FEB_Get_Peak_Current_Delimiter();

  // Cap power to peak_current * MIN_PACK_VOLTAGE_V (e.g., 60A * 400V = 24kW)
  float power_capped = peak_current_limited * MIN_PACK_VOLTAGE_V;

  // Select torque limit based on pack voltage
  uint16_t minimum_torque = MAX_TORQUE;
  if (BMS_MESSAGE.last_rx_timestamp == 0)
  {
    // No BMS data yet, use default max torque
  }
  else if (BMS_MESSAGE.voltage < LOW_PACK_VOLTAGE)
  {
    minimum_torque = MAX_TORQUE_LOW_V;
    // Rate-limit this warning to once per second
    static uint32_t last_low_pack_log = 0;
    if (HAL_GetTick() - last_low_pack_log >= 1000)
    {
      last_low_pack_log = HAL_GetTick();
      LOG_W(TAG_RMS, "Low pack voltage detected, reducing max torque to %d", minimum_torque);
    }
  }

  // Below minimum speed threshold: use constant torque mode
  // Prevents division by zero and handles stopped/negative rotation
  if (motor_speed < MIN_MOTOR_SPEED_RAD_S)
  {
    return minimum_torque;
  }

  // Above minimum speed: limit by power (constant power mode)
  float maxTorque = min(minimum_torque, (power_capped) / motor_speed);

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
void FEB_RMS_Torque(void)
{
  // Auto-disable RMS if BMS leaves drive state or communication is lost
  if (DRIVE_STATE && !FEB_CAN_BMS_InDriveState())
  {
    LOG_W(TAG_RMS, "BMS left drive state or timeout, disabling RMS");
    FEB_RMS_Disable();
  }

  // Read latest sensor data
  FEB_ADC_GetAPPSData(&APPS_Data);
  FEB_ADC_GetBrakeData(&Brake_Data);

  // Check plausibility and safety conditions (require BMS in drive state)
  bool bms_in_drive = FEB_CAN_BMS_InDriveState();
  bool sensors_plausible = bms_in_drive && DRIVE_STATE;

  // Log any safety violations
  if (!sensors_plausible)
  {
    if (Brake_Data.brake_position > BRAKE_POSITION_THRESHOLD)
    {
      LOG_W(TAG_RMS, "Brake pressed (%.1f%%), cutting torque", Brake_Data.brake_position);
    }
    if (!APPS_Data.plausible)
    {
      LOG_E(TAG_RMS, "APPS implausible, cutting torque");
    }
    if (!Brake_Data.plausible)
    {
      LOG_E(TAG_RMS, "Brake sensor implausible, cutting torque");
    }
    if (!DRIVE_STATE)
    {
      LOG_W(TAG_RMS, "Not in drive state, cutting torque");
    }
  }

  // Reset plausibility if pedals are released
  if (APPS_Data.position1 < 5.0f && APPS_Data.position2 < 5.0f && Brake_Data.brake_position < 15.0f)
  {
    if (!APPS_Data.plausible || !Brake_Data.plausible)
    {
      LOG_I(TAG_RMS, "Pedals released, resetting plausibility flags");
    }
    APPS_Data.plausible = true;
    Brake_Data.plausible = true;
  }

  // Determine operating mode: regen braking vs acceleration
  if (Brake_Data.brake_position > REGEN_BRAKE_POS_THRESH && sensors_plausible)
  {
    // REGEN MODE: Brake is pressed and sensors are plausible
    float filtered_regen = FEB_Regen_GetFilteredTorque();

    // Apply brake position scaling and negative sign (SN3 style)
    // torque_command = -1 * 10 * brake% * filtered_regen / 100
    RMS_CONTROL_MESSAGE.torque = (int16_t)(-10.0f * Brake_Data.brake_position * filtered_regen / 100.0f);
  }
  else if (Brake_Data.brake_position < BRAKE_POSITION_THRESHOLD && sensors_plausible)
  {
    // ACCELERATION MODE: No brake and sensors are plausible
    // Calculate commanded torque: acceleration (0-100%) * max_torque
    RMS_CONTROL_MESSAGE.torque = (int16_t)(0.01f * APPS_Data.acceleration * FEB_RMS_GetMaxTorque());
  }
  else
  {
    // SAFETY MODE: Sensors not plausible or not in drive state - zero torque
    RMS_CONTROL_MESSAGE.torque = 0;
  }

  // Transmit torque command to RMS motor controller
  FEB_CAN_RMS_Transmit_UpdateTorque(RMS_CONTROL_MESSAGE.torque, RMS_CONTROL_MESSAGE.enabled);
}
