#ifndef FEB_I2C_PROTECTED_H
#define FEB_I2C_PROTECTED_H

#include "stm32f4xx_hal.h"      /* Device-specific HAL header */
#include "cmsis_os2.h"          /* For osMutexAcquire / osMutexRelease */

/* Extern declaration of the mutex created in main.c */
extern osMutexId_t FEB_I2C_MutexHandle;

/* -------- Wrapped versions of HAL I2C APIs -------- */

HAL_StatusTypeDef FEB_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c,
                                          uint16_t DevAddress,
                                          uint8_t *pData,
                                          uint16_t Size,
                                          uint32_t Timeout);

HAL_StatusTypeDef FEB_I2C_Master_Receive(I2C_HandleTypeDef *hi2c,
                                         uint16_t DevAddress,
                                         uint8_t *pData,
                                         uint16_t Size,
                                         uint32_t Timeout);

HAL_StatusTypeDef FEB_I2C_Slave_Transmit(I2C_HandleTypeDef *hi2c,
                                         uint8_t *pData,
                                         uint16_t Size,
                                         uint32_t Timeout);

HAL_StatusTypeDef FEB_I2C_Slave_Receive(I2C_HandleTypeDef *hi2c,
                                        uint8_t *pData,
                                        uint16_t Size,
                                        uint32_t Timeout);

HAL_StatusTypeDef FEB_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                    uint16_t DevAddress,
                                    uint16_t MemAddress,
                                    uint16_t MemAddSize,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout);

HAL_StatusTypeDef FEB_I2C_Mem_Read(I2C_HandleTypeDef *hi2c,
                                   uint16_t DevAddress,
                                   uint16_t MemAddress,
                                   uint16_t MemAddSize,
                                   uint8_t *pData,
                                   uint16_t Size,
                                   uint32_t Timeout);

#endif /* FEB_I2C_PROTECTED_H */