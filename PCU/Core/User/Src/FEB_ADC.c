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
#define FAULT_APPS_SIGNAL_SHORT (1 << 8) /* Line-to-line APPS signal short (FSAE T.4.2.3) */
#define FAULT_BIT_COUNT 9

/* Faults that must NOT be clearable via the CLI while the car is in drive. */
#define FAULT_DRIVE_LOCKED_MASK                                                                                        \
  (FAULT_APPS_IMPLAUSIBILITY | FAULT_APPS_SHORT_CIRCUIT | FAULT_APPS_OPEN_CIRCUIT | FAULT_APPS_SIGNAL_SHORT |          \
   FAULT_BRAKE_PLAUSIBILITY | FAULT_BOTS_ACTIVE | FAULT_BRAKE_SENSOR_FAULT)

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
static float apps_latest_raw_separation = 0.0f; /* RAW pin-domain |raw1-raw2| %, for T.4.2.3 short detect */
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
static uint32_t last_bse_log_tick = 0;

/* Brake fault detection state (owned by FEB_ADC_TickBrakeFaults, 1 ms tick). */
static uint32_t bse_fault_start_tick = 0; /* T.4.3.4 BSE open/short 100 ms latch */
static uint32_t ev47_start_tick = 0;      /* EV.4.7 brake+throttle debounce */
static bool brake_bypass_enabled = false; /* mirrors FEB_RMS bench brake bypass */

/* DMA Buffers for continuous conversion - must match number of channels */
static uint16_t adc1_dma_buffer[3 * ADC_DMA_BUFFER_SIZE]; /* 3 channels: PA0, PA1, PC4 */
static uint16_t adc2_dma_buffer[3 * ADC_DMA_BUFFER_SIZE]; /* 3 channels: PA4, PA6, PA7 */
static uint16_t adc3_dma_buffer[4 * ADC_DMA_BUFFER_SIZE]; /* 4 channels: PC0, PC1, PC2, PC3 */

/* Channel indices in DMA buffers */
#define ADC1_CH0_BRAKE_PRESSURE2_IDX 0 /* PA0 - Channel 0 - Brake Pressure 2 (BP2) */
#define ADC1_CH1_BRAKE_PRESSURE1_IDX 1 /* PA1 - Channel 1 - Brake Pressure 1 (BP1) */
#define ADC1_CH14_BRAKE_INPUT_IDX 2    /* PC4 - Channel 14 */

#define ADC2_CH4_CURRENT_SENSE_IDX 0 /* PA4 - Channel 4 */
#define ADC2_CH6_SHUTDOWN_IN_IDX 1   /* PA6 - Channel 6 */
#define ADC2_CH7_PRE_TIMING_IDX 2    /* PA7 - Channel 7 */

#define ADC3_CH10_BSPD_INDICATOR_IDX 0 /* PC0 - Channel 10 - BSPD Indicator */
#define ADC3_CH11_BSPD_RESET_IDX 1     /* PC1 - Channel 11 - BSPD Reset */
#define ADC3_CH12_ACCEL_PEDAL2_IDX 2   /* PC2 - Channel 12 - APPS2 (Acceleration_Pedal_2) */
#define ADC3_CH13_ACCEL_PEDAL1_IDX 3   /* PC3 - Channel 13 - APPS1 (Acceleration_Pedal_1) */

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
    .min_voltage = BRAKE_PRESSURE_1_MIN_MV,          /* Sensor 1 @ 0%  brake */
    .max_voltage = BRAKE_PRESSURE_1_MAX_MV,          /* Sensor 1 @ 100% brake */
    .min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR, /* Physical: 0 bar */
    .max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR, /* Physical: 200 bar */
    .inverted = false};

static ADC_CalibrationTypeDef brake_pressure2_calibration = {
    .offset = 0.0f,
    .gain = 1.0f,
    .min_voltage = BRAKE_PRESSURE_2_MIN_MV,          /* Sensor 2 @ 0%  brake */
    .max_voltage = BRAKE_PRESSURE_2_MAX_MV,          /* Sensor 2 @ 100% brake */
    .min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR, /* Physical: 0 bar */
    .max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR, /* Physical: 200 bar */
    .inverted = false};

