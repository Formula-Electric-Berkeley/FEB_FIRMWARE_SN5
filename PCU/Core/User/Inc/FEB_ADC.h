/**
  ******************************************************************************
  * @file           : FEB_ADC.h
  * @brief          : Advanced ADC abstraction library for Formula Electric PCU
  ******************************************************************************
  * @attention
  *
  * This library provides comprehensive ADC functionality including:
  * - Multi-channel ADC reading with DMA support
  * - Sensor calibration and normalization
  * - Safety checks for FSAE compliance
  * - Filtering and averaging capabilities
  * - Error detection and diagnostics
  *
  ******************************************************************************
  */

#ifndef __FEB_ADC_H
#define __FEB_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "FEB_PINOUT.h"
#include <stdbool.h>
#include <stdint.h>

/* ========================================================================== */
/*                           TYPE DEFINITIONS                                 */
/* ========================================================================== */

/**
 * @brief ADC Channel Status
 */
typedef enum {
    ADC_STATUS_OK = 0,              /* ADC reading successful */
    ADC_STATUS_ERROR,                /* General ADC error */
    ADC_STATUS_TIMEOUT,              /* ADC conversion timeout */
    ADC_STATUS_OUT_OF_RANGE,         /* Reading outside valid range */
    ADC_STATUS_SHORT_CIRCUIT,        /* Potential short circuit detected */
    ADC_STATUS_OPEN_CIRCUIT,         /* Potential open circuit detected */
    ADC_STATUS_NOT_INITIALIZED       /* ADC not initialized */
} ADC_StatusTypeDef;

/**
 * @brief ADC Operating Mode - DMA is always used for reliability
 */
typedef enum {
    ADC_MODE_DMA = 0,                /* DMA-based continuous mode (default and only mode) */
} ADC_ModeTypeDef;

/**
 * @brief Sensor Calibration Data
 */
typedef struct {
    float offset;                    /* Zero offset in mV */
    float gain;                      /* Gain correction factor */
    float min_voltage;               /* Minimum valid voltage in mV */
    float max_voltage;               /* Maximum valid voltage in mV */
    float min_physical;              /* Minimum physical value */
    float max_physical;              /* Maximum physical value */
    bool  inverted;                  /* True if sensor output is inverted */
} ADC_CalibrationTypeDef;

/**
 * @brief ADC Filter Configuration
 */
typedef struct {
    uint8_t  enabled;                /* Filter enable flag */
    uint8_t  samples;                /* Number of samples to average */
    float    alpha;                  /* Low-pass filter coefficient (0-1) */
    uint16_t buffer[ADC_DMA_BUFFER_SIZE]; /* Sample buffer */
    uint8_t  buffer_index;           /* Current buffer index */
} ADC_FilterTypeDef;

/**
 * @brief ADC Channel Configuration
 */
typedef struct {
    ADC_HandleTypeDef*      hadc;           /* ADC peripheral handle */
    uint32_t                channel;        /* ADC channel number */
    ADC_CalibrationTypeDef  calibration;    /* Calibration parameters */
    ADC_FilterTypeDef       filter;         /* Filter configuration */
    uint16_t                last_raw;       /* Last raw ADC value */
    float                   last_voltage;   /* Last voltage reading */
    float                   last_physical;  /* Last physical value */
    ADC_StatusTypeDef       status;         /* Channel status */
} ADC_ChannelConfigTypeDef;

/**
 * @brief APPS (Accelerator Pedal Position Sensor) Data
 */
typedef struct {
    float    position1;              /* APPS1 position (0-100%) */
    float    position2;              /* APPS2 position (0-100%) */
    float    average;                /* Average position */
    bool     plausible;              /* Plausibility check status */
    uint32_t implausibility_time;    /* Time of implausibility detection */
    bool     short_circuit;          /* Short circuit detected */
    bool     open_circuit;           /* Open circuit detected */
} APPS_DataTypeDef;

/**
 * @brief Brake System Data
 */
typedef struct {
    float    pressure1_bar;          /* Brake pressure sensor 1 (bar) */
    float    pressure2_bar;          /* Brake pressure sensor 2 (bar) */
    float    brake_position;         /* Brake pedal position (0-100%) */
    bool     brake_pressed;          /* Brake switch/input status */
    bool     plausible;              /* Plausibility check status */
    bool     bots_active;            /* Brake Over-Travel Switch status */
} Brake_DataTypeDef;

/**
 * @brief BSPD (Brake System Plausibility Device) Data
 */
