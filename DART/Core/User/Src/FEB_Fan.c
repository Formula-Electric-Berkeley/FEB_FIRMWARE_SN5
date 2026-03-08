// ********************************** Includes & External **********************************
#include "FEB_Fan.h"

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

static TIM_HandleTypeDef *timer[NUM_FANS] = {&htim14, &htim16, &htim17, &htim2, &htim2};
static uint32_t tim_active_channels[NUM_FANS] = {HAL_TIM_ACTIVE_CHANNEL_1, HAL_TIM_ACTIVE_CHANNEL_1,
                                                 HAL_TIM_ACTIVE_CHANNEL_1, HAL_TIM_ACTIVE_CHANNEL_1,
                                                 HAL_TIM_ACTIVE_CHANNEL_2};
static uint32_t tim_channels[NUM_FANS] = {TIM_CHANNEL_1, TIM_CHANNEL_1, TIM_CHANNEL_1, TIM_CHANNEL_1, TIM_CHANNEL_2};

// ********************************** Static Function Prototypes **********************************

static void FEB_TACH_IIR(uint16_t *data_in, uint16_t *data_out, uint32_t *filters, uint8_t length,
                         bool *filter_initialized);

// ********************************** Initialize **********************************

void FEB_Fan_Init(void)
{
  FEB_Fan_PWM_Init();
  FEB_Fan_All_Speed_Set((uint8_t)(PWM_COUNTER * PWM_START_PERCENT)); // starts at 100% duty cycle
  FEB_Fan_TACH_Init();
}

// ********************************** CAN **********************************

void FEB_Fan_CAN_Msg_Process(uint8_t *FEB_CAN_Rx_Data)
{
  float fanPercent = 0;

  int16_t temp = (uint16_t)(FEB_CAN_Rx_Data[4]) << 8;
  temp = temp || FEB_CAN_Rx_Data[5];

  if (temp - TEMP_START_FAN > 0)
  {
    fanPercent = ((temp - TEMP_START_FAN) / ((float)TEMP_END_FAN - TEMP_START_FAN) > 1)
                     ? 1
                     : (temp - TEMP_START_FAN) / ((float)TEMP_END_FAN - TEMP_START_FAN);
  }

  FEB_Fan_All_Speed_Set(PWM_COUNTER * fanPercent);

  //	__HAL_TIM_SET_COMPARE(timer[0], tim_channels[0], FEB_CAN_Rx_Data[0]);
  //	__HAL_TIM_SET_COMPARE(timer[1], tim_channels[1], FEB_CAN_Rx_Data[1]);
  //	__HAL_TIM_SET_COMPARE(timer[2], tim_channels[2], FEB_CAN_Rx_Data[2]);
  //	__HAL_TIM_SET_COMPARE(timer[3], tim_channels[3], FEB_CAN_Rx_Data[3]);
  //	__HAL_TIM_SET_COMPARE(timer[4], tim_channels[4], FEB_CAN_Rx_Data[4]);
}

// ********************************** PWM **********************************

void FEB_Fan_PWM_Init(void)
{
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    HAL_TIM_PWM_Start(timer[i], tim_channels[i]);
  }
}

void FEB_Fan_All_Speed_Set(uint8_t speed)
{
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    __HAL_TIM_SET_COMPARE(timer[i], tim_channels[i], speed);
  }
}

// ********************************** TACH **********************************

void FEB_Fan_TACH_Init(void)
{
  for (size_t i = 0; i < NUM_FANS; ++i)
  {
    HAL_TIM_IC_Start_IT(timer[i], tim_channels[i]);
  }
}

void FEB_Fan_TACH_Callback(TIM_HandleTypeDef *htim)
{

  for (size_t i = 0; i < NUM_FANS; ++i)
  {

    if (timer[i] == htim)
    {

      if (htim->Channel == tim_active_channels[i])
      {

        if (first_capture[i] == false)
        {

          IC_first_rising_edge[i] = HAL_TIM_ReadCapturedValue(htim, tim_channels[i]);
          first_capture[i] = true;
        }

        else
        {
          uint32_t diff = 0;

          IC_second_rising_edge[i] = HAL_TIM_ReadCapturedValue(htim, tim_channels[i]);

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

#if PRINT_DEBUG_TACH

          char str[512];

          sprintf(str, "Fan %u: %u\n\r", i + 1, frequency[i]);

          HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);

          IC_first_rising_edge[i] = IC_second_rising_edge[i];

#endif
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
      dest_filters[i] = data_in[i] << ADC_FILTER_EXPONENT;
      dest[i] = data_in[i];
      filter_initialized[i] = true;
    }

    else
    {
      dest_filters[i] += data_in[i] - (dest_filters[i] >> ADC_FILTER_EXPONENT);
      dest[i] = dest_filters[i] >> ADC_FILTER_EXPONENT;
    }
  }
}
