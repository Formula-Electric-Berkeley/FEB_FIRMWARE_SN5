/**
 ******************************************************************************
 * @file           : DCU_TPS.h
 * @brief          : TPS2482 power monitoring for DCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef DCU_TPS_H
#define DCU_TPS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  /**
   * @brief TPS measurement data structure
   */
  typedef struct
  {
    uint16_t bus_voltage_mv;  /**< Bus voltage in millivolts */
    int16_t current_ma;       /**< Current in milliamps (negative = reverse) */
    int32_t shunt_voltage_uv; /**< Shunt voltage in microvolts */
    bool valid;               /**< true if measurements are valid */
  } DCU_TPS_Data_t;

  /**
   * @brief Initialize TPS subsystem
   */
  void DCU_TPS_Init(void);

  /**
   * @brief Update TPS measurements (call at ~10Hz from main loop)
   */
  void DCU_TPS_Update(void);

  /**
   * @brief Get latest TPS data for console display
   *
   * @param data Pointer to structure to fill with TPS data
   */
  void DCU_TPS_GetData(DCU_TPS_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* DCU_TPS_H */
