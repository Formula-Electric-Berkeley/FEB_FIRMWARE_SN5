/**
 * @file FEB_ADBMS_Platform.h
 * @brief Platform abstraction for ADBMS6830B register driver
 *
 * This file provides the bridge between the hardware-agnostic ADBMS6830B_Registers
 * driver and the STM32 HAL + FEB hardware abstraction layer.
 */

#ifndef FEB_ADBMS_PLATFORM_H
#define FEB_ADBMS_PLATFORM_H

#include <stdint.h>

/**
 * @brief Initialize platform-specific hardware for ADBMS communication
 *
 * Should be called before ADBMS_Init(). Initializes SPI redundancy if enabled.
 */
void FEB_ADBMS_Platform_Init(void);

#endif /* FEB_ADBMS_PLATFORM_H */