#define VOLTAGE_DIVIDER_RATIO (5.0f / 3.3f)       /* shutdown, current sense, pre-timing trip */
#define VOLTAGE_DIVIDER_RATIO_BRAKE (5.0f / 3.3f) /* brake: 5V->3.3V PCB divider */
#define VOLTAGE_DIVIDER_RATIO_ACCEL1 1.168f       /* APPS1: k=0.856 measured (2.16V→1.849V at ADC) */
#define VOLTAGE_DIVIDER_RATIO_ACCEL2 1.0f         /* APPS2: direct connection, no resistor divider */

/* Private function prototypes -----------------------------------------------*/
static uint16_t ADC_BoxcarRaw(ADC_HandleTypeDef *hadc, uint16_t *buffer_ptr, uint32_t num_channels,
                              uint32_t channel_idx, uint8_t samples);
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

  /* Accelerator Pedal Sensor 1 Configuration */
  accel_pedal1_config.hadc = &hadc3;
  accel_pedal1_config.channel = ADC3_ACCEL_PEDAL_1_CHANNEL;
  accel_pedal1_config.filter.enabled = FILTER_ACCEL_PEDAL_ENABLED;
  accel_pedal1_config.filter.samples = FILTER_ACCEL_PEDAL_SAMPLES;

  /* Accelerator Pedal Sensor 2 Configuration */
  accel_pedal2_config.hadc = &hadc3;
  accel_pedal2_config.channel = ADC3_ACCEL_PEDAL_2_CHANNEL;
  accel_pedal2_config.filter.enabled = FILTER_ACCEL_PEDAL_ENABLED;
  accel_pedal2_config.filter.samples = FILTER_ACCEL_PEDAL_SAMPLES;

  /* Brake Pressure Sensor 1 Configuration */
  brake_pressure1_config.hadc = &hadc1;
  brake_pressure1_config.channel = ADC1_BRAKE_PRESSURE_1_CHANNEL;
  brake_pressure1_config.filter.enabled = FILTER_BRAKE_PRESSURE_ENABLED;
  brake_pressure1_config.filter.samples = FILTER_BRAKE_PRESSURE_SAMPLES;

  /* Brake Pressure Sensor 2 Configuration */
  brake_pressure2_config.hadc = &hadc1;
  brake_pressure2_config.channel = ADC1_BRAKE_PRESSURE_2_CHANNEL;
  brake_pressure2_config.filter.enabled = FILTER_BRAKE_PRESSURE_ENABLED;
  brake_pressure2_config.filter.samples = FILTER_BRAKE_PRESSURE_SAMPLES;

  /* Current Sensor Configuration */
  current_sense_config.hadc = &hadc2;
  current_sense_config.channel = ADC2_CURRENT_SENSE_CHANNEL;
  current_sense_config.filter.enabled = FILTER_CURRENT_SENSE_ENABLED;
  current_sense_config.filter.samples = FILTER_CURRENT_SENSE_SAMPLES;

  /* Shutdown Circuit Monitoring Configuration */
  shutdown_in_config.hadc = &hadc2;
  shutdown_in_config.channel = ADC2_SHUTDOWN_IN_CHANNEL;
  shutdown_in_config.filter.enabled = FILTER_SHUTDOWN_ENABLED;
  shutdown_in_config.filter.samples = FILTER_SHUTDOWN_SAMPLES;

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
  apps_latest_raw_separation = 0.0f;
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

/* Coherent Snapshot ---------------------------------------------------------*/

/* One time-coherent snapshot, published once per 1 ms control tick by
 * FEB_ADC_TickSample() and read tear-free via FEB_ADC_GetCoherentSnapshot().
 * Single-writer (1 ms TIM1 ISR, the highest priority among ADC consumers)
 * seqlock: readers retry while the sequence is odd or changes mid-copy. */
static ADC_CoherentSnapshot_t adc_snapshot = {0};
static volatile uint32_t adc_snapshot_seq = 0;

/* Staleness watchdog: if the summed DMA NDTR across all three ADCs stops
 * advancing (TIM2 trigger or DMA stalled) the snapshot would silently freeze.
 * Detect it and raise FAULT_ADC_TIMEOUT so it is visible and edge-counted. */
static uint32_t adc_prev_ndtr_sum = 0xFFFFFFFFu;
static uint32_t adc_stale_ticks = 0;
#define ADC_STALE_FAULT_TICKS 5u /* ~5 ms with no new conversions => stalled */

