#include "FEB_P_Sense.h"
#include "main.h"
#include "feb_log.h"
#include "feb_can_lib.h"

extern osMessageQueueId_t canTxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

extern osMessageQueueId_t StateHandle;

extern ADC_HandleTypeDef hadc1;

static DSB_Status status = OFF;

DSB_Status get_status()
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

  uint16_t sense_buf[2];

  // Calibrate ADC
  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&sense_buf, 2);

  /*
  TODO: DEFINE CAN TX sender here:

  */

  uint16_t p_sense_1;
  uint16_t p_sense_2;

  for (;;)
  {
    p_sense_1 = sense_buf[0];
    p_sense_2 = sense_buf[1];

    // TODO: Define state update

    osDelay(10); // 100Hz
  }
}