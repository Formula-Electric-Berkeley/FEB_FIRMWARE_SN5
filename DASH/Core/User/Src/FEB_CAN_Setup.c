/**
 * @file FEB_CAN_Setup.c
 * @brief CAN System Initialization Entry Point
 *
 * Following PCU's modular pattern: each subsystem file has its own Init() function
 * that registers its callbacks. This file just calls all those Init functions.
 *
 * Call FEB_CAN_Setup() once at startup (after HAL_CAN_Init) to initialize the
 * complete CAN system.
 */

// ============================================================================
// INCLUDES
// ============================================================================

#include "FEB_CAN_Setup.h"
#include "FEB_CAN_TX.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_DASH.h"
#include "FEB_CAN_PCU.h"

// ============================================================================
// CAN SYSTEM INITIALIZATION
// ============================================================================

/**
 * @brief Initialize complete CAN system
 *
 * Starts CAN hardware and calls each subsystem's Init function.
 * Each subsystem registers its own callbacks internally.
 *
 * Call once at startup in main.c USER CODE BEGIN 2 section.
 *
 * @return FEB_CAN_Status_t FEB_CAN_OK if successful
 */
FEB_CAN_Status_t FEB_CAN_Setup(void)
{
  FEB_CAN_Status_t status;

  // Initialize CAN TX/RX system (starts hardware, enables interrupts)
  status = FEB_CAN_TX_Init();
  if (status != FEB_CAN_OK)
  {
    return status;
  }

  // Each subsystem registers its own callbacks
  FEB_CAN_BMS_Init();
  FEB_CAN_DASH_Init();
  FEB_CAN_PCU_Init();

  return FEB_CAN_OK;
}
