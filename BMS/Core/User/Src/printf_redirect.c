/**
  ******************************************************************************
  * @file           : printf_redirect.c
  * @brief          : Printf redirection to UART implementation
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "printf_redirect.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* FreeRTOS includes - Always enabled in this project -----------------------*/
#ifndef UNIT_TEST
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#endif

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart2;

/* FreeRTOS variables - Always available ------------------------------------*/
static SemaphoreHandle_t uart_mutex = NULL;
static bool printf_redirect_initialized = false;

/* FreeRTOS objects for ISR printf functionality ----------------------------*/
// For production, these are external references to .ioc-defined objects
extern QueueHandle_t printfISRQueueHandle;  // Generated from .ioc: printfISRQueue
extern TaskHandle_t printfISRTaskHandle;    // Generated from .ioc: printfISRTask

typedef struct {
    char message[PRINTF_ISR_BUFFER_SIZE];
    uint16_t length;
} printf_isr_msg_t;

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
  * @brief  Transmit a single character via UART (unsafe - no mutex protection)
  * @param  ch: Character to transmit
  * @param  huart: Pointer to UART handle
  * @retval Character transmitted on success, -1 on error
  */
static int uart_putchar_unsafe(int ch, UART_HandleTypeDef* huart) {
    if (huart == NULL || huart->Instance == NULL) {
        return -1;
    }
    
    uint8_t byte = (uint8_t)ch;
    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, &byte, 1, PRINTF_UART_TIMEOUT_MS);
    return (status == HAL_OK) ? ch : -1;
}

/**
  * @brief  Transmit a string via UART (unsafe - no mutex protection)
  * @param  str: String to transmit
  * @param  huart: Pointer to UART handle
  * @retval Number of characters transmitted on success, -1 on error
  */
static int uart_puts_unsafe(const char* str, UART_HandleTypeDef* huart) {
    if (str == NULL || huart == NULL) {
        return -1;
    }
    
    size_t len = strlen(str);
    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t*)str, len, PRINTF_UART_TIMEOUT_MS);
    return (status == HAL_OK) ? (int)len : -1;
}


#ifdef __GNUC__
/**
  * @brief  Redirect printf output to UART for GCC
  * @param  ch: Character to transmit
  * @retval Character transmitted on success, -1 on error
  */
int __io_putchar(int ch) {
    // Use unsafe version to avoid recursion
    return uart_putchar_unsafe(ch, &huart2);
}
#else
/**
  * @brief  Redirect printf output to UART for non-GCC compilers
  * @param  ch: Character to transmit
  * @param  f: File pointer (unused)
  * @retval Character transmitted on success, -1 on error
  */
int fputc(int ch, FILE *f) {
    (void)f;
    // Use unsafe version to avoid recursion
    return uart_putchar_unsafe(ch, &huart2);
}
#endif

/**
  * @brief  Check if UART is ready for transmission
  * @param  huart: Pointer to UART handle
  * @retval true if UART is ready, false otherwise
  */
bool uart_is_ready(UART_HandleTypeDef* huart) {
    return (huart != NULL && huart->Instance != NULL);
}


/* Public API Functions (Smart safe/unsafe selection) ----------------------*/

/**
  * @brief  Transmit a single character via UART (thread-safe when FreeRTOS initialized)
  * @param  ch: Character to transmit
  * @param  huart: Pointer to UART handle
  * @retval Character transmitted on success, -1 on error
  */
int uart_putchar(int ch, UART_HandleTypeDef* huart) {
    if (printf_redirect_initialized && uart_mutex != NULL) {
        // FreeRTOS is initialized, use mutex protection
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(PRINTF_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            return -1;
        }
        int result = uart_putchar_unsafe(ch, huart);
        xSemaphoreGive(uart_mutex);
        return result;
    } else {
        // FreeRTOS not initialized, use unsafe version
        return uart_putchar_unsafe(ch, huart);
    }
}

/**
  * @brief  Transmit a string via UART (thread-safe when FreeRTOS initialized)
  * @param  str: String to transmit
  * @param  huart: Pointer to UART handle
  * @retval Number of characters transmitted on success, -1 on error
  */
int uart_puts(const char* str, UART_HandleTypeDef* huart) {
    if (printf_redirect_initialized && uart_mutex != NULL) {
        // FreeRTOS is initialized, use mutex protection
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(PRINTF_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            return -1;
        }
        int result = uart_puts_unsafe(str, huart);
        xSemaphoreGive(uart_mutex);
        return result;
    } else {
        // FreeRTOS not initialized, use unsafe version
        return uart_puts_unsafe(str, huart);
    }
}

/**
  * @brief  Formatted output via UART (thread-safe when FreeRTOS initialized)
  * @param  huart: Pointer to UART handle
  * @param  format: Format string
  * @param  ...: Variable arguments
  * @retval Number of characters transmitted on success, -1 on error
  */