/* Boxcar-average the most-recent `samples` conversions for one channel by
 * walking back from the DMA write pointer. Every ADC is launched by the same
 * TIM2 TRGO edge, so the most-recent samples are time-coherent across ADCs.
 * Mirrors the NDTR index math in FEB_ADC_GetRawValue, extended to N samples. */
static uint16_t ADC_BoxcarRaw(ADC_HandleTypeDef *hadc, uint16_t *buffer_ptr, uint32_t num_channels,
                              uint32_t channel_idx, uint8_t samples)
{
  if (samples < 1)
    samples = 1;
  if (samples > ADC_DMA_BUFFER_SIZE)
    samples = ADC_DMA_BUFFER_SIZE;

  uint32_t buffer_total = num_channels * ADC_DMA_BUFFER_SIZE;

  if (hadc->DMA_Handle == NULL)
    return buffer_ptr[channel_idx];

  uint32_t ndtr = __HAL_DMA_GET_COUNTER(hadc->DMA_Handle);
  if (ndtr == 0 || ndtr > buffer_total)
    return buffer_ptr[channel_idx];

  /* Most-recent fully-written slot for this channel (see FEB_ADC_GetRawValue). */
  uint32_t last_written = (buffer_total - ndtr + buffer_total - 1) % buffer_total;
  uint32_t offset = (last_written + buffer_total - channel_idx) % num_channels;
  uint32_t latest = (last_written + buffer_total - offset) % buffer_total;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; i++)
  {
    uint32_t slot = (latest + buffer_total - (uint32_t)i * num_channels) % buffer_total;
    sum += buffer_ptr[slot];
  }
  return (uint16_t)(sum / samples);
}

static uint32_t ADC_SumNDTR(void)
{
  uint32_t s = 0;
  if (hadc1.DMA_Handle)
    s += __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
  if (hadc2.DMA_Handle)
    s += __HAL_DMA_GET_COUNTER(hadc2.DMA_Handle);
  if (hadc3.DMA_Handle)
    s += __HAL_DMA_GET_COUNTER(hadc3.DMA_Handle);
  return s;
}

void FEB_ADC_TickSample(void)
{
  /* Per-class boxcar window (samples), shared within each plausibility pair so
   * filtering can never manufacture an inter-sensor deviation. enabled=0 => 1. */
  uint8_t n_apps = accel_pedal1_config.filter.enabled ? accel_pedal1_config.filter.samples : 1;
  uint8_t n_brkp = brake_pressure1_config.filter.enabled ? brake_pressure1_config.filter.samples : 1;
  uint8_t n_brki = brake_input_config.filter.enabled ? brake_input_config.filter.samples : 1;
  uint8_t n_curr = current_sense_config.filter.enabled ? current_sense_config.filter.samples : 1;
  uint8_t n_shdn = shutdown_in_config.filter.enabled ? shutdown_in_config.filter.samples : 1;

  ADC_CoherentSnapshot_t s;
  s.tick_ms = HAL_GetTick();

  /* ADC1 (DMA2_S0): brake2 (idx0), brake1 (idx1), brake_input (idx2). */
  s.brake2_raw = ADC_BoxcarRaw(&hadc1, adc1_dma_buffer, 3, ADC1_CH0_BRAKE_PRESSURE2_IDX, n_brkp);
  s.brake1_raw = ADC_BoxcarRaw(&hadc1, adc1_dma_buffer, 3, ADC1_CH1_BRAKE_PRESSURE1_IDX, n_brkp);
  s.brake_input_raw = ADC_BoxcarRaw(&hadc1, adc1_dma_buffer, 3, ADC1_CH14_BRAKE_INPUT_IDX, n_brki);

  /* ADC2 (DMA2_S2): current (idx0), shutdown (idx1), pre-timing (idx2). */
  s.current_raw = ADC_BoxcarRaw(&hadc2, adc2_dma_buffer, 3, ADC2_CH4_CURRENT_SENSE_IDX, n_curr);
  s.shutdown_raw = ADC_BoxcarRaw(&hadc2, adc2_dma_buffer, 3, ADC2_CH6_SHUTDOWN_IN_IDX, n_shdn);
  s.pretiming_raw = ADC_BoxcarRaw(&hadc2, adc2_dma_buffer, 3, ADC2_CH7_PRE_TIMING_IDX, 1);

  /* ADC3 (DMA2_S1): bspd_ind (idx0), bspd_rst (idx1), apps2 (idx2), apps1 (idx3). */
  s.bspd_ind_raw = ADC_BoxcarRaw(&hadc3, adc3_dma_buffer, 4, ADC3_CH10_BSPD_INDICATOR_IDX, 1);
  s.bspd_rst_raw = ADC_BoxcarRaw(&hadc3, adc3_dma_buffer, 4, ADC3_CH11_BSPD_RESET_IDX, 1);
  s.apps2_raw = ADC_BoxcarRaw(&hadc3, adc3_dma_buffer, 4, ADC3_CH12_ACCEL_PEDAL2_IDX, n_apps);
  s.apps1_raw = ADC_BoxcarRaw(&hadc3, adc3_dma_buffer, 4, ADC3_CH13_ACCEL_PEDAL1_IDX, n_apps);

  /* Publish atomically (seqlock): odd -> write payload -> even. */
  adc_snapshot_seq++;
  __DMB();
  adc_snapshot = s;
  __DMB();
  adc_snapshot_seq++;

  /* Staleness watchdog. NDTR steadily decrements while conversions flow; if the
   * sum is unchanged for several ticks the trigger/DMA has stalled. */
  uint32_t ndtr_sum = ADC_SumNDTR();
  if (ndtr_sum != 0 && ndtr_sum == adc_prev_ndtr_sum)
  {
    if (adc_stale_ticks < 0xFFFFu)
      adc_stale_ticks++;
    if (adc_stale_ticks >= ADC_STALE_FAULT_TICKS)
      active_faults |= FAULT_ADC_TIMEOUT;
  }
  else
  {
    adc_stale_ticks = 0;
    active_faults &= ~FAULT_ADC_TIMEOUT;
  }
  adc_prev_ndtr_sum = ndtr_sum;
}

