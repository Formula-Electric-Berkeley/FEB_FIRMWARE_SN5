/**
 ******************************************************************************
 * @file           : FEB_ADC.c
 * @brief          : Advanced ADC abstraction library implementation
 ******************************************************************************
 * @attention
 *
 * Implementation of the Formula Electric PCU ADC abstraction library
 * with advanced features for sensor reading, calibration, and safety checks.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "FEB_ADC.h"
#include "FEB_Debug.h"
extern UART_HandleTypeDef huart2;

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  uint32_t apps_implausibility_timer;
  uint32_t brake_plausibility_timer;
  uint32_t bots_timer;
  uint32_t last_error_code;
  uint32_t error_count;
  bool initialized;
} ADC_RuntimeDataTypeDef;

/* Private define ------------------------------------------------------------*/
#define FAULT_APPS_IMPLAUSIBILITY (1 << 0)
#define FAULT_BRAKE_PLAUSIBILITY (1 << 1)
#define FAULT_BOTS_ACTIVE (1 << 2)
#define FAULT_APPS_SHORT_CIRCUIT (1 << 3)
#define FAULT_APPS_OPEN_CIRCUIT (1 << 4)
#define FAULT_BRAKE_SENSOR_FAULT (1 << 5)
#define FAULT_CURRENT_SENSOR_FAULT (1 << 6)
#define FAULT_ADC_TIMEOUT (1 << 7)

/* Private variables ---------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

static ADC_RuntimeDataTypeDef adc_runtime = {0};
static uint32_t active_faults = 0;

/* DMA Buffers for continuous conversion - must match number of channels */
static uint16_t adc1_dma_buffer[3 * ADC_DMA_BUFFER_SIZE]; /* 3 channels: PA0, PA1, PC4 */
static uint16_t adc2_dma_buffer[3 * ADC_DMA_BUFFER_SIZE]; /* 3 channels: PA4, PA6, PA7 */
static uint16_t adc3_dma_buffer[4 * ADC_DMA_BUFFER_SIZE]; /* 4 channels: PC0, PC1, PC2, PC3 */

/* Channel indices in DMA buffers */
#define ADC1_CH0_BRAKE_PRESSURE1_IDX 0 /* PA0 - Channel 0 - Brake Pressure 1 */
#define ADC1_CH1_BRAKE_PRESSURE2_IDX 1 /* PA1 - Channel 1 - Brake Pressure 2 */
#define ADC1_CH14_BRAKE_INPUT_IDX 2    /* PC4 - Channel 14 */

#define ADC2_CH4_CURRENT_SENSE_IDX 0 /* PA4 - Channel 4 */
#define ADC2_CH6_SHUTDOWN_IN_IDX 1   /* PA6 - Channel 6 */
#define ADC2_CH7_PRE_TIMING_IDX 2    /* PA7 - Channel 7 */

#define ADC3_CH8_BSPD_INDICATOR_IDX 0 /* PC0 - Channel 8 - BSPD Indicator */
#define ADC3_CH9_BSPD_RESET_IDX 1     /* PC1 - Channel 9 - BSPD Reset */
#define ADC3_CH12_ACCEL_PEDAL1_IDX 2  /* PC2 - Channel 12 - APPS1 */
#define ADC3_CH13_ACCEL_PEDAL2_IDX 3  /* PC3 - Channel 13 - APPS2 */

/* Channel configurations */
static ADC_ChannelConfigTypeDef brake_input_config;
static ADC_ChannelConfigTypeDef brake_pressure1_config;
static ADC_ChannelConfigTypeDef brake_pressure2_config;
static ADC_ChannelConfigTypeDef accel_pedal1_config;
static ADC_ChannelConfigTypeDef accel_pedal2_config;
static ADC_ChannelConfigTypeDef current_sense_config;
static ADC_ChannelConfigTypeDef shutdown_in_config;

/* Calibration data storage - runtime configurable with macro defaults */
static ADC_CalibrationTypeDef apps1_calibration = {
    .offset = 0.0f,
    .gain = 1.0f,
    .min_voltage = APPS1_DEFAULT_MIN_VOLTAGE_MV, /* Voltage at 0% throttle */
    .max_voltage = APPS1_DEFAULT_MAX_VOLTAGE_MV, /* Voltage at 100% throttle */
    .min_physical = APPS_MIN_PHYSICAL_PERCENT,   /* Physical: 0% */
    .max_physical = APPS_MAX_PHYSICAL_PERCENT,   /* Physical: 100% */
    .inverted = false};

static ADC_CalibrationTypeDef apps2_calibration = {
    .offset = 0.0f,
    .gain = 1.0f,
    .min_voltage = APPS2_DEFAULT_MIN_VOLTAGE_MV, /* Voltage at 0% throttle */
    .max_voltage = APPS2_DEFAULT_MAX_VOLTAGE_MV, /* Voltage at 100% throttle */
    .min_physical = APPS_MIN_PHYSICAL_PERCENT,   /* Physical: 0% */
    .max_physical = APPS_MAX_PHYSICAL_PERCENT,   /* Physical: 100% */
    .inverted = false};

static ADC_CalibrationTypeDef brake_pressure1_calibration = {
    .offset = 0.0f,
    .gain = 1.0f,
    .min_voltage = BRAKE_PRESSURE_DEFAULT_MIN_MV,    /* Voltage at 0 bar */
    .max_voltage = BRAKE_PRESSURE_DEFAULT_MAX_MV,    /* Voltage at max pressure */
    .min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR, /* Physical: 0 bar */
    .max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR, /* Physical: 200 bar */
    .inverted = false};

