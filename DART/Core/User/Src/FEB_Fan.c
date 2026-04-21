// ********************************** Includes & External **********************************
#include "FEB_Fan.h"
#include "FEB_CAN_Library_SN4/gen/feb_can.h"
#include "main.h"
#include "stm32f0xx_hal_gpio.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim14;
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;

extern UART_HandleTypeDef huart2;

// ********************************** Global Variables **********************************

uint16_t frequency[NUM_FANS] = {0, 0, 0, 0, 0};

static uint32_t filter[NUM_FANS];
static bool filter_init[NUM_FANS];

static uint32_t IC_first_rising_edge[NUM_FANS] = {0, 0, 0, 0, 0};
static uint32_t IC_second_rising_edge[NUM_FANS] = {0, 0, 0, 0, 0};
static bool first_capture[NUM_FANS] = {false, false, false, false, false};

static uint32_t last_bms_rx_ms = 0;
static int16_t last_max_cell_temp = 0;
static bool manual_override = false;
static uint8_t commanded_percent[NUM_FANS] = {0, 0, 0, 0, 0};

static inline uint32_t percent_to_counts(uint8_t percent)
{
  if (percent > 100)
  {
    percent = 100;
  }
  return (uint32_t)PWM_COUNTER * percent / 100u;
}

static TIM_HandleTypeDef *pwm_timer[NUM_FANS] = {&htim1, &htim1, &htim1, &htim3, &htim3};
static uint32_t pwm_channels[NUM_FANS] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_2, TIM_CHANNEL_1};

static TIM_HandleTypeDef *tach_timer[NUM_FANS] = {&htim14, &htim16, &htim17, &htim2, &htim2};
static uint32_t tach_channels[NUM_FANS] = {TIM_CHANNEL_1, TIM_CHANNEL_1, TIM_CHANNEL_1, TIM_CHANNEL_1, TIM_CHANNEL_2};
static uint32_t tach_active_channels[NUM_FANS] = {HAL_TIM_ACTIVE_CHANNEL_1, HAL_TIM_ACTIVE_CHANNEL_1,
                                                  HAL_TIM_ACTIVE_CHANNEL_1, HAL_TIM_ACTIVE_CHANNEL_1,
                                                  HAL_TIM_ACTIVE_CHANNEL_2};

// ********************************** Static Function Prototypes **********************************

static void FEB_TACH_IIR(uint16_t *data_in, uint16_t *data_out, uint32_t *filters, uint8_t length,
                         bool *filter_initialized);

// ********************************** Initialize **********************************

void FEB_Fan_Init(void)
{
  FEB_Fan_PWM_Init();
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    commanded_percent[i] = (uint8_t)(PWM_START_PERCENT * 100.0f);
  }
  FEB_Fan_All_Speed_Set((uint32_t)(PWM_COUNTER * PWM_START_PERCENT)); // starts at 100% duty cycle
  FEB_Fan_TACH_Init();
}

// ********************************** CAN **********************************

void FEB_Fan_CAN_Msg_Process(uint8_t *FEB_CAN_Rx_Data)
{
  struct feb_can_bms_accumulator_temperature_t msg;
  if (feb_can_bms_accumulator_temperature_unpack(&msg, FEB_CAN_Rx_Data, FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_LENGTH) !=
      0)
  {
    return;
  }

  last_bms_rx_ms = HAL_GetTick();
  last_max_cell_temp = msg.max_cell_temperature;

  if (manual_override)
  {
    return;
  }

  float fanPercent = 0.0f;
  if (last_max_cell_temp > TEMP_START_FAN)
  {
    float span = (float)(TEMP_END_FAN - TEMP_START_FAN);
    fanPercent = (last_max_cell_temp - TEMP_START_FAN) / span;
    if (fanPercent > 1.0f)
    {
      fanPercent = 1.0f;
    }
  }

  uint8_t pct = (uint8_t)(fanPercent * 100.0f);
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    commanded_percent[i] = pct;
  }
  FEB_Fan_All_Speed_Set((uint32_t)(PWM_COUNTER * fanPercent));
}

void FEB_Fan_Watchdog_Tick(void)
{
  if (manual_override)
  {
    return;
  }
  if (HAL_GetTick() - last_bms_rx_ms > BMS_RX_TIMEOUT_MS)
  {
    for (size_t i = 0; i < NUM_FANS; ++i)
    {
      commanded_percent[i] = 100;
    }
    FEB_Fan_All_Speed_Set(PWM_COUNTER);
  }
}