void FEB_ADC_GetCoherentSnapshot(ADC_CoherentSnapshot_t *out)
{
  if (!out)
    return;
  uint32_t s0, s1;
  do
  {
    s0 = adc_snapshot_seq;
    __DMB();
    *out = adc_snapshot;
    __DMB();
    s1 = adc_snapshot_seq;
  } while ((s0 & 1u) || (s0 != s1));
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
      channel_idx = ADC1_CH0_BRAKE_PRESSURE2_IDX;
    else if (channel == ADC_CHANNEL_1)
      channel_idx = ADC1_CH1_BRAKE_PRESSURE1_IDX;
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
    if (channel == ADC_CHANNEL_10)
      channel_idx = ADC3_CH10_BSPD_INDICATOR_IDX;
    else if (channel == ADC_CHANNEL_11)
      channel_idx = ADC3_CH11_BSPD_RESET_IDX;
    else if (channel == ADC_CHANNEL_12)
      channel_idx = ADC3_CH12_ACCEL_PEDAL2_IDX;
    else if (channel == ADC_CHANNEL_13)
      channel_idx = ADC3_CH13_ACCEL_PEDAL1_IDX;
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

float FEB_ADC_RawToVoltage(uint16_t raw_value)
{
  return ((float)raw_value * ADC_VREF_VOLTAGE) / (float)ADC_MAX_VALUE;
}

uint32_t FEB_ADC_RawToMillivolts(uint16_t raw_value)
{
  return (uint32_t)((raw_value * ADC_REFERENCE_VOLTAGE_MV) / ADC_MAX_VALUE);
}

/* Sensor-Specific Raw Functions ---------------------------------------------*/

/* All sensor raw getters return the latest time-coherent snapshot value
 * (boxcar-averaged in FEB_ADC_TickSample), so every consumer in a given control
 * cycle observes the same sampling instant. */

uint16_t FEB_ADC_GetBrakeInputRaw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.brake_input_raw;
}

uint16_t FEB_ADC_GetAccelPedal1Raw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.apps1_raw;
}

uint16_t FEB_ADC_GetAccelPedal2Raw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.apps2_raw;
}

uint16_t FEB_ADC_GetBrakePressure1Raw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.brake1_raw;
}

uint16_t FEB_ADC_GetBrakePressure2Raw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.brake2_raw;
}

uint16_t FEB_ADC_GetCurrentSenseRaw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.current_raw;
}

uint16_t FEB_ADC_GetShutdownInRaw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.shutdown_raw;
}