static ADC_CalibrationTypeDef brake_pressure2_calibration = {
    .offset = 0.0f,
    .gain = 1.0f,
    .min_voltage = BRAKE_PRESSURE_DEFAULT_MIN_MV,    /* Voltage at 0 bar */
    .max_voltage = BRAKE_PRESSURE_DEFAULT_MAX_MV,    /* Voltage at max pressure */
    .min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR, /* Physical: 0 bar */
    .max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR, /* Physical: 200 bar */
    .inverted = false};

#define VOLTAGE_DIVIDER_RATIO (5.0f / 3.3f)
#define VOLTAGE_DIVIDER_RATIO_ACCEL1 2.0f

/* Private function prototypes -----------------------------------------------*/
static uint16_t GetAveragedADCValue(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples);
static void UpdateFaultTimer(uint32_t *timer, bool fault_condition, uint32_t threshold);

/* Initialization Functions --------------------------------------------------*/

ADC_StatusTypeDef FEB_ADC_Init(void)
{
  if (adc_runtime.initialized)
  {
    return ADC_STATUS_OK;
  }

  /* Initialize channel configurations using settings from pinout.h */
  /* This allows easy tuning of filter parameters without code changes */

  /* Brake Input Configuration */
  brake_input_config.hadc = &hadc1;
  brake_input_config.channel = ADC1_BRAKE_INPUT_CHANNEL;
  brake_input_config.filter.enabled = FILTER_BRAKE_INPUT_ENABLED;
  brake_input_config.filter.samples = FILTER_BRAKE_INPUT_SAMPLES;
  brake_input_config.filter.alpha = FILTER_BRAKE_INPUT_ALPHA;

  /* Accelerator Pedal Sensor 1 Configuration */
  accel_pedal1_config.hadc = &hadc3;
  accel_pedal1_config.channel = ADC3_ACCEL_PEDAL_1_CHANNEL;
  accel_pedal1_config.filter.enabled = FILTER_ACCEL_PEDAL_ENABLED;
  accel_pedal1_config.filter.samples = FILTER_ACCEL_PEDAL_SAMPLES;
  accel_pedal1_config.filter.alpha = FILTER_ACCEL_PEDAL_ALPHA;

  /* Accelerator Pedal Sensor 2 Configuration */
  accel_pedal2_config.hadc = &hadc3;
  accel_pedal2_config.channel = ADC3_ACCEL_PEDAL_2_CHANNEL;
  accel_pedal2_config.filter.enabled = FILTER_ACCEL_PEDAL_ENABLED;
  accel_pedal2_config.filter.samples = FILTER_ACCEL_PEDAL_SAMPLES;
  accel_pedal2_config.filter.alpha = FILTER_ACCEL_PEDAL_ALPHA;

  /* Brake Pressure Sensor 1 Configuration */
  brake_pressure1_config.hadc = &hadc1;
  brake_pressure1_config.channel = ADC1_BRAKE_PRESSURE_1_CHANNEL;
  brake_pressure1_config.filter.enabled = FILTER_BRAKE_PRESSURE_ENABLED;
  brake_pressure1_config.filter.samples = FILTER_BRAKE_PRESSURE_SAMPLES;
  brake_pressure1_config.filter.alpha = FILTER_BRAKE_PRESSURE_ALPHA;

  /* Brake Pressure Sensor 2 Configuration */
  brake_pressure2_config.hadc = &hadc1;
  brake_pressure2_config.channel = ADC1_BRAKE_PRESSURE_2_CHANNEL;
  brake_pressure2_config.filter.enabled = FILTER_BRAKE_PRESSURE_ENABLED;
  brake_pressure2_config.filter.samples = FILTER_BRAKE_PRESSURE_SAMPLES;
  brake_pressure2_config.filter.alpha = FILTER_BRAKE_PRESSURE_ALPHA;

  /* Current Sensor Configuration */
  current_sense_config.hadc = &hadc2;
  current_sense_config.channel = ADC2_CURRENT_SENSE_CHANNEL;
  current_sense_config.filter.enabled = FILTER_CURRENT_SENSE_ENABLED;
  current_sense_config.filter.samples = FILTER_CURRENT_SENSE_SAMPLES;
  current_sense_config.filter.alpha = FILTER_CURRENT_SENSE_ALPHA;

  /* Shutdown Circuit Monitoring Configuration */
  shutdown_in_config.hadc = &hadc2;
  shutdown_in_config.channel = ADC2_SHUTDOWN_IN_CHANNEL;
  shutdown_in_config.filter.enabled = FILTER_SHUTDOWN_ENABLED;
  shutdown_in_config.filter.samples = FILTER_SHUTDOWN_SAMPLES;
  shutdown_in_config.filter.alpha = FILTER_SHUTDOWN_ALPHA;

  /* Clear DMA buffers */
  memset(adc1_dma_buffer, 0, sizeof(adc1_dma_buffer));
  memset(adc2_dma_buffer, 0, sizeof(adc2_dma_buffer));
  memset(adc3_dma_buffer, 0, sizeof(adc3_dma_buffer));

  /* Reset runtime data */
  memset(&adc_runtime, 0, sizeof(adc_runtime));
  adc_runtime.initialized = true;

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_Start(ADC_ModeTypeDef mode)
{
  if (!adc_runtime.initialized)
  {
    return ADC_STATUS_NOT_INITIALIZED;
  }

  HAL_StatusTypeDef hal_status = HAL_OK;

  /* Always use DMA mode for reliability and performance */
  /* The .ioc file has been configured for DMA operation */

  /* Start DMA-based continuous conversion with proper buffer sizes */
  hal_status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_dma_buffer, 3 * ADC_DMA_BUFFER_SIZE);
  if (hal_status != HAL_OK)
    return ADC_STATUS_ERROR;

  hal_status = HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buffer, 3 * ADC_DMA_BUFFER_SIZE);
  if (hal_status != HAL_OK)
  {
    HAL_ADC_Stop_DMA(&hadc1);
    return ADC_STATUS_ERROR;
  }

  hal_status = HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3_dma_buffer, 4 * ADC_DMA_BUFFER_SIZE);
  if (hal_status != HAL_OK)
  {
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Stop_DMA(&hadc2);
    return ADC_STATUS_ERROR;
  }

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_Stop(void)
{
  HAL_ADC_Stop(&hadc1);
  HAL_ADC_Stop(&hadc2);
  HAL_ADC_Stop(&hadc3);

  HAL_ADC_Stop_DMA(&hadc1);
  HAL_ADC_Stop_DMA(&hadc2);
  HAL_ADC_Stop_DMA(&hadc3);

  return ADC_STATUS_OK;
}

