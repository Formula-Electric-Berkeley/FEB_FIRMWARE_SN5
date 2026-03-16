/**
 * @file main.h
 * @brief Stub main.h for host-based unit testing.
 *
 * feb_tps.c includes "main.h" as a fallback when no STM32 variant is defined.
 * In the test build, hal_mock.h has already been injected via -include, so
 * this file only needs to guard against double-inclusion.
 */
#ifndef MAIN_H
#define MAIN_H

/* hal_mock.h was already injected via compiler -include flag.
 * All required HAL types (I2C_HandleTypeDef, GPIO_TypeDef, HAL_StatusTypeDef,
 * GPIO_PinState, etc.) are already defined. Nothing more needed here. */

#endif /* MAIN_H */