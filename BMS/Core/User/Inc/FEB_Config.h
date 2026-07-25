/**
 * @file FEB_Config.h
 * @brief BMS Configuration Getters
 * @note Hardcoded configuration values for initial development. Cell V/T
 *       validation limits are NOT served from here anymore — they live in the
 *       runtime-switchable NORMAL/CHARGING profile table in FEB_ADBMS6830B.c.
 */

#ifndef FEB_CONFIG_H
#define FEB_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "FEB_Const.h"
#include "main.h"

  /**
   * @brief Get cell balancing voltage threshold
   * @return Threshold in millivolts (mV)
   */
  static inline uint16_t FEB_Config_Get_Balance_Threshold_mV(void)
  {
    return FEB_CELL_BALANCE_THRESHOLD_MV;
  }

#ifdef __cplusplus
}
#endif

#endif // FEB_CONFIG_H
