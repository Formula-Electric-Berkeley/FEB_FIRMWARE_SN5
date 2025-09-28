//logic for scaling values (i.e. voltage divider, resistor)
//in main, create function to call all adc functions and store
//write expressions for getting normalized brake, acceleration, etc.
//normalized code in old kavin code
//brake plausibility safety features etc.
// PCU sometimes gives 0 to 5 V
// output voltage, voltage on pin, counter value

#include "stm32f4xx_hal.h"

// ADC reference voltage
#define ADC_REF_VOLTAGE    3.3f
#define ADC_RESOLUTION     (1<<12 - 1)  // 4096 for 12-bit ADC

// --- ADC1 Channels ---
#define BRAKE_INPUT_ADC_CHANNEL         ADC_CHANNEL_14  // PC4
#define BRAKE_PRESSURE_1_ADC_CHANNEL    ADC_CHANNEL_1   // PA1
#define BRAKE_PRESSURE_2_ADC_CHANNEL    ADC_CHANNEL_0   // PA0

// --- ADC2 Channels ---
#define CURRENT_SENSE_ADC_CHANNEL           ADC_CHANNEL_4   // PA4
#define PRE_TIMING_TRIP_SENSE_ADC_CHANNEL  ADC_CHANNEL_7   // PA7
#define SHUTDOWN_IN_ADC_CHANNEL            ADC_CHANNEL_6   // PA6

// --- ADC3 Channels ---
#define BSPD_INDICATOR_ADC_CHANNEL     ADC_CHANNEL_8   // PC0
#define BSPD_RESET_ADC_CHANNEL         ADC_CHANNEL_9   // PC1
#define ACC_PEDAL_1_ADC_CHANNEL        ADC_CHANNEL_11  // PC3
#define ACC_PEDAL_2_ADC_CHANNEL        ADC_CHANNEL_10  // PC2

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

// Generic function to get ADC value from a specific ADC peripheral and channel
uint16_t getADCValue(ADC_HandleTypeDef* hadc, uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

    HAL_ADC_ConfigChannel(hadc, &sConfig);

    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY);
    uint16_t value = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    return value;
}
float adcToVoltage(uint16_t adcValue) {
    return ((float)adcValue * ADC_REF_VOLTAGE) / ADC_RESOLUTION;
}

// ----------- GETTTING RAW ADC VALUES ----------- 
// --- ADC1 Raw Value Functions ---
uint16_t getBrakeInputRaw(void) {
    return getADCValue(&hadc1, BRAKE_INPUT_ADC_CHANNEL);
}
uint16_t getBrakePressure1Raw(void) {
    return getADCValue(&hadc1, BRAKE_PRESSURE_1_ADC_CHANNEL);
}
uint16_t getBrakePressure2Raw(void) {
    return getADCValue(&hadc1, BRAKE_PRESSURE_2_ADC_CHANNEL);
}

// --- ADC2 Raw Value Functions ---
uint16_t getCurrentSenseRaw(void) {
    return getADCValue(&hadc2, CURRENT_SENSE_ADC_CHANNEL);
}
uint16_t getPreTimingTripSenseRaw(void) {
    return getADCValue(&hadc2, PRE_TIMING_TRIP_SENSE_ADC_CHANNEL);
}
uint16_t getShutdownInRaw(void) {
    return getADCValue(&hadc2, SHUTDOWN_IN_ADC_CHANNEL);
}

// --- ADC3 Raw Value Functions ---
uint16_t getBSPDIndicatorRaw(void) {
    return getADCValue(&hadc3, BSPD_INDICATOR_ADC_CHANNEL);
}
uint16_t getBSPDResetRaw(void) {
    return getADCValue(&hadc3, BSPD_RESET_ADC_CHANNEL);
}
uint16_t getACCPedal1Raw(void) {
    return getADCValue(&hadc3, ACC_PEDAL_1_ADC_CHANNEL);
}
uint16_t getACCPedal2Raw(void) {
    return getADCValue(&hadc3, ACC_PEDAL_2_ADC_CHANNEL);
}

// ----------- GETTTING VOLTAGE VALUES -----------
// --- ADC1 Voltage Functions ---
float getBrakeInputVoltage(void) {
    return adcToVoltage(getBrakeInputRaw());
}
float getBrakePressure1Voltage(void) {
    return adcToVoltage(getBrakePressure1Raw());
}
float getBrakePressure2Voltage(void) {
    return adcToVoltage(getBrakePressure2Raw());
}

// --- ADC2 Voltage Functions ---
float getCurrentSenseVoltage(void) {
    return adcToVoltage(getCurrentSenseRaw());
}
float getPreTimingTripSenseVoltage(void) {
    return adcToVoltage(getPreTimingTripSenseRaw());
}
float getShutdownInVoltage(void) {
    return adcToVoltage(getShutdownInRaw());
}

// --- ADC3 Voltage Functions ---
float getBSPDIndicatorVoltage(void) {
    return adcToVoltage(getBSPDIndicatorRaw());
}
float getBSPDResetVoltage(void) {
    return adcToVoltage(getBSPDResetRaw());
}
float getACCPedal1Voltage(void) {
    return adcToVoltage(getACCPedal1Raw());
}
float getACCPedal2Voltage(void) {
    return adcToVoltage(getACCPedal2Raw());
}