uint16_t FEB_ADC_GetPreTimingTripRaw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.pretiming_raw;
}

uint16_t FEB_ADC_GetBSPDIndicatorRaw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.bspd_ind_raw;
}

uint16_t FEB_ADC_GetBSPDResetRaw(void)
{
  ADC_CoherentSnapshot_t s;
  FEB_ADC_GetCoherentSnapshot(&s);
  return s.bspd_rst_raw;
}

/* Sensor-Specific Voltage Functions -----------------------------------------*/

/* Voltage getters convert the coherent-snapshot raw value (already boxcar-
 * averaged in FEB_ADC_TickSample) — no per-call filtering. */

float FEB_ADC_GetBrakeInputVoltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetBrakeInputRaw());
}

float FEB_ADC_GetAccelPedal1Voltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetAccelPedal1Raw()) * VOLTAGE_DIVIDER_RATIO_ACCEL1;
}

float FEB_ADC_GetAccelPedal2Voltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetAccelPedal2Raw()) * VOLTAGE_DIVIDER_RATIO_ACCEL2;
}

float FEB_ADC_GetBrakePressure1Voltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetBrakePressure1Raw()) * VOLTAGE_DIVIDER_RATIO_BRAKE;
}

float FEB_ADC_GetBrakePressure2Voltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetBrakePressure2Raw()) * VOLTAGE_DIVIDER_RATIO_BRAKE;
}

float FEB_ADC_GetCurrentSenseVoltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetCurrentSenseRaw()) * VOLTAGE_DIVIDER_RATIO;
}

