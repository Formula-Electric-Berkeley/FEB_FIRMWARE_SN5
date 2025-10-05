/**
  ******************************************************************************
  * @file           : FEB_Printf_Redirect.c
  * @brief          : Simple printf redirection to UART using DMA
  ******************************************************************************
  * @attention
  *
  * This file implements minimal printf redirection by overriding the
  * _write() system call to route stdout to a UART peripheral using DMA.
  *
  * Prerequisites:
  * - Configure UART with DMA TX in STM32CubeMX
  * - Enable circular mode for DMA if continuous transmission is needed
  *
  ******************************************************************************
  */

#include "FEB_Printf_Redirect.h"
#include <stdio.h>

/* Private variables */
static UART_HandleTypeDef *printf_huart = NULL;

/* Timeout for DMA transmission */
#define PRINTF_DMA_TIMEOUT_MS 100

/**
 * @brief Initialize printf redirection to UART with DMA
 * @param huart Pointer to UART handle (must be initialized with DMA)
 */
void FEB_Printf_Init(UART_HandleTypeDef *huart) {
    printf_huart = huart;
    
    /* Disable stdout buffering for immediate output */
    setvbuf(stdout, NULL, _IONBF, 0);
}

/**
 * @brief Override _write system call for printf redirection
 * @note This is the minimal implementation needed for printf to work
 * @param file File descriptor (1 = stdout, 2 = stderr)
 * @param ptr Pointer to data to write
 * @param len Length of data
 * @return Number of bytes written or -1 on error
 */
int _write(int file, char *ptr, int len) {
    /* Check if UART is initialized and file is stdout/stderr */
    if (printf_huart == NULL || (file != 1 && file != 2)) {
        return -1;
    }
    
    /* Transmit data using DMA */
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(printf_huart, (uint8_t*)ptr, len);
    
    if (status != HAL_OK) {
        return -1;
    }
    
    /* Wait for DMA transfer to complete with timeout */
    uint32_t tickstart = HAL_GetTick();
    while (HAL_UART_GetState(printf_huart) != HAL_UART_STATE_READY) {
        if ((HAL_GetTick() - tickstart) > PRINTF_DMA_TIMEOUT_MS) {
            return -1;  /* Timeout */
        }
    }
    
    return len;
}