#ifndef FEB_STEER_H
#define FEB_STEER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

typedef struct
{
  int32_t count;
  int16_t angle_raw;
} FEB_STEER_Data_t;

void FEB_STEER_Init(TIM_HandleTypeDef *htim);
HAL_StatusTypeDef FEB_STEER_Start(void);
void FEB_STEER_Update(void);
void FEB_STEER_SetZero(void);
bool FEB_STEER_GetData(FEB_STEER_Data_t *out_data);
void FEB_STEER_PackCanPayload(const FEB_STEER_Data_t *data, uint32_t can_counter, uint16_t flags, uint8_t out_payload[8]);

#ifdef __cplusplus
}
#endif

#endif /* FEB_STEER_H */