float FEB_ADC_GetShutdownInVoltage(void)
{
  return FEB_ADC_RawToVoltage(FEB_ADC_GetShutdownInRaw()) * VOLTAGE_DIVIDER_RATIO;
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
      apps_latest_raw_separation = 0.0f;
      ADC_StatsAccumulate(apps_sim_percent, apps_sim_percent, 0.0f);
      ADC_UpdateFaultEdges(active_faults);
      return;
    }
  }

  /* Sensor circuit faults — voltage is post-divider, in mV. Per-sensor floors
   * and ceilings (FSAE T.4.2.10/.13): out-of-range low = short/under-range,
   * out-of-range high = open/over-range. APPS2's 0% sits near 0.4 V so its floor
   * must be lower than APPS1's — hence per-sensor, not a single global value. */
  bool short_circuit = (voltage1 < APPS1_SHORT_CIRCUIT_DETECT_MV) || (voltage2 < APPS2_SHORT_CIRCUIT_DETECT_MV);
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

  /* Map voltage → percent using current calibration and constrain to [0,100].
   * The plausibility deviation below is computed on these constrained-but-NOT-
   * deadzoned positions: ApplyDeadzone rescales [dz,100-dz]->[0,100] (~1.11x
   * gain at dz=5), which would inflate a real channel disagreement and push
   * borderline cases over the 10% limit. Deadzone is applied only to the torque
   * command (acceleration) below, never to the plausibility comparison. */
  float position1 =
      FEB_ADC_MapRange(voltage1, apps1_calibration.min_voltage, apps1_calibration.max_voltage, 0.0f, 100.0f);
  float position2 =
      FEB_ADC_MapRange(voltage2, apps2_calibration.min_voltage, apps2_calibration.max_voltage, 0.0f, 100.0f);
  position1 = FEB_ADC_Constrain(position1, 0.0f, 100.0f);
  position2 = FEB_ADC_Constrain(position2, 0.0f, 100.0f);

  apps_cache.position1 = position1;
  apps_cache.position2 = position2;

  float deviation = fabsf(position1 - position2);
  apps_latest_deviation = deviation;

  if (FEB_APPS_SingleSensorMode)
  {
    /* Bench-mode bypass — only available outside drive state, so we don't
     * gate again here, just skip the dual-sensor logic. */
    apps_cache.position2 = position1;
    apps_cache.acceleration = FEB_ADC_ApplyDeadzone(position1, apps_deadzone_percent);
    apps_cache.plausible = true;
    apps_cache.implausibility_time = 0;
    apps_implaus_start_tick = 0;
  }
  else
  {
    /* FSAE-safe torque source: command from the LOWER of the two sensors so a
     * single high-failing channel can never inflate torque during the up-to-
     * 100 ms window before the deviation latch fires. Deadzone applied here
     * (torque command only), not to the plausibility comparison above. */
    apps_cache.acceleration = FEB_ADC_ApplyDeadzone(fminf(position1, position2), apps_deadzone_percent);

    /* FSAE T.4.2.3 minimum-separation check (line-to-line short detection).
     * The two APPS use different transfer functions, so their RAW pin-domain
     * outputs (raw/ADC_MAX_VALUE*100) are designed to stay well apart (>=~23%
     * of ADC FS on SN5) across the pedal range. If the two signal lines short
     * together, both ADC inputs collapse to ~the same value and that designed
     * separation disappears -> an "other failure defined in T.4.2" =
     * implausibility.
     *
     * Use RAW counts (pre-calibration, pre-clamp, pre-deadzone): the clamp and
     * deadzone can flatten both positions to 0/100 and mask a short. Both APPS
     * share the ADC reference, so a short forces raw1 ~= raw2 wherever on the
     * harness it occurs. The one weak corner — a harness short driven up to
     * APPS1's ceiling — is independently caught by the APPS2 open-circuit
     * detector above. Gate on the MAX of the two positions so either sensor
     * reporting real travel arms the check (the rule applies above 10% pedal). */
    float raw1_pct = ((float)apps_raw1_cached / (float)ADC_MAX_VALUE) * 100.0f;
    float raw2_pct = ((float)apps_raw2_cached / (float)ADC_MAX_VALUE) * 100.0f;
    float raw_separation = fabsf(raw1_pct - raw2_pct);
    apps_latest_raw_separation = raw_separation;

    bool above_gate = (fmaxf(position1, position2) > (float)APPS_SEPARATION_PEDAL_GATE_PERCENT);
    bool too_close = above_gate && (raw_separation < (float)APPS_MIN_SEPARATION_PERCENT);

    /* FSAE T.4.2.3 / T.4.2.4: implausible = persistent >10% disagreement OR a
     * collapsed raw separation, sustained for >100 ms. */
    bool in_disagreement = (deviation > (float)APPS_PLAUSIBILITY_TOLERANCE);
    bool implausible_now = in_disagreement || too_close;

    if (implausible_now)
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
          LOG_E(TAG_ADC, "APPS implausibility latched (%.1fms, dev=%.1f%%, rawsep=%.1f%%, gate=%d)", (double)elapsed,
                (double)deviation, (double)raw_separation, above_gate);
        }
        active_faults |= FAULT_APPS_IMPLAUSIBILITY;
        if (too_close)
          active_faults |= FAULT_APPS_SIGNAL_SHORT; /* sub-cause: distinguish a wiring short in logs/faults */
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
  /* Release-and-reset must not fire while a hardware circuit fault is still
   * asserted: a noisy short/open can intermittently map both channels below 5%
   * and "look like" a released pedal, clearing the latch and reopening a fresh
   * 100 ms drive window. Require the pedal low AND no live short/open circuit. */
  if (apps_cache.position1 < APPS_RELEASE_PERCENT && apps_cache.position2 < APPS_RELEASE_PERCENT &&
      !apps_cache.short_circuit && !apps_cache.open_circuit)
  {
    if (active_faults & FAULT_APPS_IMPLAUSIBILITY)
    {
      LOG_I(TAG_ADC, "APPS plausibility cleared on release");
    }
    active_faults &= ~(FAULT_APPS_IMPLAUSIBILITY | FAULT_APPS_SIGNAL_SHORT);
    apps_implaus_start_tick = 0;
    apps_cache.plausible = true;
    apps_cache.implausibility_time = 0;
  }
}

