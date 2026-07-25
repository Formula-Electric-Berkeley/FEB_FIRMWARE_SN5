/**
 ******************************************************************************
 * @file           : FEB_CAN_LVPDB.h
 * @brief          : CAN LVPDB Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_LVPDB_H
#define FEB_CAN_LVPDB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  void FEB_CAN_LVPDB_Init(void);
  uint16_t FEB_CAN_LVPDB_GetLast24VVoltage(void);
  uint16_t FEB_CAN_LVPDB_GetLast12VVoltage(void);
  bool FEB_CAN_LVPDB_IsDataFresh(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_LVPDB_H */
