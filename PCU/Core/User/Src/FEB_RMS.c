#include "FEB_RMS.h"
#include "FEB_ADC.h"
#include "main.h"

#define min(x1, x2) x1 < x2 ? x1 : x2;

bool DRIVE_STATE;

void FEB_RMS_Setup(void) {
	RMS_CONTROL_MESSAGE.enabled = 0;
	RMS_CONTROL_MESSAGE.torque = 0.0;
}

void FEB_RMS_Process(void) {
	if (!RMS_CONTROL_MESSAGE.enabled){
		RMS_CONTROL_MESSAGE.enabled = 1;
	}

	DRIVE_STATE = true;
}

void FEB_RMS_Disable(void) {
	RMS_CONTROL_MESSAGE.enabled = 0;

	DRIVE_STATE = false;
}



// Essentially we want our voltage to never drop below 400 to be safe (~2.85V per cell) 
// To keep 400 as our floor setpoint, we will derate our torque limit based on voltage
// We will control this by derating our PEAK_CURRENT value. 
// Based on data at 510 V, we see that the voltage drops about 62V when commanding 65 A of current
// This means we will assume the pack resistance is about 1 Ohm
// Note this will likely be steeper as we approach lower SOC, but these cell dynamics are hopefully
// negligible due to the heavy current limiting and the ohmic losses of the accumulator
// Linear interpolation between (460V, 60/60A) and (410V, 10/60A), where PEAK_CURRENT is 60A
float FEB_Get_Peak_Current_Delimiter() {
	float accumulator_voltage = (RMS_MESSAGE.HV_Bus_Voltage-50.0) / 10.0;
	float estimated_voltage_drop_at_peak = PEAK_CURRENT;
	float start_derating_voltage = 400.0 + PEAK_CURRENT; // Assume R_acc = 1ohm
	// Note: Comments are based on start_derating_voltage = 460V and PEAK_CURRENT = 60

	// Saturate outside of 460 and 410
	if (accumulator_voltage > start_derating_voltage)
	{
		return 1;
	}
	if (accumulator_voltage <= 410)
	{
		return (10.0 / PEAK_CURRENT); // limit to only 10A 
	}

	//      m   = (        y_1           -              y_0)              / (x_1 -          x_0)
	float slope = ((10.0 / PEAK_CURRENT) - (PEAK_CURRENT / PEAK_CURRENT)) / (410.0 - (start_derating_voltage));
	//      y     =   m     (       x            -          x_0          ) + y_0
	float derater = slope * (accumulator_voltage - start_derating_voltage) + 1.0;

	return derater;   
}

float FEB_RMS_GetMaxTorque(void){
	// float accumulator_voltage = min(INIT_VOLTAGE, (RMS_MESSAGE.HV_Bus_Voltage-50) / 10);
	float motor_speed = RMS_MESSAGE.Motor_Speed * RPM_TO_RAD_S;
	float peak_current_limited = PEAK_CURRENT * FEB_Get_Peak_Current_Delimiter();
	float power_capped = peak_current_limited * 400.0; // Cap power to 24kW (i.e. our min voltage)
 	// If speed is less than 15, we should command max torque
  	// This catches divide by 0 errors and also negative speeds (which may create very high negative torque values)

	uint16_t minimum_torque = MAX_TORQUE;

	if (BMS_MESSAGE.voltage < LOW_PACK_VOLTAGE) {
		minimum_torque = MAX_TORQUE_LOW_V;
	}

	if (motor_speed < 15) {
		return minimum_torque;
	}

	float maxTorque = min(minimum_torque, (power_capped) / motor_speed);

	return maxTorque;
}



void FEB_RMS_Torque(void){

	// Update Acceleration and Brake
	FEB_ADC_GetAPPSData(&APPS_Data);
	FEB_ADC_GetBrakeData(&Brake_Data);

	// If break is pressed or implausibility
	if (Brake_Data.brake_position > 15.0f || !APPS_Data.plausible || !Brake_Data.plausible || !DRIVE_STATE) {
		APPS_Data.acceleration = 0.0f;
	}

	// Reset plausibility if pedals are released
	if (APPS_Data.acceleration < 5.0f && Brake_Data.brake_position < 15.0f) {
		APPS_Data.plausible = true;
		Brake_Data.plausible = true;
	}

	RMS_CONTROL_MESSAGE.torque = 0.1f *APPS_Data.acceleration * FEB_RMS_GetMaxTorque();

	FEB_CAN_RMS_Transmit_UpdateTorque(RMS_CONTROL_MESSAGE.torque, RMS_CONTROL_MESSAGE.enabled);
}