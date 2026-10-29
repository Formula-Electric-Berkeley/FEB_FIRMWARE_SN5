#include "FEB_P_Sense.h"
#include "FEB_ARC_State.h"
#include "main.h"
#include "cmsis_os.h"
#include "feb_log.h"

extern ADC_HandleTypeDef hadc1;

static DSB_Status status = DSB_STATUS_OFF;

DSB_Status get_status(void)
{
  return status;
}

void set_status(DSB_Status s)
{
  status = s;
}

void START_P_SENSE(void const *argument)
{

  LOG_I(TAG_P_SENSE, "%s", "P_Sense Task Started Successfully");

  uint16_t p_sense_1 = 0;
  uint16_t p_sense_2 = 0;

  for (;;)
  {
    ADC_ChannelConfTypeDef channel_config = {
        .Rank = 1,
        .SamplingTime = ADC_SAMPLETIME_84CYCLES,
    };

    channel_config.Channel = ADC_CHANNEL_4;
    HAL_StatusTypeDef status_1 = HAL_ADC_ConfigChannel(&hadc1, &channel_config);
    if (status_1 == HAL_OK)
    {
      status_1 = HAL_ADC_Start(&hadc1);
    }
    if (status_1 == HAL_OK)
    {
      status_1 = HAL_ADC_PollForConversion(&hadc1, 10);
    }
    if (status_1 == HAL_OK)
    {
      p_sense_1 = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    channel_config.Channel = ADC_CHANNEL_5;
    HAL_StatusTypeDef status_2 = HAL_ADC_ConfigChannel(&hadc1, &channel_config);
    if (status_2 == HAL_OK)
    {
      status_2 = HAL_ADC_Start(&hadc1);
    }
    if (status_2 == HAL_OK)
    {
      status_2 = HAL_ADC_PollForConversion(&hadc1, 10);
    }
    if (status_2 == HAL_OK)
    {
      p_sense_2 = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    if (status_1 != HAL_OK || status_2 != HAL_OK)
    {
      if (status != DSB_STATUS_NOT_READY)
      {
        LOG_E(TAG_P_SENSE, "%s", "ADC pressure-sensor conversion failed");
      }
      set_status(DSB_STATUS_NOT_READY);
    }
    else
    {
      set_status(DSB_STATUS_READY);
    }

    // TODO: Define state update
    (void)p_sense_1;
    (void)p_sense_2;

    osDelay(10); // 100Hz
  }
}