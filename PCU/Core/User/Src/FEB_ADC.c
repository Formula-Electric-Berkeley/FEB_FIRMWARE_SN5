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
#include "FEB_CAN_BMS.h"
#include "feb_log.h"
#include "feb_string_utils.h"
#include "adc.h"
#include "usart.h"

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
#define FAULT_BIT_COUNT 8

/* Faults that must NOT be clearable via the CLI while the car is in drive. */
#define FAULT_DRIVE_LOCKED_MASK                                                                                        \
  (FAULT_APPS_IMPLAUSIBILITY | FAULT_APPS_SHORT_CIRCUIT | FAULT_APPS_OPEN_CIRCUIT | FAULT_BRAKE_PLAUSIBILITY |         \
   FAULT_BOTS_ACTIVE | FAULT_BRAKE_SENSOR_FAULT)

#define APPS_SIM_DURATION_MS 30000u
#define APPS_LOG_RATE_LIMIT_MS 500u

/* DRIVE_STATE is owned by FEB_RMS.c. We treat "in drive" as either the
 * locally cached flag OR the BMS reporting drive state. */
extern bool DRIVE_STATE;

/* Private variables ---------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

static ADC_RuntimeDataTypeDef adc_runtime = {0};
static uint32_t active_faults = 0;
static uint32_t prev_active_faults = 0;
static uint32_t fault_hit_counts[FAULT_BIT_COUNT] = {0};

/* Cached APPS data — written by FEB_ADC_TickAPPS, read by every consumer. */
static APPS_DataTypeDef apps_cache = {0};
static uint32_t apps_implaus_start_tick = 0;
static float apps_latest_deviation = 0.0f;
static uint32_t apps_v1_mv_cached = 0;
static uint32_t apps_v2_mv_cached = 0;
static uint16_t apps_raw1_cached = 0;
static uint16_t apps_raw2_cached = 0;

/* APPS sim injection state. */
static bool apps_sim_active = false;
static float apps_sim_percent = 0.0f;
static uint32_t apps_sim_until_tick = 0;

/* Runtime tunables (replace compile-time #defines for live tuning). */
bool FEB_APPS_SingleSensorMode = false;
static float apps_deadzone_percent = (float)APPS_DEADZONE_PERCENT;

/* Running statistics over the post-deadzone APPS positions. */
static struct
{
  float p1_min, p1_max, p2_min, p2_max, dev_max;
  double p1_sum, p2_sum;
  uint32_t samples;
} apps_stats = {0};

/* Rate-limit timestamps for noisy fault logs. */
static uint32_t last_open_log_tick = 0;
static uint32_t last_short_log_tick = 0;

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

#define VOLTAGE_DIVIDER_RATIO (5.0f / 3.3f) /* brake pressure, shutdown, BSPD */
#define VOLTAGE_DIVIDER_RATIO_ACCEL1 1.168f /* APPS1: k=0.856 measured (2.16V→1.849V at ADC) */
#define VOLTAGE_DIVIDER_RATIO_ACCEL2 1.0f   /* APPS2: direct connection, no resistor divider */