/* Raw ADC Reading Functions -------------------------------------------------*/

uint16_t FEB_ADC_GetRawValue(ADC_HandleTypeDef *hadc, uint32_t channel)
{
  /* This function now reads from DMA buffer instead of polling */
  /* DMA continuously updates the buffers in the background */

  uint16_t *buffer_ptr = NULL;
  uint32_t channel_idx = 0;

  /* Map hardware channel number to DMA buffer index */
  if (hadc == &hadc1)
  {
    buffer_ptr = adc1_dma_buffer;
    if (channel == ADC_CHANNEL_0)
      channel_idx = ADC1_CH0_BRAKE_PRESSURE1_IDX;
    else if (channel == ADC_CHANNEL_1)
      channel_idx = ADC1_CH1_BRAKE_PRESSURE2_IDX;
    else if (channel == ADC_CHANNEL_14)
      channel_idx = ADC1_CH14_BRAKE_INPUT_IDX;
    else
      return 0;
  }
  else if (hadc == &hadc2)
  {
    buffer_ptr = adc2_dma_buffer;
    if (channel == ADC_CHANNEL_4)
      channel_idx = ADC2_CH4_CURRENT_SENSE_IDX;
    else if (channel == ADC_CHANNEL_6)
      channel_idx = ADC2_CH6_SHUTDOWN_IN_IDX;
    else if (channel == ADC_CHANNEL_7)
      channel_idx = ADC2_CH7_PRE_TIMING_IDX;
    else
      return 0;
  }
  else if (hadc == &hadc3)
  {
    buffer_ptr = adc3_dma_buffer;
    if (channel == ADC_CHANNEL_8)
      channel_idx = ADC3_CH8_BSPD_INDICATOR_IDX;
    else if (channel == ADC_CHANNEL_9)
      channel_idx = ADC3_CH9_BSPD_RESET_IDX;
    else if (channel == ADC_CHANNEL_12)
      channel_idx = ADC3_CH12_ACCEL_PEDAL1_IDX;
    else if (channel == ADC_CHANNEL_13)
      channel_idx = ADC3_CH13_ACCEL_PEDAL2_IDX;
    else
      return 0;
  }
  else
  {
    return 0;
  }

  /* Read the latest value from DMA buffer */
  /* The buffer is organized as: [ch0_sample0, ch1_sample0, ch2_sample0, ch0_sample1, ...] */
  /* We read the most recent sample (first in the circular buffer) */
  uint16_t value = buffer_ptr[channel_idx];

  return value;
}

uint16_t FEB_ADC_GetFilteredValue(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples)
{
  if (samples < 1)
    samples = 1;
  if (samples > ADC_DMA_BUFFER_SIZE)
    samples = ADC_DMA_BUFFER_SIZE;

  return GetAveragedADCValue(hadc, channel, samples);
}

float FEB_ADC_RawToVoltage(uint16_t raw_value)
{
  return ((float)raw_value * ADC_VREF_VOLTAGE) / (float)ADC_MAX_VALUE;
}

uint32_t FEB_ADC_RawToMillivolts(uint16_t raw_value)
{
  return (uint32_t)((raw_value * ADC_REFERENCE_VOLTAGE_MV) / ADC_MAX_VALUE);
}

/* Sensor-Specific Raw Functions ---------------------------------------------*/

uint16_t FEB_ADC_GetBrakeInputRaw(void)
{
  return FEB_ADC_GetRawValue(&hadc1, ADC1_BRAKE_INPUT_CHANNEL);
}

uint16_t FEB_ADC_GetAccelPedal1Raw(void)
{
  return FEB_ADC_GetRawValue(&hadc3, ADC3_ACCEL_PEDAL_1_CHANNEL);
}

uint16_t FEB_ADC_GetAccelPedal2Raw(void)
{
  return FEB_ADC_GetRawValue(&hadc3, ADC3_ACCEL_PEDAL_2_CHANNEL);
}

uint16_t FEB_ADC_GetBrakePressure1Raw(void)
{
  return FEB_ADC_GetRawValue(&hadc1, ADC1_BRAKE_PRESSURE_1_CHANNEL);
}

uint16_t FEB_ADC_GetBrakePressure2Raw(void)
{
  return FEB_ADC_GetRawValue(&hadc1, ADC1_BRAKE_PRESSURE_2_CHANNEL);
}

uint16_t FEB_ADC_GetCurrentSenseRaw(void)
{
  return FEB_ADC_GetRawValue(&hadc2, ADC2_CURRENT_SENSE_CHANNEL);
}

uint16_t FEB_ADC_GetShutdownInRaw(void)
{
  return FEB_ADC_GetRawValue(&hadc2, ADC2_SHUTDOWN_IN_CHANNEL);
}

