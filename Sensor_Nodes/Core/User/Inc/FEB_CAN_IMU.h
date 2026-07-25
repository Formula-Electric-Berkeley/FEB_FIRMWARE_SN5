/**
 ******************************************************************************
 * @file           : FEB_CAN_IMU.h
 * @brief          : CAN IMU Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_IMU_H
#define FEB_CAN_IMU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

  /* ============================================================================
   * API Functions
   * ============================================================================ */

  /**
   * @brief Process IMU data transmissions
   *
   * Packs and transmits acceleration and gyroscope data over CAN.
   */

  void FEB_CAN_IMU_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_IMU_H */