typedef struct {
    bool     indicator;              /* BSPD indicator status */
    bool     reset_requested;        /* BSPD reset button status */
    bool     fault;                  /* BSPD fault status */
    uint32_t fault_time;             /* Time of fault detection */
} BSPD_DataTypeDef;

/**
 * @brief Power System Data
 */
typedef struct {
    float    current_amps;           /* Measured current in amps */
    float    shutdown_voltage;       /* Shutdown circuit voltage */
    bool     pre_timing_trip;        /* Pre-timing trip status */
} Power_DataTypeDef;

/* ========================================================================== */
/*                      INITIALIZATION FUNCTIONS                              */
/* ========================================================================== */

/**
 * @brief  Initialize the ADC abstraction library
 * @retval ADC_StatusTypeDef: Initialization status
 */
ADC_StatusTypeDef FEB_ADC_Init(void);

/**
 * @brief  Start ADC conversions with DMA
 * @param  mode: Operating mode (currently only DMA is supported)
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_Start(ADC_ModeTypeDef mode);

/**
 * @brief  Stop ADC conversions
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_Stop(void);

/**
 * @brief  Initialize DMA for continuous ADC conversions
 * @param  hadc: ADC handle
 * @retval ADC_StatusTypeDef: Initialization status
 */
ADC_StatusTypeDef FEB_ADC_InitDMA(ADC_HandleTypeDef* hadc);

/* ========================================================================== */
/*                      RAW ADC READING FUNCTIONS                            */
/* ========================================================================== */

/**
 * @brief  Get raw ADC value from specific channel
 * @param  hadc: ADC handle
 * @param  channel: ADC channel
 * @retval uint16_t: Raw ADC value (0-4095)
 */
uint16_t FEB_ADC_GetRawValue(ADC_HandleTypeDef* hadc, uint32_t channel);

/**
 * @brief  Get filtered ADC value with averaging
 * @param  hadc: ADC handle
 * @param  channel: ADC channel
 * @param  samples: Number of samples to average
 * @retval uint16_t: Averaged ADC value
 */
uint16_t FEB_ADC_GetFilteredValue(ADC_HandleTypeDef* hadc, uint32_t channel, uint8_t samples);

/**
 * @brief  Convert raw ADC value to voltage
 * @param  raw_value: Raw ADC value (0-4095)
 * @retval float: Voltage in volts
 */
float FEB_ADC_RawToVoltage(uint16_t raw_value);

/**
 * @brief  Convert raw ADC value to millivolts
 * @param  raw_value: Raw ADC value (0-4095)
 * @retval uint32_t: Voltage in millivolts
 */
uint32_t FEB_ADC_RawToMillivolts(uint16_t raw_value);

/* ========================================================================== */
/*                      SENSOR-SPECIFIC RAW FUNCTIONS                        */
/* ========================================================================== */

/* Brake System Raw Values */
uint16_t FEB_ADC_GetBrakeInputRaw(void);
uint16_t FEB_ADC_GetBrakePressure1Raw(void);
uint16_t FEB_ADC_GetBrakePressure2Raw(void);

/* Accelerator Pedal Raw Values */
uint16_t FEB_ADC_GetAccelPedal1Raw(void);
uint16_t FEB_ADC_GetAccelPedal2Raw(void);

/* Power System Raw Values */
uint16_t FEB_ADC_GetCurrentSenseRaw(void);
uint16_t FEB_ADC_GetShutdownInRaw(void);
uint16_t FEB_ADC_GetPreTimingTripRaw(void);

/* BSPD Raw Values */
uint16_t FEB_ADC_GetBSPDIndicatorRaw(void);
uint16_t FEB_ADC_GetBSPDResetRaw(void);

/* ========================================================================== */
/*                     SENSOR-SPECIFIC VOLTAGE FUNCTIONS                     */
/* ========================================================================== */

/* Brake System Voltages */
float FEB_ADC_GetBrakeInputVoltage(void);
float FEB_ADC_GetBrakePressure1Voltage(void);
float FEB_ADC_GetBrakePressure2Voltage(void);

/* Accelerator Pedal Voltages */
float FEB_ADC_GetAccelPedal1Voltage(void);
float FEB_ADC_GetAccelPedal2Voltage(void);

/* Power System Voltages */
float FEB_ADC_GetCurrentSenseVoltage(void);
float FEB_ADC_GetShutdownInVoltage(void);
float FEB_ADC_GetPreTimingTripVoltage(void);

