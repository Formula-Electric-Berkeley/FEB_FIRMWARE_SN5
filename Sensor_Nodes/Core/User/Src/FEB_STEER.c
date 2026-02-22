#include "FEB_STEER.h"

static TIM_HandleTypeDef *s_tim = NULL;
static int32_t s_count = 0;
static uint16_t s_last_counter = 0U;
static bool s_started = false;

void FEB_STEER_Init(TIM_HandleTypeDef *htim)
{
  s_tim = htim;
  s_count = 0;
  s_last_counter = 0U;
  s_started = false;
}

HAL_StatusTypeDef FEB_STEER_Start(void)
{
  if (s_tim == NULL)
  {
    return HAL_ERROR;
  }

  if (HAL_TIM_Encoder_Start(s_tim, TIM_CHANNEL_ALL) != HAL_OK)
  {
    return HAL_ERROR;
  }

  s_last_counter = (uint16_t)__HAL_TIM_GET_COUNTER(s_tim);
  s_started = true;
  return HAL_OK;
}

void FEB_STEER_Update(void)
{
  uint16_t current;
  int16_t delta;

  if ((s_tim == NULL) || !s_started)
  {
    return;
  }

  current = (uint16_t)__HAL_TIM_GET_COUNTER(s_tim);
  delta = (int16_t)(current - s_last_counter);
  s_last_counter = current;
  s_count += delta;
}

void FEB_STEER_SetZero(void)
{
  s_count = 0;
}

bool FEB_STEER_GetData(FEB_STEER_Data_t *out_data)
{
  if (out_data == NULL)
  {
    return false;
  }

  out_data->count = s_count;
  out_data->angle_raw = (int16_t)s_count;
  return true;
}

void FEB_STEER_PackCanPayload(const FEB_STEER_Data_t *data, uint32_t can_counter, uint16_t flags, uint8_t out_payload[8])
{
  int16_t angle;

  if ((data == NULL) || (out_payload == NULL))
  {
    return;
  }

  angle = data->angle_raw;

  out_payload[0] = (uint8_t)(can_counter & 0xFFU);
  out_payload[1] = (uint8_t)((can_counter >> 8) & 0xFFU);
  out_payload[2] = (uint8_t)((can_counter >> 16) & 0xFFU);
  out_payload[3] = (uint8_t)((can_counter >> 24) & 0xFFU);
  out_payload[4] = (uint8_t)(flags & 0xFFU);
  out_payload[5] = (uint8_t)((flags >> 8) & 0xFFU);
  out_payload[6] = (uint8_t)(angle & 0xFF);
  out_payload[7] = (uint8_t)((uint16_t)angle >> 8);
}