uint16_t FEB_ADC_GetPreTimingTripRaw(void)
{
  return FEB_ADC_GetRawValue(&hadc2, ADC2_PRE_TIMING_TRIP_CHANNEL);
}

uint16_t FEB_ADC_GetBSPDIndicatorRaw(void)
{
  return FEB_ADC_GetRawValue(&hadc3, ADC3_BSPD_INDICATOR_CHANNEL);
}

uint16_t FEB_ADC_GetBSPDResetRaw(void)
{
  return FEB_ADC_GetRawValue(&hadc3, ADC3_BSPD_RESET_CHANNEL);
}

/* Sensor-Specific Voltage Functions -----------------------------------------*/

float FEB_ADC_GetBrakeInputVoltage(void)
{
  uint16_t raw = brake_input_config.filter.enabled
                     ? FEB_ADC_GetFilteredValue(&hadc1, ADC1_BRAKE_INPUT_CHANNEL, brake_input_config.filter.samples)
                     : FEB_ADC_GetBrakeInputRaw();
  return FEB_ADC_RawToVoltage(raw);
}

float FEB_ADC_GetAccelPedal1Voltage(void)
{
  uint16_t raw = accel_pedal1_config.filter.enabled
                     ? FEB_ADC_GetFilteredValue(&hadc3, ADC3_ACCEL_PEDAL_1_CHANNEL, accel_pedal1_config.filter.samples)
                     : FEB_ADC_GetAccelPedal1Raw();
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO_ACCEL1;
}

float FEB_ADC_GetAccelPedal2Voltage(void)
{
  uint16_t raw = accel_pedal2_config.filter.enabled
                     ? FEB_ADC_GetFilteredValue(&hadc3, ADC3_ACCEL_PEDAL_2_CHANNEL, accel_pedal2_config.filter.samples)
                     : FEB_ADC_GetAccelPedal2Raw();
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetBrakePressure1Voltage(void)
{
  uint16_t raw = brake_pressure1_config.filter.enabled ? FEB_ADC_GetFilteredValue(&hadc1, ADC1_BRAKE_PRESSURE_1_CHANNEL,
                                                                                  brake_pressure1_config.filter.samples)
                                                       : FEB_ADC_GetBrakePressure1Raw();
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetBrakePressure2Voltage(void)
{
  uint16_t raw = brake_pressure2_config.filter.enabled ? FEB_ADC_GetFilteredValue(&hadc1, ADC1_BRAKE_PRESSURE_2_CHANNEL,
                                                                                  brake_pressure2_config.filter.samples)
                                                       : FEB_ADC_GetBrakePressure2Raw();
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetCurrentSenseVoltage(void)
{
  uint16_t raw = current_sense_config.filter.enabled
                     ? FEB_ADC_GetFilteredValue(&hadc2, ADC2_CURRENT_SENSE_CHANNEL, current_sense_config.filter.samples)
                     : FEB_ADC_GetCurrentSenseRaw();
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetShutdownInVoltage(void)
{
  uint16_t raw = shutdown_in_config.filter.enabled
                     ? FEB_ADC_GetFilteredValue(&hadc2, ADC2_SHUTDOWN_IN_CHANNEL, shutdown_in_config.filter.samples)
                     : FEB_ADC_GetShutdownInRaw();
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetPreTimingTripVoltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetPreTimingTripRaw()) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetBSPDIndicatorVoltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetBSPDIndicatorRaw());
}

float FEB_ADC_GetBSPDResetVoltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetBSPDResetRaw());
}

/* Normalized/Physical Value Functions ---------------------------------------*/

