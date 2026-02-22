#ifndef FEB_GPS_H
#define FEB_GPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define FEB_GPS_LINE_MAX_LEN 128U

typedef struct
{
  float latitude_deg;
  float longitude_deg;
  uint8_t valid;
} FEB_GPS_Fix_t;

void FEB_GPS_Init(UART_HandleTypeDef *huart, GPIO_TypeDef *en_port, uint16_t en_pin);
void FEB_GPS_SetPower(bool enable);
HAL_StatusTypeDef FEB_GPS_Start(void);
void FEB_GPS_UART_RxCpltCallback(UART_HandleTypeDef *huart);
bool FEB_GPS_ReadLine(char *out_line, uint16_t out_size);
bool FEB_GPS_ProcessLine(const char *nmea_line);
bool FEB_GPS_ProcessPendingLine(char *out_line, uint16_t out_size);
bool FEB_GPS_GetLastFix(FEB_GPS_Fix_t *out_fix);
void FEB_GPS_FixToBytes(const FEB_GPS_Fix_t *fix, uint8_t out_8bytes[8]);

#ifdef __cplusplus
}
#endif

#endif /* FEB_GPS_H */
