/**
 ******************************************************************************
 * @file           : DCU_CAN.h
 * @brief          : CAN initialization for DCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef DCU_CAN_H
#define DCU_CAN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

  /**
   * @brief Initialize CAN with accept-all filter
   *
   * Configures both CAN1 and CAN2 with filters that accept all messages.
   * Messages are received but not processed (no handlers registered).
   *
   * @return true if successful, false otherwise
   */
  bool DCU_CAN_Init(void);

  /**
   * @brief Get CAN initialization status
   *
   * @return true if CAN has been successfully initialized
   */
  bool DCU_CAN_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* DCU_CAN_H */
