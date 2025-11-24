#ifndef __FEB_CAN_SETUP_H
#define __FEB_CAN_SETUP_H

#include "FEB_CAN_RX.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and setup complete CAN system
 * 
 * Call once at startup to initialize CAN and register all callbacks.
 * This is the main entry point for CAN setup.
 * 
 * @return FEB_CAN_Status_t FEB_CAN_OK if successful
 */
FEB_CAN_Status_t FEB_CAN_Setup(void);

#ifdef __cplusplus
}
#endif

#endif