void FEB_ADC_AcknowledgeBrakeImplausibility(void)
{
  /* FSAE EV.4.7.2.b: once the APPS/brake plausibility shutdown latches, it must
   * stay active until the APPS signals less than 5% pedal travel, WITH OR
   * WITHOUT brake operation. Gate the release on APPS travel, NOT on brake
   * release (a driver lifting off the brake while still on the throttle must
   * not restore torque). */
  if (apps_cache.acceleration < APPS_RELEASE_PERCENT)
  {
    if (active_faults & FAULT_BRAKE_PLAUSIBILITY)
    {
      LOG_I(TAG_ADC, "EV.4.7 brake/APPS plausibility cleared (APPS < %.0f%%)", (double)APPS_RELEASE_PERCENT);
    }
    active_faults &= ~FAULT_BRAKE_PLAUSIBILITY;
    adc_runtime.brake_plausibility_timer = 0;
    ev47_start_tick = 0;
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

  /* Calculate brake position based on pressure (the selected sensor). */
  brake_data->brake_position = brake_data->brake_switch ? brake_data->pressure2_percent : brake_data->pressure1_percent;

  /* Brake is "pressed" when the combined position exceeds the threshold — derived
   * from the pressure sensors, not the separate PC4 brake-input line. */
  brake_data->brake_pressed = (brake_data->brake_position > BRAKE_PRESSED_POSITION_PERCENT);

  /* Check plausibility between pressure sensors. Both inputs are percentages, so
   * the threshold must be a percentage too. A latched BSE open/short sensor fault
   * (T.4.3.4/.5, set in FEB_ADC_TickBrakeFaults) also makes the brake implausible
   * so every consumer cuts torque consistently. */
  float pressure_diff = fabsf(brake_data->pressure1_percent - brake_data->pressure2_percent);
  brake_data->plausible = (pressure_diff <= BRAKE_PRESSURE_PLAUSIBILITY_TOLERANCE_PERCENT) &&
                          ((active_faults & FAULT_BRAKE_SENSOR_FAULT) == 0);

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
  brake_pressure1_calibration.min_voltage = BRAKE_PRESSURE_1_MIN_MV;
  brake_pressure1_calibration.max_voltage = BRAKE_PRESSURE_1_MAX_MV;
  brake_pressure1_calibration.min_physical = BRAKE_PRESSURE_MIN_PHYSICAL_BAR;
  brake_pressure1_calibration.max_physical = BRAKE_PRESSURE_MAX_PHYSICAL_BAR;

  brake_pressure2_calibration.min_voltage = BRAKE_PRESSURE_2_MIN_MV;
  brake_pressure2_calibration.max_voltage = BRAKE_PRESSURE_2_MAX_MV;
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

void FEB_ADC_SetBrakeBypass(bool enabled)
{
  /* Mirrors the FEB_RMS bench brake bypass. When set, FEB_ADC_TickBrakeFaults
   * skips BSE open/short and EV.4.7 detection so a deliberately-disconnected
   * brake input on the bench can't latch a fault. */
  brake_bypass_enabled = enabled;
}

void FEB_ADC_TickBrakeFaults(void)
{
  uint32_t now = HAL_GetTick();

  if (brake_bypass_enabled)
  {
    /* Bench bypass: don't detect, and don't let a prior latch persist. */
    bse_fault_start_tick = 0;
    ev47_start_tick = 0;
    active_faults &= ~(FAULT_BRAKE_SENSOR_FAULT | FAULT_BRAKE_PLAUSIBILITY);
    ADC_UpdateFaultEdges(active_faults);
    return;
  }

  /* --- FSAE T.4.3.4/.5: BSE sensor open/short, latched after 100 ms. A lost
   * shared 5 V supply reads both sensors ~0 mV (< UNDER), which would otherwise
   * look like "no brake" and silently allow torque + disable BSPD. --- */
  float bp1_mv = FEB_ADC_GetBrakePressure1Voltage() * 1000.0f;
  float bp2_mv = FEB_ADC_GetBrakePressure2Voltage() * 1000.0f;
  bool bse_bad = (bp1_mv < BRAKE_PRESSURE_1_UNDER_MV) || (bp1_mv > BRAKE_PRESSURE_1_OVER_MV) ||
                 (bp2_mv < BRAKE_PRESSURE_2_UNDER_MV) || (bp2_mv > BRAKE_PRESSURE_2_OVER_MV);
  if (bse_bad)
  {
    if (bse_fault_start_tick == 0)
    {
      bse_fault_start_tick = now;
      if (bse_fault_start_tick == 0)
        bse_fault_start_tick = 1; /* sentinel: 0 means "not started" */
    }
    if ((now - bse_fault_start_tick) > BRAKE_PLAUSIBILITY_TIME_MS)
    {
      if ((active_faults & FAULT_BRAKE_SENSOR_FAULT) == 0 && (now - last_bse_log_tick) >= APPS_LOG_RATE_LIMIT_MS)
      {
        last_bse_log_tick = now;
        LOG_E(TAG_ADC, "BSE sensor fault (open/short): P1=%.0fmV P2=%.0fmV", (double)bp1_mv, (double)bp2_mv);
      }
      active_faults |= FAULT_BRAKE_SENSOR_FAULT;
    }
  }
  else
  {
    /* Self-healing hardware fault: clear once both sensors are back in range. */
    bse_fault_start_tick = 0;
    active_faults &= ~FAULT_BRAKE_SENSOR_FAULT;
  }

  /* --- FSAE EV.4.7: brakes engaged + APPS > 25% must stop motor power
   * effectively immediately (only a short debounce, NOT the 100 ms T.4.x
   * grace). The latch is cleared by FEB_ADC_AcknowledgeBrakeImplausibility once
   * APPS < 5%, with or without brake (EV.4.7.2.b). --- */
  Brake_DataTypeDef brake_data;
  bool brake_engaged = false;
  if (FEB_ADC_GetBrakeData(&brake_data) == ADC_STATUS_OK)
  {
    brake_engaged = (brake_data.pressure1_percent > BRAKE_PRESSURE_THRESHOLD_PERCENT) ||
                    (brake_data.pressure2_percent > BRAKE_PRESSURE_THRESHOLD_PERCENT);
  }
  bool throttle_high = (apps_cache.acceleration > 25.0f);

  if (brake_engaged && throttle_high)
  {
    if (ev47_start_tick == 0)
    {
      ev47_start_tick = now;
      if (ev47_start_tick == 0)
        ev47_start_tick = 1; /* sentinel */
    }
    if ((now - ev47_start_tick) > EV47_PLAUSIBILITY_DEBOUNCE_MS)
    {
      if ((active_faults & FAULT_BRAKE_PLAUSIBILITY) == 0)
      {
        LOG_E(TAG_ADC, "EV.4.7 fault: brake engaged + APPS >25%% - cutting power");
      }
      active_faults |= FAULT_BRAKE_PLAUSIBILITY;
    }
  }
  else
  {
    /* Conditions no longer co-occur: reset the debounce. The latch itself
     * persists until APPS < 5% (FEB_ADC_AcknowledgeBrakeImplausibility). */
    ev47_start_tick = 0;
  }

  ADC_UpdateFaultEdges(active_faults);
}

bool FEB_ADC_CheckBrakePlausibility(void)
{
  /* Detection + latching now live in FEB_ADC_TickBrakeFaults() (1 ms tick) so
   * the EV.4.7 shutdown is effectively immediate. This entry point only reports
   * the latched state, mirroring FEB_ADC_CheckAPPSPlausibility(). */
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
    active_faults &= ~FAULT_APPS_SIGNAL_SHORT; /* sub-cause tracks the implausibility latch */
    adc_runtime.apps_implausibility_timer = 0;
    apps_implaus_start_tick = 0;
    apps_cache.plausible = true;
    apps_cache.implausibility_time = 0;
  }
  if (fault_mask & FAULT_BRAKE_PLAUSIBILITY)
  {
    adc_runtime.brake_plausibility_timer = 0;
    ev47_start_tick = 0;
  }
  if (fault_mask & FAULT_BRAKE_SENSOR_FAULT)
  {
    bse_fault_start_tick = 0;
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
  case FAULT_APPS_SIGNAL_SHORT:
    return 8;
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
    {"APPS_SIGNAL_SHORT", FAULT_APPS_SIGNAL_SHORT},
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

void FEB_ADC_GetAPPSFilterConfig(bool *enabled, uint8_t *samples)
{
  if (enabled)
    *enabled = accel_pedal1_config.filter.enabled ? true : false;
  if (samples)
    *samples = accel_pedal1_config.filter.samples;
}

ADC_StatusTypeDef FEB_ADC_SetAPPSFilter(bool enabled, uint8_t samples)
{
  if (samples < 1)
    samples = 1;
  if (samples > ADC_DMA_BUFFER_SIZE)
    samples = ADC_DMA_BUFFER_SIZE;
  /* Both APPS channels share one boxcar window so group delay stays symmetric. */
  accel_pedal1_config.filter.enabled = enabled ? 1 : 0;
  accel_pedal1_config.filter.samples = samples;
  accel_pedal2_config.filter.enabled = enabled ? 1 : 0;
  accel_pedal2_config.filter.samples = samples;
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

float FEB_ADC_GetAPPSRawSeparation(void)
{
  return apps_latest_raw_separation;
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