/* BSPD Voltages */
float FEB_ADC_GetBSPDIndicatorVoltage(void);
float FEB_ADC_GetBSPDResetVoltage(void);

/* ========================================================================== */
/*                     NORMALIZED/PHYSICAL VALUE FUNCTIONS                   */
/* ========================================================================== */

/**
 * @brief  Get normalized accelerator pedal position
 * @param  apps_data: Pointer to APPS data structure
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_GetAPPSData(APPS_DataTypeDef* apps_data);

/**
 * @brief  Get brake system data
 * @param  brake_data: Pointer to brake data structure
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_GetBrakeData(Brake_DataTypeDef* brake_data);

/**
 * @brief  Get BSPD status
 * @param  bspd_data: Pointer to BSPD data structure
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_GetBSPDData(BSPD_DataTypeDef* bspd_data);

/**
 * @brief  Get power system measurements
 * @param  power_data: Pointer to power data structure
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_GetPowerData(Power_DataTypeDef* power_data);

/**
 * @brief  Get brake pressure in bar
 * @param  sensor_num: Sensor number (1 or 2)
 * @retval float: Pressure in bar
 */
float FEB_ADC_GetBrakePressureBar(uint8_t sensor_num);

/**
 * @brief  Get current measurement in amps
 * @retval float: Current in amps (positive or negative)
 */
float FEB_ADC_GetCurrentAmps(void);

/**
 * @brief  Get shutdown circuit voltage
 * @retval float: Actual shutdown circuit voltage after divider compensation
 */
float FEB_ADC_GetShutdownVoltage(void);

/* ========================================================================== */
/*                        CALIBRATION FUNCTIONS                              */
/* ========================================================================== */

/**
 * @brief  Calibrate accelerator pedal sensors
 * @param  record_min: True to record current position as minimum
 * @param  record_max: True to record current position as maximum
 * @retval ADC_StatusTypeDef: Calibration status
 */
ADC_StatusTypeDef FEB_ADC_CalibrateAPPS(bool record_min, bool record_max);

/**
 * @brief  Set custom voltage range for APPS calibration
 * @param  sensor_num: Sensor number (1 or 2)
 * @param  min_mv: Minimum voltage in millivolts
 * @param  max_mv: Maximum voltage in millivolts
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_SetAPPSVoltageRange(uint8_t sensor_num, float min_mv, float max_mv);

/**
 * @brief  Get current APPS calibration values
 * @param  sensor_num: Sensor number (1 or 2)
 * @param  min_mv: Pointer to store minimum voltage
 * @param  max_mv: Pointer to store maximum voltage
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_GetAPPSCalibration(uint8_t sensor_num, float* min_mv, float* max_mv);

/**
 * @brief  Calibrate brake pressure sensors
 * @param  sensor_num: Sensor number (1 or 2)
 * @param  zero_pressure: True to set zero pressure calibration
 * @retval ADC_StatusTypeDef: Calibration status
 */
ADC_StatusTypeDef FEB_ADC_CalibrateBrakePressure(uint8_t sensor_num, bool zero_pressure);

/**
 * @brief  Set brake pressure sensor calibration
 * @param  sensor_num: Sensor number (1 or 2)
 * @param  zero_mv: Zero pressure voltage in millivolts
 * @param  max_mv: Max pressure voltage in millivolts
 * @param  max_bar: Maximum pressure in bar
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_SetBrakePressureCalibration(uint8_t sensor_num, 
                                                      float zero_mv, 
                                                      float max_mv, 
                                                      float max_bar);

/**
 * @brief  Reset all calibrations to factory defaults
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_ResetCalibrationToDefaults(void);

/**
 * @brief  Set custom calibration for a channel
 * @param  config: Channel configuration
 * @param  calibration: Calibration parameters
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_SetCalibration(ADC_ChannelConfigTypeDef* config, 
                                         ADC_CalibrationTypeDef* calibration);

/**
 * @brief  Load calibration from non-volatile storage
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_LoadCalibration(void);

/**
 * @brief  Save calibration to non-volatile storage
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_SaveCalibration(void);

/* ========================================================================== */
/*                     SAFETY AND PLAUSIBILITY CHECKS                        */
/* ========================================================================== */

/**
 * @brief  Check APPS plausibility according to FSAE rules
 * @retval bool: True if APPS is plausible
 */
bool FEB_ADC_CheckAPPSPlausibility(void);

/**
 * @brief  Check brake plausibility (APPS vs brake)
 * @retval bool: True if brake/throttle combination is plausible
 */
bool FEB_ADC_CheckBrakePlausibility(void);