int uart_printf(UART_HandleTypeDef* huart, const char* format, ...) {
    if (format == NULL || huart == NULL || huart->Instance == NULL) {
        return -1;
    }
    
    char buffer[UART_PRINTF_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len < 0) {
        return -1;
    }
    
    if (len >= (int)sizeof(buffer)) {
        len = sizeof(buffer) - 1;
        buffer[len] = '\0';
    }
    
    if (len == 0) {
        return 0;
    }
    
    // Now transmit the buffer with or without mutex protection
    if (printf_redirect_initialized && uart_mutex != NULL) {
        // FreeRTOS is initialized, use mutex protection
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(PRINTF_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            return -1;
        }
        
        HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t*)buffer, (uint16_t)len, PRINTF_UART_TIMEOUT_MS);
        int result = (status == HAL_OK) ? len : -1;
        
        xSemaphoreGive(uart_mutex);
        return result;
    } else {
        // FreeRTOS not initialized, use unsafe version
        HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t*)buffer, (uint16_t)len, PRINTF_UART_TIMEOUT_MS);
        return (status == HAL_OK) ? len : -1;
    }
}

/**
  * @brief  Thread-safe debug printf function
  * @param  format: Format string
  * @param  ...: Variable arguments
  * @retval Number of characters transmitted on success, -1 on error
  */
int debug_printf_safe(const char* format, ...) {
    if (format == NULL) {
        return -1;
    }
    
    char buffer[UART_PRINTF_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len < 0) {
        return -1;
    }
    
    if (len >= (int)sizeof(buffer)) {
        len = sizeof(buffer) - 1;
        buffer[len] = '\0';
    }
    
    if (len == 0) {
        return 0;
    }
    
    // Use uart_printf approach but for debug output via huart2
    return uart_printf(&huart2, "%s", buffer);
}

/* FreeRTOS Functions - Always available -----------------------------------*/
/**
  * @brief  Initialize printf redirection for FreeRTOS
  * @retval None
  */
void printf_redirect_init(void) {
    if (!printf_redirect_initialized) {
        // Create mutex first
        uart_mutex = xSemaphoreCreateMutex();
        if (uart_mutex == NULL) {
            return;
        }
        
#ifdef UNIT_TEST
        // Create queue for ISR messages (unit tests only)
        printfISRQueueHandle = xQueueCreate(10, sizeof(printf_isr_msg_t));
        if (printfISRQueueHandle == NULL) {
            vSemaphoreDelete(uart_mutex);
            uart_mutex = NULL;
            return;
        }
        
        // Create task to process ISR messages (unit tests only)
        if (xTaskCreate(printf_isr_task, "PrintfISRTask", 256, NULL, 1, &printfISRTaskHandle) != pdTRUE) {
            vQueueDelete(printfISRQueueHandle);
            printfISRQueueHandle = NULL;
            vSemaphoreDelete(uart_mutex);
            uart_mutex = NULL;
            return;
        }
#else
        // In production, queue and task are created by .ioc configuration
        // We just assume they exist and are accessible via extern declarations
#endif
        
        printf_redirect_initialized = true;
    }
}

/**
  * @brief  Deinitialize printf redirection for FreeRTOS
  * @retval None
  */
void printf_redirect_deinit(void) {
    if (printf_redirect_initialized) {
        if (uart_mutex != NULL) {
            vSemaphoreDelete(uart_mutex);
            uart_mutex = NULL;
        }
        
#ifdef UNIT_TEST
        // In unit tests, we manage queue and task deletion
        if (printfISRTaskHandle != NULL) {
            vTaskDelete(printfISRTaskHandle);
            printfISRTaskHandle = NULL;
        }
        
        if (printfISRQueueHandle != NULL) {
            vQueueDelete(printfISRQueueHandle);
            printfISRQueueHandle = NULL;
        }
#else
        // In production, queue and task are managed by .ioc configuration
        // We don't delete them here as they're external objects
#endif
        
        printf_redirect_initialized = false;
    }
}


/**
  * @brief  ISR-safe printf function using FreeRTOS queue
  * @param  format: Format string
  * @param  ...: Variable arguments  
  * @retval Number of characters queued on success, -1 on error
  */
int uart_printf_isr(const char* format, ...) {
    if (format == NULL || printfISRQueueHandle == NULL) {
        return -1;
    }
    
    printf_isr_msg_t msg;
    va_list args;
    va_start(args, format);
    int len = vsnprintf(msg.message, sizeof(msg.message), format, args);
    va_end(args);
    
    if (len < 0) {
        return -1;
    }
    
    if (len >= (int)sizeof(msg.message)) {
        len = sizeof(msg.message) - 1;
        msg.message[len] = '\0';
    }
    
    msg.length = (uint16_t)len;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(printfISRQueueHandle, &msg, &xHigherPriorityTaskWoken) == pdTRUE) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return len;
    }
    
    return -1;
}

#ifdef UNIT_TEST
/**
  * @brief  Reset printf redirect state for unit testing
  * @retval None
  */
void printf_redirect_reset_for_test(void) {
    printf_redirect_initialized = false;
    uart_mutex = NULL;
    printfISRQueueHandle = NULL;
    printfISRTaskHandle = NULL;
}

/**
  * @brief  Mock ISR task function for unit testing
  * @param  pvParameters: Task parameters (unused in mock)
  * @retval None
  */
void printf_isr_task(void *pvParameters) {
    (void)pvParameters;
    
    typedef struct {
        char message[64];
        uint16_t length;
    } printf_isr_msg_t;
    
    printf_isr_msg_t msg;
    
    // Single iteration for testing
    if (xQueueReceive(printfISRQueueHandle, &msg, portMAX_DELAY) == pdTRUE) {
        if (uart_is_ready(&huart2)) {
            HAL_UART_Transmit(&huart2, (uint8_t*)msg.message, msg.length, PRINTF_UART_TIMEOUT_MS);
        }
    }
}
#endif