ADC_StatusTypeDef FEB_ADC_GetAPPSData(APPS_DataTypeDef *apps_data)
{
  if (!apps_data)
    return ADC_STATUS_ERROR;

  /* Get voltage readings */
  float voltage1 = FEB_ADC_GetAccelPedal1Voltage() * 1000.0f; /* Convert to mV */
  float voltage2 = FEB_ADC_GetAccelPedal2Voltage() * 1000.0f;

  /* Check for sensor faults */
  apps_data->short_circuit = (voltage1 < APPS_SHORT_CIRCUIT_DETECT_MV) || (voltage2 < APPS_SHORT_CIRCUIT_DETECT_MV);
  apps_data->open_circuit = (voltage1 > APPS_OPEN_CIRCUIT_DETECT_MV) || (voltage2 > APPS_OPEN_CIRCUIT_DETECT_MV);

  if (apps_data->short_circuit || apps_data->open_circuit)
  {
    apps_data->position1 = 0.0f;
    apps_data->position2 = 0.0f;
    apps_data->acceleration = 0.0f;
    apps_data->plausible = false;
    return ADC_STATUS_OUT_OF_RANGE;
  }

  /* Apply calibration and convert to percentage */
  apps_data->position1 =
      FEB_ADC_MapRange(voltage1, apps1_calibration.min_voltage, apps1_calibration.max_voltage, 0.0f, 100.0f);
  apps_data->position2 =
      FEB_ADC_MapRange(voltage2, apps2_calibration.min_voltage, apps2_calibration.max_voltage, 0.0f, 100.0f);

  /* Constrain to valid range */
  apps_data->position1 = FEB_ADC_Constrain(apps_data->position1, 0.0f, 100.0f);
  apps_data->position2 = FEB_ADC_Constrain(apps_data->position2, 0.0f, 100.0f);

  /* Apply deadzone */
  apps_data->position1 = FEB_ADC_ApplyDeadzone(apps_data->position1, APPS_DEADZONE_PERCENT);
  apps_data->position2 = FEB_ADC_ApplyDeadzone(apps_data->position2, APPS_DEADZONE_PERCENT);

  /* Calculate average */
  apps_data->acceleration = (apps_data->position1 + apps_data->position2) / 2.0f;

  /* Check plausibility */
  float deviation = fabs(apps_data->position1 - apps_data->position2);
  if (deviation >= APPS_PLAUSIBILITY_TOLERANCE)
  {
    apps_data->plausible = false;
  }
  else
  {
    apps_data->plausible = true;
  }

  if (!apps_data->plausible)
  {
    if (apps_data->implausibility_time == 0)
    {
      apps_data->implausibility_time = HAL_GetTick();
    }
  }
  else
  {
    apps_data->implausibility_time = 0;
  }

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_GetBrakeData(Brake_DataTypeDef *brake_data)
{
  if (!brake_data)
    return ADC_STATUS_ERROR;

  /* Get brake pressure readings */
  float pressure1_voltage = FEB_ADC_GetBrakePressure1Voltage() * 1000.0f; /* mV */
  float pressure2_voltage = FEB_ADC_GetBrakePressure2Voltage() * 1000.0f;
  float brake_input_mv = FEB_ADC_GetBrakeInputVoltage() * 1000.0f;

  // Check which sensor is shorted to the brake input
  float pressure1_diff = fabs(pressure1_voltage - brake_input_mv);
  float pressure2_diff = fabs(pressure2_voltage - brake_input_mv);
  if (pressure1_diff < pressure2_diff)
  {
    brake_data->brake_switch = false;
  }
  else
  {
    brake_data->brake_switch = true;
  }

  /* Convert voltage to pressure using runtime calibration */

  /* Convert voltage to pressure percentage (0-100%) */
  brake_data->pressure1_percent = FEB_ADC_MapRange(pressure1_voltage, brake_pressure1_calibration.min_voltage,
                                                   brake_pressure1_calibration.max_voltage, 0.0f, 100.0f);
  brake_data->pressure2_percent = FEB_ADC_MapRange(pressure2_voltage, brake_pressure2_calibration.min_voltage,
                                                   brake_pressure2_calibration.max_voltage, 0.0f, 100.0f);

  brake_data->pressure1_percent = FEB_ADC_Constrain(brake_data->pressure1_percent, 0.0f, 100.0f);
  brake_data->pressure2_percent = FEB_ADC_Constrain(brake_data->pressure2_percent, 0.0f, 100.0f);

  /* Get brake switch status */
  brake_data->brake_pressed = (brake_input_mv > BRAKE_INPUT_THRESHOLD_MV);

  /* Calculate brake position based on pressure */
  // float avg_pressure = (brake_data->pressure1_percent + brake_data->pressure2_percent) / 2.0f;
  brake_data->brake_position = brake_data->brake_switch ? brake_data->pressure2_percent : brake_data->pressure1_percent;

  /* Check plausibility between pressure sensors */
  float pressure_diff = fabs(brake_data->pressure1_percent - brake_data->pressure2_percent);

  if (pressure_diff > (BRAKE_PRESSURE_MAX_PHYSICAL_BAR * 0.2f))
  {
    brake_data->plausible = false;
  } /* 20% tolerance */

  /* Check BOTS */
  brake_data->bots_active = (brake_data->brake_position > BOTS_ACTIVATION_PERCENT);

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_GetBSPDData(BSPD_DataTypeDef *bspd_data)
{
  if (!bspd_data)
    return ADC_STATUS_ERROR;

  float indicator_voltage = FEB_ADC_GetBSPDIndicatorVoltage();
  float reset_voltage = FEB_ADC_GetBSPDResetVoltage();

  /* BSPD signals after voltage divider */
  indicator_voltage *= BSPD_VOLTAGE_DIVIDER_RATIO;
  reset_voltage *= BSPD_VOLTAGE_DIVIDER_RATIO;

  /* Digital interpretation of analog signals */
  bspd_data->indicator = (indicator_voltage > 2.5f);
  bspd_data->reset_requested = (reset_voltage > 2.5f);

  /* Fault detection logic */
  if (bspd_data->indicator && !bspd_data->fault)
  {
    bspd_data->fault = true;
    bspd_data->fault_time = HAL_GetTick();
  }
  else if (!bspd_data->indicator && bspd_data->reset_requested)
  {
    bspd_data->fault = false;
    bspd_data->fault_time = 0;
  }

  return ADC_STATUS_OK;
}

float FEB_ADC_GetBrakePressureBar(uint8_t sensor_num)
{
  float voltage_mv;
  ADC_CalibrationTypeDef *cal;

  if (sensor_num == 1)
  {
    voltage_mv = FEB_ADC_GetBrakePressure1Voltage() * 1000.0f;
    cal = &brake_pressure1_calibration;
  }
  else if (sensor_num == 2)
  {
    voltage_mv = FEB_ADC_GetBrakePressure2Voltage() * 1000.0f;
    cal = &brake_pressure2_calibration;
  }
  else
  {
    return -1.0f; /* Invalid sensor number */
  }

  /* Use runtime calibration values */
  return FEB_ADC_MapRange(voltage_mv, cal->min_voltage, cal->max_voltage, cal->min_physical, cal->max_physical);
}

float FEB_ADC_GetShutdownVoltage(void)
{
  return FEB_ADC_GetShutdownInVoltage() * SHUTDOWN_VOLTAGE_DIVIDER_RATIO;
}

/* Calibration Functions -----------------------------------------------------*/

ADC_StatusTypeDef FEB_ADC_CalibrateAPPS(bool record_min, bool record_max)
{
  if (record_min)
  {
    /* Record current position as minimum (0% throttle) */
    apps1_calibration.min_voltage = FEB_ADC_GetAccelPedal1Voltage() * 1000.0f;
    apps2_calibration.min_voltage = FEB_ADC_GetAccelPedal2Voltage() * 1000.0f;
  }

  if (record_max)
  {
    /* Record current position as maximum (100% throttle) */
    apps1_calibration.max_voltage = FEB_ADC_GetAccelPedal1Voltage() * 1000.0f;
    apps2_calibration.max_voltage = FEB_ADC_GetAccelPedal2Voltage() * 1000.0f;
  }

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_CalibrateBrakePressure(uint8_t sensor_num, bool zero_pressure)
{
  if (sensor_num != 1 && sensor_num != 2)
  {
    return ADC_STATUS_ERROR;
  }

  ADC_CalibrationTypeDef *cal = (sensor_num == 1) ? &brake_pressure1_calibration : &brake_pressure2_calibration;

  if (zero_pressure)
  {
    /* Record current voltage as zero pressure point */
    float voltage_mv =
        (sensor_num == 1) ? FEB_ADC_GetBrakePressure1Voltage() * 1000.0f : FEB_ADC_GetBrakePressure2Voltage() * 1000.0f;

    cal->min_voltage = voltage_mv;
    cal->offset = voltage_mv; /* Store as offset for zero correction */
  }

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_SetAPPSVoltageRange(uint8_t sensor_num, float min_mv, float max_mv)
{
  if (sensor_num != 1 && sensor_num != 2)
  {
    return ADC_STATUS_ERROR;
  }

  ADC_CalibrationTypeDef *cal = (sensor_num == 1) ? &apps1_calibration : &apps2_calibration;

  cal->min_voltage = min_mv;
  cal->max_voltage = max_mv;

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_GetAPPSCalibration(uint8_t sensor_num, float *min_mv, float *max_mv)
{
  if ((sensor_num != 1 && sensor_num != 2) || !min_mv || !max_mv)
  {
    return ADC_STATUS_ERROR;
  }

  ADC_CalibrationTypeDef *cal = (sensor_num == 1) ? &apps1_calibration : &apps2_calibration;

  *min_mv = cal->min_voltage;
  *max_mv = cal->max_voltage;

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_SetBrakePressureCalibration(uint8_t sensor_num, float zero_mv, float max_mv, float max_bar)
{
  if (sensor_num != 1 && sensor_num != 2)
  {
    return ADC_STATUS_ERROR;
  }

  ADC_CalibrationTypeDef *cal = (sensor_num == 1) ? &brake_pressure1_calibration : &brake_pressure2_calibration;

  cal->min_voltage = zero_mv;
  cal->max_voltage = max_mv;
  cal->min_physical = 0.0f;
  cal->max_physical = max_bar;

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_ResetCalibrationToDefaults(void)
{
  /* Reset APPS calibrations to defaults from macros */
  apps1_calibration.min_voltage = APPS1_DEFAULT_MIN_VOLTAGE_MV;
  apps1_calibration.max_voltage = APPS1_DEFAULT_MAX_VOLTAGE_MV;
  apps1_calibration.min_physical = APPS_MIN_PHYSICAL_PERCENT;
  apps1_calibration.max_physical = APPS_MAX_PHYSICAL_PERCENT;

  apps2_calibration.min_voltage = APPS2_DEFAULT_MIN_VOLTAGE_MV;
  apps2_calibration.max_voltage = APPS2_DEFAULT_MAX_VOLTAGE_MV;
  apps2_calibration.min_physical = APPS_MIN_PHYSICAL_PERCENT;
  apps2_calibration.max_physical = APPS_MAX_PHYSICAL_PERCENT;

  /* Reset brake pressure calibrations */
  brake_pressure1_calibration.min_voltage = BRAKE_PRESSURE_DEFAULT_MIN_MV;
  brake_pressure1_calibration.max_voltage = BRAKE_PRESSURE_DEFAULT_MAX_MV;
  brake_pressure1_calibration.min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR;
  brake_pressure1_calibration.max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR;

  brake_pressure2_calibration.min_voltage = BRAKE_PRESSURE_DEFAULT_MIN_MV;
  brake_pressure2_calibration.max_voltage = BRAKE_PRESSURE_DEFAULT_MAX_MV;
  brake_pressure2_calibration.min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR;
  brake_pressure2_calibration.max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR;

  /* Reset offsets and gains */
  apps1_calibration.offset = 0.0f;
  apps1_calibration.gain = 1.0f;
  apps2_calibration.offset = 0.0f;
  apps2_calibration.gain = 1.0f;
  brake_pressure1_calibration.offset = 0.0f;
  brake_pressure1_calibration.gain = 1.0f;
  brake_pressure2_calibration.offset = 0.0f;
  brake_pressure2_calibration.gain = 1.0f;

  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_SetCalibration(ADC_ChannelConfigTypeDef *config, ADC_CalibrationTypeDef *calibration)
{
  if (!config || !calibration)
    return ADC_STATUS_ERROR;

  memcpy(&config->calibration, calibration, sizeof(ADC_CalibrationTypeDef));
  return ADC_STATUS_OK;
}

/* Safety and Plausibility Checks --------------------------------------------*/

bool FEB_ADC_CheckAPPSPlausibility(void)
{
  APPS_DataTypeDef apps_data;

  if (FEB_ADC_GetAPPSData(&apps_data) != ADC_STATUS_OK)
  {
    return false;
  }

  /* Check if implausibility has persisted long enough */
  if (!apps_data.plausible && apps_data.implausibility_time > 0)
  {
    uint32_t elapsed = HAL_GetTick() - apps_data.implausibility_time;
    if (elapsed > APPS_IMPLAUSIBILITY_TIME_MS)
    {
      active_faults |= FAULT_APPS_IMPLAUSIBILITY;
      return false;
    }
  }

  /* Check for sensor faults */
  if (apps_data.short_circuit)
  {
    active_faults |= FAULT_APPS_SHORT_CIRCUIT;
    return false;
  }

  if (apps_data.open_circuit)
  {
    active_faults |= FAULT_APPS_OPEN_CIRCUIT;
    return false;
  }

  return apps_data.plausible;
}

bool FEB_ADC_CheckBrakePlausibility(void)
{
  APPS_DataTypeDef apps_data;
  Brake_DataTypeDef brake_data;

  if (FEB_ADC_GetAPPSData(&apps_data) != ADC_STATUS_OK || FEB_ADC_GetBrakeData(&brake_data) != ADC_STATUS_OK)
  {
    return false;
  }

  /* FSAE rule: If brake is pressed hard and throttle > 25%, cut throttle */
  bool brake_hard = (brake_data.pressure1_percent > BRAKE_PRESSURE_THRESHOLD_PERCENT) ||
                    (brake_data.pressure2_percent > BRAKE_PRESSURE_THRESHOLD_PERCENT);
  bool throttle_high = (apps_data.acceleration > 25.0f);

  if (brake_hard && throttle_high)
  {
    UpdateFaultTimer(&adc_runtime.brake_plausibility_timer, true, BRAKE_PLAUSIBILITY_TIME_MS);

    if (adc_runtime.brake_plausibility_timer > BRAKE_PLAUSIBILITY_TIME_MS)
    {
      active_faults |= FAULT_BRAKE_PLAUSIBILITY;
      return false;
    }
  }
  else
  {
    adc_runtime.brake_plausibility_timer = 0;
  }

  return true;
}

bool FEB_ADC_CheckBOTS(void)
{
  Brake_DataTypeDef brake_data;

  if (FEB_ADC_GetBrakeData(&brake_data) != ADC_STATUS_OK)
  {
    return false;
  }

  if (brake_data.bots_active)
  {
    active_faults |= FAULT_BOTS_ACTIVE;
    return true;
  }

  /* Reset BOTS if brake is released below threshold */
  if (brake_data.brake_position < BOTS_RESET_PERCENT)
  {
    active_faults &= ~FAULT_BOTS_ACTIVE;
  }

  return brake_data.bots_active;
}

uint32_t FEB_ADC_PerformSafetyChecks(void)
{
  /* Clear transient faults */
  // uint32_t previous_faults = active_faults;

  /* Perform all safety checks */
  FEB_ADC_CheckAPPSPlausibility();
  FEB_ADC_CheckBrakePlausibility();
  FEB_ADC_CheckBOTS();

  /* Additional checks can be added here */

  return active_faults;
}

ADC_StatusTypeDef FEB_ADC_ClearFaults(uint32_t fault_mask)
{
  active_faults &= ~fault_mask;

  /* Reset associated timers */
  if (fault_mask & FAULT_APPS_IMPLAUSIBILITY)
  {
    adc_runtime.apps_implausibility_timer = 0;
  }
  if (fault_mask & FAULT_BRAKE_PLAUSIBILITY)
  {
    adc_runtime.brake_plausibility_timer = 0;
  }
  if (fault_mask & FAULT_BOTS_ACTIVE)
  {
    adc_runtime.bots_timer = 0;
  }

  return ADC_STATUS_OK;
}

/* Filter and Processing Functions -------------------------------------------*/

ADC_StatusTypeDef FEB_ADC_ConfigureFilter(ADC_ChannelConfigTypeDef *config, bool enable, uint8_t samples, float alpha)
{
  if (!config)
    return ADC_STATUS_ERROR;

  config->filter.enabled = enable;
  config->filter.samples = samples;
  config->filter.alpha = alpha;

  /* Clear filter buffer */
  memset(config->filter.buffer, 0, sizeof(config->filter.buffer));
  config->filter.buffer_index = 0;

  return ADC_STATUS_OK;
}

float FEB_ADC_LowPassFilter(float new_value, float old_value, float alpha)
{
  if (alpha < 0.0f)
    alpha = 0.0f;
  if (alpha > 1.0f)
    alpha = 1.0f;

  return (alpha * new_value) + ((1.0f - alpha) * old_value);
}

/* Diagnostic and Debug Functions --------------------------------------------*/

ADC_StatusTypeDef FEB_ADC_GetDiagnostics(char *buffer, size_t size)
{
  if (!buffer || size == 0)
    return ADC_STATUS_ERROR;

  APPS_DataTypeDef apps_data;
  Brake_DataTypeDef brake_data;

  FEB_ADC_GetAPPSData(&apps_data);
  FEB_ADC_GetBrakeData(&brake_data);

  float shutdown_voltage = FEB_ADC_GetShutdownVoltage();

  snprintf(buffer, size,
           "ADC Diagnostics:\n"
           "APPS1: %.1f%% | APPS2: %.1f%% | Plausible: %s\n"
           "Brake P1: %.1f %% | P2: %.1f %% | Pressed: %s\n"
           "Shutdown: %.1f V\n"
           "Active Faults: 0x%08lX | Errors: %lu\n",
           apps_data.position1, apps_data.position2, apps_data.plausible ? "Yes" : "No", brake_data.pressure1_percent,
           brake_data.pressure2_percent, brake_data.brake_pressed ? "Yes" : "No", shutdown_voltage,
           (unsigned long)active_faults, (unsigned long)adc_runtime.error_count);

  return ADC_STATUS_OK;
}

bool FEB_ADC_IsChannelValid(ADC_HandleTypeDef *hadc, uint32_t channel)
{
  uint16_t value = FEB_ADC_GetRawValue(hadc, channel);

  return (value >= ADC_WATCHDOG_LOW_THRESHOLD && value <= ADC_WATCHDOG_HIGH_THRESHOLD);
}

uint32_t FEB_ADC_GetLastError(void)
{
  return adc_runtime.last_error_code;
}

void FEB_ADC_ResetErrors(void)
{
  adc_runtime.last_error_code = 0;
  adc_runtime.error_count = 0;
}

/* Interrupt Callbacks -------------------------------------------------------*/

void FEB_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  /* Handle conversion complete - update filtered values */
  /* This would be called from HAL_ADC_ConvCpltCallback */
}

void FEB_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  /* Handle half-transfer complete for DMA mode */
}

void FEB_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  adc_runtime.error_count++;
  adc_runtime.last_error_code = FAULT_ADC_TIMEOUT;
}

void FEB_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
  /* Handle ADC watchdog trigger */
  adc_runtime.last_error_code = FAULT_ADC_TIMEOUT;
}

/* Utility Functions ---------------------------------------------------------*/

float FEB_ADC_MapRange(float value, float in_min, float in_max, float out_min, float out_max)
{
  if (in_max == in_min)
    return out_min; /* Avoid division by zero */

  float scaled = (value - in_min) / (in_max - in_min);
  return (scaled * (out_max - out_min)) + out_min;
}

float FEB_ADC_Constrain(float value, float min, float max)
{
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

float FEB_ADC_ApplyDeadzone(float value, float deadzone)
{
  if (value < deadzone)
  {
    return 0.0f;
  }
  else if (value > (100.0f - deadzone))
  {
    return 100.0f;
  }
  else
  {
    /* Remap to full range after deadzone */
    return FEB_ADC_MapRange(value, deadzone, 100.0f - deadzone, 0.0f, 100.0f);
  }
}

/* Private Functions ---------------------------------------------------------*/

static uint16_t GetAveragedADCValue(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples)
{
  /* Input validation */
  if (hadc == NULL)
  {
    return 0;
  }

  if (samples == 0 || samples > ADC_DMA_BUFFER_SIZE)
  {
    samples = ADC_DMA_BUFFER_SIZE; // Use max if invalid
  }

  /* Average multiple values from the DMA circular buffer */
  /* No delays needed as DMA continuously updates the buffer */

  uint32_t sum = 0;
  uint32_t channel_idx = 0;
  uint16_t *buffer_ptr = NULL;
  uint32_t num_channels = 0;
  uint32_t buffer_size = 0;

  /* Determine buffer parameters */
  if (hadc == &hadc1)
  {
    buffer_ptr = adc1_dma_buffer;
    num_channels = 3;
    buffer_size = 3 * ADC_DMA_BUFFER_SIZE;
    if (channel == ADC_CHANNEL_0)
      channel_idx = ADC1_CH0_BRAKE_PRESSURE1_IDX;
    else if (channel == ADC_CHANNEL_1)
      channel_idx = ADC1_CH1_BRAKE_PRESSURE2_IDX;
    else if (channel == ADC_CHANNEL_14)
      channel_idx = ADC1_CH14_BRAKE_INPUT_IDX;
    else
      return 0;
  }
  else if (hadc == &hadc2)
  {
    buffer_ptr = adc2_dma_buffer;
    num_channels = 3;
    buffer_size = 3 * ADC_DMA_BUFFER_SIZE;
    if (channel == ADC_CHANNEL_4)
      channel_idx = ADC2_CH4_CURRENT_SENSE_IDX;
    else if (channel == ADC_CHANNEL_6)
      channel_idx = ADC2_CH6_SHUTDOWN_IN_IDX;
    else if (channel == ADC_CHANNEL_7)
      channel_idx = ADC2_CH7_PRE_TIMING_IDX;
    else
      return 0;
  }
  else if (hadc == &hadc3)
  {
    buffer_ptr = adc3_dma_buffer;
    num_channels = 4;
    buffer_size = 4 * ADC_DMA_BUFFER_SIZE;
    if (channel == ADC_CHANNEL_8)
      channel_idx = ADC3_CH8_BSPD_INDICATOR_IDX;
    else if (channel == ADC_CHANNEL_9)
      channel_idx = ADC3_CH9_BSPD_RESET_IDX;
    else if (channel == ADC_CHANNEL_12)
      channel_idx = ADC3_CH12_ACCEL_PEDAL1_IDX;
    else if (channel == ADC_CHANNEL_13)
      channel_idx = ADC3_CH13_ACCEL_PEDAL2_IDX;
    else
      return 0;
  }
  else
  {
    return 0;
  }

  /* Average the last 'samples' readings from the circular buffer */
  samples = (samples > ADC_DMA_BUFFER_SIZE) ? ADC_DMA_BUFFER_SIZE : samples;

  for (uint8_t i = 0; i < samples; i++)
  {
    /* Each set of channels is stored sequentially */
    uint32_t buffer_offset = (i * num_channels) + channel_idx;
    if (buffer_offset < buffer_size)
    {
      sum += buffer_ptr[buffer_offset];
    }
  }

  return (uint16_t)(sum / samples);
}

static void UpdateFaultTimer(uint32_t *timer, bool fault_condition, uint32_t threshold)
{
  if (fault_condition)
  {
    if (*timer == 0)
    {
      *timer = HAL_GetTick();
    }
  }
  else
  {
    *timer = 0;
  }
}