/* Private function prototypes -----------------------------------------------*/
static uint16_t GetAveragedADCValue(ADC_HandleTypeDef *hadc, uint32_t channel, uint8_t samples);
static bool ADC_InDriveState(void);
static void ADC_UpdateFaultEdges(uint32_t new_faults);
static void ADC_StatsAccumulate(float p1, float p2, float deviation);

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

  /* Reset APPS cache + debug state. apps_cache.plausible defaults to true so
   * the very first tick before any fault evaluation does not lock out RMS. */
  memset(&apps_cache, 0, sizeof(apps_cache));
  apps_cache.plausible = true;
  apps_implaus_start_tick = 0;
  apps_latest_deviation = 0.0f;
  apps_v1_mv_cached = 0;
  apps_v2_mv_cached = 0;
  apps_raw1_cached = 0;
  apps_raw2_cached = 0;
  apps_sim_active = false;
  active_faults = 0;
  prev_active_faults = 0;
  memset(fault_hit_counts, 0, sizeof(fault_hit_counts));
  FEB_ADC_ResetAPPSStats();

  LOG_I(TAG_ADC, "ADC initialized");
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
  {
    LOG_E(TAG_ADC, "Failed to start ADC1 DMA");
    return ADC_STATUS_ERROR;
  }

  hal_status = HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buffer, 3 * ADC_DMA_BUFFER_SIZE);
  if (hal_status != HAL_OK)
  {
    LOG_E(TAG_ADC, "Failed to start ADC2 DMA");
    HAL_ADC_Stop_DMA(&hadc1);
    return ADC_STATUS_ERROR;
  }

  hal_status = HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3_dma_buffer, 4 * ADC_DMA_BUFFER_SIZE);
  if (hal_status != HAL_OK)
  {
    LOG_E(TAG_ADC, "Failed to start ADC3 DMA");
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Stop_DMA(&hadc2);
    return ADC_STATUS_ERROR;
  }

  LOG_I(TAG_ADC, "ADC DMA started");
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
  /* Read from DMA buffer. The ADC driver fills the buffer circularly:
   *   [ch0_s0, ch1_s0, ..., chN-1_s0, ch0_s1, ch1_s1, ...]
   * The DMA stream's NDTR register reports remaining transfers. The slot
   * the DMA last wrote to is therefore: (buffer_total - NDTR - 1) mod
   * buffer_total. From there we walk back to the most recent sample for
   * the requested channel. */
  uint16_t *buffer_ptr = NULL;
  uint32_t channel_idx = 0;
  uint32_t num_channels = 0;

  if (hadc == &hadc1)
  {
    buffer_ptr = adc1_dma_buffer;
    num_channels = 3;
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

  uint32_t buffer_total = num_channels * ADC_DMA_BUFFER_SIZE;

  /* If DMA is not running yet, fall back to slot 0. */
  if (hadc->DMA_Handle == NULL)
  {
    return buffer_ptr[channel_idx];
  }

  uint32_t ndtr = __HAL_DMA_GET_COUNTER(hadc->DMA_Handle);
  if (ndtr == 0 || ndtr > buffer_total)
  {
    return buffer_ptr[channel_idx];
  }

  /* Index of the slot the DMA last completed. */
  uint32_t last_written = (buffer_total - ndtr + buffer_total - 1) % buffer_total;

  /* Walk back to the most recent slot whose modular index matches our channel. */
  uint32_t offset = (last_written + buffer_total - channel_idx) % num_channels;
  uint32_t latest = (last_written + buffer_total - offset) % buffer_total;

  return buffer_ptr[latest];
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
  return FEB_ADC_RawToVoltage(raw) * VOLTAGE_DIVIDER_RATIO_ACCEL2;
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

void FEB_ADC_TickAPPS(void)
{
  uint32_t now = HAL_GetTick();

  /* Defense in depth: a sim must never survive entry into drive state. */
  if (apps_sim_active && ADC_InDriveState())
  {
    apps_sim_active = false;
    LOG_W(TAG_ADC, "APPS sim cancelled: BMS entered drive state");
  }

  /* Cache raw + voltage readings up front so the snapshot view is
   * self-consistent with whatever the plausibility logic uses below. */
  apps_raw1_cached = FEB_ADC_GetAccelPedal1Raw();
  apps_raw2_cached = FEB_ADC_GetAccelPedal2Raw();
  float voltage1 = FEB_ADC_GetAccelPedal1Voltage() * 1000.0f;
  float voltage2 = FEB_ADC_GetAccelPedal2Voltage() * 1000.0f;
  apps_v1_mv_cached = (uint32_t)voltage1;
  apps_v2_mv_cached = (uint32_t)voltage2;

  /* Sim short-circuits the rest of the path. The sim window is 30 s; on
   * expiry we log once and drop back to real sensor handling. */
  if (apps_sim_active)
  {
    if ((int32_t)(now - apps_sim_until_tick) >= 0)
    {
      apps_sim_active = false;
      LOG_W(TAG_ADC, "APPS sim window expired");
    }
    else
    {
      apps_cache.position1 = apps_sim_percent;
      apps_cache.position2 = apps_sim_percent;
      apps_cache.acceleration = apps_sim_percent;
      apps_cache.plausible = true;
      apps_cache.short_circuit = false;
      apps_cache.open_circuit = false;
      apps_cache.implausibility_time = 0;
      apps_implaus_start_tick = 0;
      apps_latest_deviation = 0.0f;
      ADC_StatsAccumulate(apps_sim_percent, apps_sim_percent, 0.0f);
      ADC_UpdateFaultEdges(active_faults);
      return;
    }
  }

  /* Sensor circuit faults — voltage is post-divider, in mV. */
  bool short_circuit = (voltage1 < APPS_SHORT_CIRCUIT_DETECT_MV) || (voltage2 < APPS_SHORT_CIRCUIT_DETECT_MV);
  bool open_circuit = (voltage1 > APPS1_OPEN_CIRCUIT_DETECT_MV) || (voltage2 > APPS2_OPEN_CIRCUIT_DETECT_MV);
  apps_cache.short_circuit = short_circuit;
  apps_cache.open_circuit = open_circuit;

  if (short_circuit && (now - last_short_log_tick) >= APPS_LOG_RATE_LIMIT_MS)
  {
    last_short_log_tick = now;
    LOG_E(TAG_ADC, "APPS short circuit: V1=%.0fmV V2=%.0fmV", voltage1, voltage2);
  }
  if (open_circuit && (now - last_open_log_tick) >= APPS_LOG_RATE_LIMIT_MS)
  {
    last_open_log_tick = now;
    LOG_E(TAG_ADC, "APPS open circuit: V1=%.0fmV V2=%.0fmV", voltage1, voltage2);
  }

  /* Map voltage → percent using current calibration, constrain, then deadzone. */
  float position1 =
      FEB_ADC_MapRange(voltage1, apps1_calibration.min_voltage, apps1_calibration.max_voltage, 0.0f, 100.0f);
  float position2 =
      FEB_ADC_MapRange(voltage2, apps2_calibration.min_voltage, apps2_calibration.max_voltage, 0.0f, 100.0f);
  position1 = FEB_ADC_Constrain(position1, 0.0f, 100.0f);
  position2 = FEB_ADC_Constrain(position2, 0.0f, 100.0f);
  position1 = FEB_ADC_ApplyDeadzone(position1, apps_deadzone_percent);
  position2 = FEB_ADC_ApplyDeadzone(position2, apps_deadzone_percent);

  apps_cache.position1 = position1;
  apps_cache.position2 = position2;

  float deviation = fabsf(position1 - position2);
  apps_latest_deviation = deviation;

  if (FEB_APPS_SingleSensorMode)
  {
    /* Bench-mode bypass — only available outside drive state, so we don't
     * gate again here, just skip the dual-sensor logic. */
    apps_cache.position2 = position1;
    apps_cache.acceleration = position1;
    apps_cache.plausible = true;
    apps_cache.implausibility_time = 0;
    apps_implaus_start_tick = 0;
  }
  else
  {
    apps_cache.acceleration = (position1 + position2) / 2.0f;

    /* FSAE EV.5.3: implausible = persistent disagreement >10% for >100 ms. */
    bool in_disagreement = (deviation > (float)APPS_PLAUSIBILITY_TOLERANCE);

    if (in_disagreement)
    {
      if (apps_implaus_start_tick == 0)
      {
        apps_implaus_start_tick = now;
        if (apps_implaus_start_tick == 0)
          apps_implaus_start_tick = 1; /* sentinel: 0 means "not started" */
      }
      uint32_t elapsed = now - apps_implaus_start_tick;
      apps_cache.implausibility_time = elapsed;

      /* Latch the fault and the local plausible flag once we cross the
       * 100 ms persistence threshold; both stay set until cleared by
       * release-and-reset (FEB_ADC_AcknowledgeAPPSImplausibility). */
      if (elapsed > APPS_IMPLAUSIBILITY_TIME_MS)
      {
        if ((active_faults & FAULT_APPS_IMPLAUSIBILITY) == 0)
        {
          LOG_E(TAG_ADC, "APPS implausibility latched (%.1fms, dev=%.1f%%)", (double)elapsed, (double)deviation);
        }
        active_faults |= FAULT_APPS_IMPLAUSIBILITY;
        apps_cache.plausible = false;
      }
      else if ((active_faults & FAULT_APPS_IMPLAUSIBILITY) == 0)
      {
        /* Disagreeing but not yet latched — caller may still drive. */
        apps_cache.plausible = true;
      }
      else
      {
        apps_cache.plausible = false;
      }
    }
    else
    {
      apps_implaus_start_tick = 0;
      /* If a previous latch exists, leave plausible=false until the
       * release-and-reset condition clears it. */
      if ((active_faults & FAULT_APPS_IMPLAUSIBILITY) == 0)
      {
        apps_cache.plausible = true;
        apps_cache.implausibility_time = 0;
      }
      else
      {
        apps_cache.plausible = false;
      }
    }
  }

  /* Latch sensor-circuit faults. They clear when the condition clears. */
  if (short_circuit)
    active_faults |= FAULT_APPS_SHORT_CIRCUIT;
  else
    active_faults &= ~FAULT_APPS_SHORT_CIRCUIT;

  if (open_circuit)
    active_faults |= FAULT_APPS_OPEN_CIRCUIT;
  else
    active_faults &= ~FAULT_APPS_OPEN_CIRCUIT;

  ADC_StatsAccumulate(apps_cache.position1, apps_cache.position2, deviation);
  ADC_UpdateFaultEdges(active_faults);
}

ADC_StatusTypeDef FEB_ADC_GetAPPSData(APPS_DataTypeDef *apps_data)
{
  if (!apps_data)
    return ADC_STATUS_ERROR;
  *apps_data = apps_cache;
  return ADC_STATUS_OK;
}

void FEB_ADC_GetAPPSCacheSnapshot(APPS_DataTypeDef *out, uint16_t *raw1, uint16_t *raw2, float *v1_mv, float *v2_mv,
                                  uint32_t *fault_bitmask, uint32_t *implaus_elapsed_ms, float *latest_deviation)
{
  if (out)
    *out = apps_cache;
  if (raw1)
    *raw1 = apps_raw1_cached;
  if (raw2)
    *raw2 = apps_raw2_cached;
  if (v1_mv)
    *v1_mv = (float)apps_v1_mv_cached;
  if (v2_mv)
    *v2_mv = (float)apps_v2_mv_cached;
  if (fault_bitmask)
    *fault_bitmask = active_faults;
  if (implaus_elapsed_ms)
    *implaus_elapsed_ms = apps_cache.implausibility_time;
  if (latest_deviation)
    *latest_deviation = apps_latest_deviation;
}

void FEB_ADC_AcknowledgeAPPSImplausibility(void)
{
  if (apps_cache.position1 < 5.0f && apps_cache.position2 < 5.0f)
  {
    if (active_faults & FAULT_APPS_IMPLAUSIBILITY)
    {
      LOG_I(TAG_ADC, "APPS plausibility cleared on release");
    }
    active_faults &= ~FAULT_APPS_IMPLAUSIBILITY;
    apps_implaus_start_tick = 0;
    apps_cache.plausible = true;
    apps_cache.implausibility_time = 0;
  }
}

void FEB_ADC_AcknowledgeBrakeImplausibility(void)
{
  /* Brake plausibility is recomputed every read in FEB_ADC_GetBrakeData;
   * here we only clear the latched fault bit and timer once travel is low. */
  Brake_DataTypeDef brake_data;
  if (FEB_ADC_GetBrakeData(&brake_data) != ADC_STATUS_OK)
    return;
  if (brake_data.brake_position < 15.0f)
  {
    if (active_faults & FAULT_BRAKE_PLAUSIBILITY)
    {
      LOG_I(TAG_ADC, "Brake plausibility cleared on release");
    }
    active_faults &= ~FAULT_BRAKE_PLAUSIBILITY;
    adc_runtime.brake_plausibility_timer = 0;
  }
}

ADC_StatusTypeDef FEB_ADC_GetBrakeData(Brake_DataTypeDef *brake_data)
{
  if (!brake_data)
    return ADC_STATUS_ERROR;

  /* Default to plausible; the disagreement check below flips it to false. */
  brake_data->plausible = true;

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

  /* Check plausibility between pressure sensors. Both inputs are
   * percentages, so the threshold must be a percentage too. */
  float pressure_diff = fabsf(brake_data->pressure1_percent - brake_data->pressure2_percent);
  brake_data->plausible = (pressure_diff <= BRAKE_PRESSURE_PLAUSIBILITY_TOLERANCE_PERCENT);

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
  /* All actual computation lives in FEB_ADC_TickAPPS(); this entry point
   * just reports the latched state so consumers stay in sync. */
  if (active_faults & (FAULT_APPS_IMPLAUSIBILITY | FAULT_APPS_SHORT_CIRCUIT | FAULT_APPS_OPEN_CIRCUIT))
    return false;
  return apps_cache.plausible;
}

bool FEB_ADC_CheckBrakePlausibility(void)
{
  APPS_DataTypeDef apps_data;
  Brake_DataTypeDef brake_data;

  if (FEB_ADC_GetAPPSData(&apps_data) != ADC_STATUS_OK || FEB_ADC_GetBrakeData(&brake_data) != ADC_STATUS_OK)
  {
    return false;
  }

  /* FSAE EV.5.7: brake hard + throttle >25% latches BSPD until both
   * conditions clear; latch persists for 100 ms before becoming a fault. */
  bool brake_hard = (brake_data.pressure1_percent > BRAKE_PRESSURE_THRESHOLD_PERCENT) ||
                    (brake_data.pressure2_percent > BRAKE_PRESSURE_THRESHOLD_PERCENT);
  bool throttle_high = (apps_data.acceleration > 25.0f);
  uint32_t now = HAL_GetTick();

  if (brake_hard && throttle_high)
  {
    if (adc_runtime.brake_plausibility_timer == 0)
    {
      adc_runtime.brake_plausibility_timer = now;
      if (adc_runtime.brake_plausibility_timer == 0)
        adc_runtime.brake_plausibility_timer = 1;
    }
    uint32_t elapsed = now - adc_runtime.brake_plausibility_timer;
    if (elapsed > BRAKE_PLAUSIBILITY_TIME_MS)
    {
      if ((active_faults & FAULT_BRAKE_PLAUSIBILITY) == 0)
      {
        LOG_E(TAG_ADC, "Brake plausibility fault: brake hard + throttle >25%%");
      }
      active_faults |= FAULT_BRAKE_PLAUSIBILITY;
      ADC_UpdateFaultEdges(active_faults);
      return false;
    }
  }
  else
  {
    adc_runtime.brake_plausibility_timer = 0;
  }

  return (active_faults & FAULT_BRAKE_PLAUSIBILITY) == 0;
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
  prev_active_faults &= ~fault_mask;

  /* Reset associated timers */
  if (fault_mask & FAULT_APPS_IMPLAUSIBILITY)
  {
    adc_runtime.apps_implausibility_timer = 0;
    apps_implaus_start_tick = 0;
    apps_cache.plausible = true;
    apps_cache.implausibility_time = 0;
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

uint32_t FEB_ADC_GetActiveFaults(void)
{
  return active_faults;
}

/* Map a single-bit fault value to the index used by fault_hit_counts. */
static int fault_bit_to_index(uint32_t fault_bit)
{
  switch (fault_bit)
  {
  case FAULT_APPS_IMPLAUSIBILITY:
    return 0;
  case FAULT_BRAKE_PLAUSIBILITY:
    return 1;
  case FAULT_BOTS_ACTIVE:
    return 2;
  case FAULT_APPS_SHORT_CIRCUIT:
    return 3;
  case FAULT_APPS_OPEN_CIRCUIT:
    return 4;
  case FAULT_BRAKE_SENSOR_FAULT:
    return 5;
  case FAULT_CURRENT_SENSOR_FAULT:
    return 6;
  case FAULT_ADC_TIMEOUT:
    return 7;
  default:
    return -1;
  }
}

uint32_t FEB_ADC_GetFaultHitCount(uint32_t fault_bit)
{
  int idx = fault_bit_to_index(fault_bit);
  if (idx < 0)
    return 0;
  return fault_hit_counts[idx];
}

static const struct
{
  const char *name;
  uint32_t bit;
} fault_name_table[] = {
    {"APPS_IMPLAUSIBILITY", FAULT_APPS_IMPLAUSIBILITY},
    {"BRAKE_PLAUSIBILITY", FAULT_BRAKE_PLAUSIBILITY},
    {"BOTS_ACTIVE", FAULT_BOTS_ACTIVE},
    {"APPS_SHORT_CIRCUIT", FAULT_APPS_SHORT_CIRCUIT},
    {"APPS_OPEN_CIRCUIT", FAULT_APPS_OPEN_CIRCUIT},
    {"BRAKE_SENSOR_FAULT", FAULT_BRAKE_SENSOR_FAULT},
    {"CURRENT_SENSOR_FAULT", FAULT_CURRENT_SENSOR_FAULT},
    {"ADC_TIMEOUT", FAULT_ADC_TIMEOUT},
};
#define FAULT_NAME_COUNT (sizeof(fault_name_table) / sizeof(fault_name_table[0]))

uint32_t FEB_ADC_FaultBitFromName(const char *name)
{
  if (!name)
    return 0;
  /* Skip an optional "FAULT_" prefix so callers can pass the C-style
   * define name interchangeably with the short form. */
  const char *bare = name;
  if ((bare[0] == 'F' || bare[0] == 'f') && (bare[1] == 'A' || bare[1] == 'a') && (bare[2] == 'U' || bare[2] == 'u') &&
      (bare[3] == 'L' || bare[3] == 'l') && (bare[4] == 'T' || bare[4] == 't') && bare[5] == '_')
  {
    bare += 6;
  }
  for (size_t i = 0; i < FAULT_NAME_COUNT; i++)
  {
    if (FEB_strcasecmp(name, fault_name_table[i].name) == 0)
      return fault_name_table[i].bit;
    if (bare != name && FEB_strcasecmp(bare, fault_name_table[i].name) == 0)
      return fault_name_table[i].bit;
  }
  return 0;
}

const char *FEB_ADC_FaultBitName(uint32_t fault_bit)
{
  for (size_t i = 0; i < FAULT_NAME_COUNT; i++)
  {
    if (fault_name_table[i].bit == fault_bit)
      return fault_name_table[i].name;
  }
  return "UNKNOWN";
}

ADC_StatusTypeDef FEB_ADC_InjectFault(uint32_t fault_bit)
{
  if (fault_bit == 0)
    return ADC_STATUS_ERROR;
  if (ADC_InDriveState())
    return ADC_STATUS_ERROR;
  active_faults |= fault_bit;
  if (fault_bit & FAULT_APPS_IMPLAUSIBILITY)
  {
    apps_cache.plausible = false;
    if (apps_implaus_start_tick == 0)
      apps_implaus_start_tick = HAL_GetTick();
  }
  ADC_UpdateFaultEdges(active_faults);
  LOG_W(TAG_ADC, "Fault injected: %s", FEB_ADC_FaultBitName(fault_bit));
  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_ClearFaultsByName(const char *name)
{
  if (!name)
    return ADC_STATUS_ERROR;
  uint32_t mask;
  if (FEB_strcasecmp(name, "all") == 0)
  {
    mask = ~0u;
  }
  else
  {
    mask = FEB_ADC_FaultBitFromName(name);
    if (mask == 0)
      return ADC_STATUS_ERROR;
  }

  /* Refuse to clear safety-related faults while the car is live. */
  if (ADC_InDriveState() && (mask & FAULT_DRIVE_LOCKED_MASK))
    return ADC_STATUS_ERROR;

  return FEB_ADC_ClearFaults(mask);
}

/* ============================================================================
 * Runtime overrides for bench debugging
 * ============================================================================ */

ADC_StatusTypeDef FEB_ADC_SetSingleSensorMode(bool enabled)
{
  if (enabled && ADC_InDriveState())
    return ADC_STATUS_ERROR;
  if (FEB_APPS_SingleSensorMode != enabled)
  {
    FEB_APPS_SingleSensorMode = enabled;
    LOG_W(TAG_ADC, "APPS single-sensor mode %s", enabled ? "ENABLED" : "disabled");
  }
  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_SetAPPSSimulation(bool enabled, float percent)
{
  if (enabled && ADC_InDriveState())
    return ADC_STATUS_ERROR;
  if (!enabled)
  {
    if (apps_sim_active)
      LOG_W(TAG_ADC, "APPS sim cleared");
    apps_sim_active = false;
    return ADC_STATUS_OK;
  }
  if (percent < 0.0f)
    percent = 0.0f;
  if (percent > 100.0f)
    percent = 100.0f;
  apps_sim_percent = percent;
  apps_sim_until_tick = HAL_GetTick() + APPS_SIM_DURATION_MS;
  apps_sim_active = true;
  LOG_W(TAG_ADC, "APPS sim active at %.1f%% for %lums", (double)percent, (unsigned long)APPS_SIM_DURATION_MS);
  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_CaptureAPPSCalibration(uint8_t sensor, bool capture_max)
{
  if (sensor != 1 && sensor != 2)
    return ADC_STATUS_ERROR;
  ADC_CalibrationTypeDef *cal = (sensor == 1) ? &apps1_calibration : &apps2_calibration;
  float voltage_mv =
      (sensor == 1) ? FEB_ADC_GetAccelPedal1Voltage() * 1000.0f : FEB_ADC_GetAccelPedal2Voltage() * 1000.0f;
  if (capture_max)
    cal->max_voltage = voltage_mv;
  else
    cal->min_voltage = voltage_mv;
  LOG_I(TAG_ADC, "APPS%u %s captured at %.0fmV", sensor, capture_max ? "max" : "min", (double)voltage_mv);
  return ADC_STATUS_OK;
}

void FEB_ADC_GetAPPSFilterConfig(bool *enabled, uint8_t *samples, float *alpha)
{
  if (enabled)
    *enabled = accel_pedal1_config.filter.enabled ? true : false;
  if (samples)
    *samples = accel_pedal1_config.filter.samples;
  if (alpha)
    *alpha = accel_pedal1_config.filter.alpha;
}

ADC_StatusTypeDef FEB_ADC_SetAPPSFilter(bool enabled, uint8_t samples, float alpha)
{
  if (samples < 1)
    samples = 1;
  if (samples > ADC_DMA_BUFFER_SIZE)
    samples = ADC_DMA_BUFFER_SIZE;
  if (alpha < 0.0f)
    alpha = 0.0f;
  if (alpha > 1.0f)
    alpha = 1.0f;
  accel_pedal1_config.filter.enabled = enabled ? 1 : 0;
  accel_pedal1_config.filter.samples = samples;
  accel_pedal1_config.filter.alpha = alpha;
  accel_pedal2_config.filter.enabled = enabled ? 1 : 0;
  accel_pedal2_config.filter.samples = samples;
  accel_pedal2_config.filter.alpha = alpha;
  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_SetAPPSDeadzone(float percent)
{
  if (percent < 0.0f)
    percent = 0.0f;
  if (percent > 20.0f)
    percent = 20.0f;
  apps_deadzone_percent = percent;
  return ADC_STATUS_OK;
}

float FEB_ADC_GetAPPSDeadzone(void)
{
  return apps_deadzone_percent;
}

void FEB_ADC_ResetAPPSStats(void)
{
  apps_stats.p1_min = 100.0f;
  apps_stats.p1_max = 0.0f;
  apps_stats.p2_min = 100.0f;
  apps_stats.p2_max = 0.0f;
  apps_stats.dev_max = 0.0f;
  apps_stats.p1_sum = 0.0;
  apps_stats.p2_sum = 0.0;
  apps_stats.samples = 0;
}

void FEB_ADC_GetAPPSStats(float *p1_min, float *p1_max, float *p2_min, float *p2_max, float *p1_avg, float *p2_avg,
                          float *dev_max, uint32_t *samples)
{
  uint32_t n = apps_stats.samples;
  if (p1_min)
    *p1_min = (n == 0) ? 0.0f : apps_stats.p1_min;
  if (p1_max)
    *p1_max = apps_stats.p1_max;
  if (p2_min)
    *p2_min = (n == 0) ? 0.0f : apps_stats.p2_min;
  if (p2_max)
    *p2_max = apps_stats.p2_max;
  if (p1_avg)
    *p1_avg = (n == 0) ? 0.0f : (float)(apps_stats.p1_sum / (double)n);
  if (p2_avg)
    *p2_avg = (n == 0) ? 0.0f : (float)(apps_stats.p2_sum / (double)n);
  if (dev_max)
    *dev_max = apps_stats.dev_max;
  if (samples)
    *samples = n;
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

static bool ADC_InDriveState(void)
{
  return DRIVE_STATE || FEB_CAN_BMS_InDriveState();
}

static void ADC_UpdateFaultEdges(uint32_t new_faults)
{
  uint32_t rising = new_faults & ~prev_active_faults;
  if (rising)
  {
    for (size_t i = 0; i < FAULT_BIT_COUNT; i++)
    {
      if (rising & (1u << i))
        fault_hit_counts[i]++;
    }
  }
  prev_active_faults = new_faults;
}

static void ADC_StatsAccumulate(float p1, float p2, float deviation)
{
  if (apps_stats.samples == 0)
  {
    apps_stats.p1_min = p1;
    apps_stats.p1_max = p1;
    apps_stats.p2_min = p2;
    apps_stats.p2_max = p2;
    apps_stats.dev_max = deviation;
  }
  else
  {
    if (p1 < apps_stats.p1_min)
      apps_stats.p1_min = p1;
    if (p1 > apps_stats.p1_max)
      apps_stats.p1_max = p1;
    if (p2 < apps_stats.p2_min)
      apps_stats.p2_min = p2;
    if (p2 > apps_stats.p2_max)
      apps_stats.p2_max = p2;
    if (deviation > apps_stats.dev_max)
      apps_stats.dev_max = deviation;
  }
  apps_stats.p1_sum += p1;
  apps_stats.p2_sum += p2;
  apps_stats.samples++;
}