void FEB_Fan_SetManualOverride(bool enable, uint8_t percent)
{
  manual_override = enable;
  if (enable)
  {
    if (percent > 100)
    {
      percent = 100;
    }
    for (size_t i = 0; i < NUM_FANS; ++i)
    {
      commanded_percent[i] = percent;
    }
    FEB_Fan_All_Speed_Set(percent_to_counts(percent));
  }
}

void FEB_Fan_SetManualFan(uint8_t fan_idx, uint8_t percent)
{
  if (fan_idx >= NUM_FANS)
  {
    return;
  }
  if (percent > 100)
  {
    percent = 100;
  }
  manual_override = true;
  commanded_percent[fan_idx] = percent;
  FEB_Fan_Speed_Set(fan_idx, percent_to_counts(percent));
}

uint8_t FEB_Fan_GetCommandedPercent(uint8_t fan_idx)
{
  if (fan_idx >= NUM_FANS)
  {
    return 0;
  }
  return commanded_percent[fan_idx];
}

uint32_t FEB_Fan_GetCommandedCounts(uint8_t fan_idx)
{
  if (fan_idx >= NUM_FANS)
  {
    return 0;
  }
  return percent_to_counts(commanded_percent[fan_idx]);
}

int16_t FEB_Fan_GetLastMaxCellTemp(void)
{
  return last_max_cell_temp;
}

uint32_t FEB_Fan_GetStalenessMs(void)
{
  return HAL_GetTick() - last_bms_rx_ms;
}

bool FEB_Fan_IsManualOverride(void)
{
  return manual_override;
}

// ********************************** PWM **********************************

void FEB_Fan_PWM_Init(void)
{
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    HAL_TIM_PWM_Start(pwm_timer[i], pwm_channels[i]);
  }
}

void FEB_Fan_All_Speed_Set(uint32_t speed)
{
  if (speed > PWM_COUNTER)
  {
    speed = PWM_COUNTER;
  }
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    __HAL_TIM_SET_COMPARE(pwm_timer[i], pwm_channels[i], speed);
  }
}

void FEB_Fan_Speed_Set(uint8_t fan_idx, uint32_t speed)
{
  if (fan_idx >= NUM_FANS)
  {
    return;
  }
  if (speed > PWM_COUNTER)
  {
    speed = PWM_COUNTER;
  }
  __HAL_TIM_SET_COMPARE(pwm_timer[fan_idx], pwm_channels[fan_idx], speed);
}

// ********************************** TACH **********************************

void FEB_Fan_TACH_Init(void)
{
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    HAL_TIM_IC_Start_IT(tach_timer[i], tach_channels[i]);
  }
}

void FEB_Fan_TACH_Callback(TIM_HandleTypeDef *htim)
{

  for (size_t i = 0; i < NUM_FANS; ++i)
  {

    if (tach_timer[i] == htim)
    {

      if (htim->Channel == tach_active_channels[i])
      {

        if (first_capture[i] == false)
        {

          IC_first_rising_edge[i] = HAL_TIM_ReadCapturedValue(htim, tach_channels[i]);
          first_capture[i] = true;
        }

        else
        {
          uint32_t diff = 0;

          IC_second_rising_edge[i] = HAL_TIM_ReadCapturedValue(htim, tach_channels[i]);

          if (IC_second_rising_edge[i] > IC_first_rising_edge[i])
          {

            diff = IC_second_rising_edge[i] - IC_first_rising_edge[i];
          }

          else if (IC_first_rising_edge[i] > IC_second_rising_edge[i])
          {

            diff = (0xFFFFFFFF - IC_first_rising_edge[i]) + IC_second_rising_edge[i];
          }

          else
          {
            frequency[i] = 0;
            return;
          }

          frequency[i] = REF_CLOCK / diff;

          FEB_TACH_IIR(frequency, frequency, filter, NUM_FANS, filter_init);

          IC_first_rising_edge[i] = IC_second_rising_edge[i];
        }
      }
    }
  }
}

static void FEB_TACH_IIR(uint16_t *data_in, uint16_t *data_out, uint32_t *filters, uint8_t length,
                         bool *filter_initialized)
{
  uint16_t *dest = data_out;
  uint32_t *dest_filters = filters;

  for (uint8_t i = 0; i < length; i++)
  {

    if (!filter_initialized[i])
    {
      dest_filters[i] = data_in[i] << TACH_FILTER_EXPONENT;
      dest[i] = data_in[i];
      filter_initialized[i] = true;
    }

    else
    {
      dest_filters[i] += data_in[i] - (dest_filters[i] >> TACH_FILTER_EXPONENT);
      dest[i] = dest_filters[i] >> TACH_FILTER_EXPONENT;
    }
  }
}
