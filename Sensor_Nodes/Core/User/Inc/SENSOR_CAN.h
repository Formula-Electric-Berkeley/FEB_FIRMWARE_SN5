#ifndef SENSOR_CAN_H
#define SENSOR_CAN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void SENSOR_CAN_Init(void);
  bool SENSOR_CAN_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_CAN_H */
