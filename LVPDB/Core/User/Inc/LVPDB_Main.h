/**
 ******************************************************************************
 * @file           : LVPDB_Main.h
 * @brief          : LVPDB Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef LVPDB_MAIN_H
#define LVPDB_MAIN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FEB_BREAK_THRESHOLD (uint8_t)20

#define LV_INDEX (uint8_t)0
#define SH_INDEX (uint8_t)1
#define AF1_AF2_INDEX (uint8_t)2
#define CP_INDEX (uint8_t)3
#define RF_INDEX (uint8_t)4

#define SLEEP_TIME 10

  void LVPDB_Init(void);

  bool LVPDB_IsSetupComplete(void); // to confirm if LVPDB.init() executed without err.

#ifdef __cplusplus
}
#endif

#endif /* LVPDB_MAIN_H */