/**
 * @brief  Check for Brake Over-Travel condition
 * @retval bool: True if BOTS is activated
 */
bool FEB_ADC_CheckBOTS(void);

/**
 * @brief  Perform comprehensive safety checks
 * @retval uint32_t: Bitmask of active faults
 */
uint32_t FEB_ADC_PerformSafetyChecks(void);

/**
 * @brief  Clear safety fault latches
 * @param  fault_mask: Bitmask of faults to clear
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_ClearFaults(uint32_t fault_mask);

/* ========================================================================== */
/*                       FILTER AND PROCESSING FUNCTIONS                     */
/* ========================================================================== */

/**
 * @brief  Configure filter for specific channel
 * @param  config: Channel configuration
 * @param  enable: Enable/disable filter
 * @param  samples: Number of samples for averaging
 * @param  alpha: Low-pass filter coefficient
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_ConfigureFilter(ADC_ChannelConfigTypeDef* config,
                                          bool enable, uint8_t samples, float alpha);

/**
 * @brief  Apply median filter to remove outliers
 * @param  values: Array of values
 * @param  count: Number of values
 * @retval float: Median filtered value
 */
float FEB_ADC_MedianFilter(float* values, uint8_t count);

/**
 * @brief  Apply low-pass filter
 * @param  new_value: New input value
 * @param  old_value: Previous filtered value
 * @param  alpha: Filter coefficient (0-1)
 * @retval float: Filtered value
 */
float FEB_ADC_LowPassFilter(float new_value, float old_value, float alpha);

/* ========================================================================== */
/*                      DIAGNOSTIC AND DEBUG FUNCTIONS                       */
/* ========================================================================== */

/**
 * @brief  Get diagnostic information for all ADC channels
 * @param  buffer: String buffer for diagnostic output
 * @param  size: Buffer size
 * @retval ADC_StatusTypeDef: Operation status
 */
ADC_StatusTypeDef FEB_ADC_GetDiagnostics(char* buffer, size_t size);

/**
 * @brief  Perform ADC self-test
 * @retval ADC_StatusTypeDef: Test result
 */
ADC_StatusTypeDef FEB_ADC_SelfTest(void);

/**
 * @brief  Check if ADC channel is within valid range
 * @param  hadc: ADC handle
 * @param  channel: ADC channel
 * @retval bool: True if channel reading is valid
 */
bool FEB_ADC_IsChannelValid(ADC_HandleTypeDef* hadc, uint32_t channel);

/**
 * @brief  Get last error code
 * @retval uint32_t: Error code
 */
uint32_t FEB_ADC_GetLastError(void);

/**
 * @brief  Reset error counters and status
 * @retval None
 */
void FEB_ADC_ResetErrors(void);

/* ========================================================================== */
/*                         INTERRUPT CALLBACKS                               */
/* ========================================================================== */

/**
 * @brief  ADC conversion complete callback
 * @param  hadc: ADC handle
 * @retval None
 */
void FEB_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);

/**
 * @brief  ADC conversion half complete callback (for DMA)
 * @param  hadc: ADC handle
 * @retval None
 */
void FEB_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc);

/**
 * @brief  ADC error callback
 * @param  hadc: ADC handle
 * @retval None
 */
void FEB_ADC_ErrorCallback(ADC_HandleTypeDef* hadc);

/**
 * @brief  ADC watchdog callback (out of range detection)
 * @param  hadc: ADC handle
 * @retval None
 */
void FEB_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc);

/* ========================================================================== */
/*                           UTILITY FUNCTIONS                               */
/* ========================================================================== */

/**
 * @brief  Map value from one range to another
 * @param  value: Input value
 * @param  in_min: Input minimum
 * @param  in_max: Input maximum
 * @param  out_min: Output minimum
 * @param  out_max: Output maximum
 * @retval float: Mapped value
 */
float FEB_ADC_MapRange(float value, float in_min, float in_max, 
                      float out_min, float out_max);

/**
 * @brief  Constrain value to range
 * @param  value: Input value
 * @param  min: Minimum value
 * @param  max: Maximum value
 * @retval float: Constrained value
 */
float FEB_ADC_Constrain(float value, float min, float max);

/**
 * @brief  Apply deadzone to value
 * @param  value: Input value (0-100%)
 * @param  deadzone: Deadzone percentage
 * @retval float: Value with deadzone applied
 */
float FEB_ADC_ApplyDeadzone(float value, float deadzone);

#ifdef __cplusplus
}
#endif

#endif /* __FEB_ADC_H */