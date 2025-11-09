#ifndef INC_FEB_HW_H_
#define INC_FEB_HW_H_

#include "spi.h"
#include "gpio.h"
#include "main.h"
#include "FEB_Const.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

// ********************************** isoSPI Hardware Abstraction *****************
// This file provides a hardware abstraction layer for ADBMS6830B isoSPI communication
// Supports three modes selectable via ISOSPI_MODE macro in FEB_Const.h:
//   - ISOSPI_MODE_REDUNDANT: Dual SPI with automatic PEC-error failover
//   - ISOSPI_MODE_SPI1_ONLY: Use only SPI1 (primary channel)
//   - ISOSPI_MODE_SPI2_ONLY: Use only SPI2 (backup channel)

// ********************************** Hardware Pin Definitions ********************

// SPI1 Configuration (Primary isoSPI channel)
#define FEB_SPI1_HANDLE &hspi1
#define FEB_SPI1_CS_PORT SPI1_CS_GPIO_Port // From main.h - PC3
#define FEB_SPI1_CS_PIN SPI1_CS_Pin

// SPI2 Configuration (Backup isoSPI channel)
// #define FEB_SPI2_HANDLE          &hspi2
// #define FEB_SPI2_CS_PORT         SPI2_CS_GPIO_Port  // From main.h - PC6
// #define FEB_SPI2_CS_PIN          SPI2_CS_Pin

// SPI Timeout (ms) - reasonable timeout for FreeRTOS operation
#define FEB_SPI_TIMEOUT_MS 100

// ********************************** Redundancy State (REDUNDANT mode only) ******

#if (ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)

typedef struct
{
    SPI_HandleTypeDef *active_spi; // Currently active SPI handle
    SPI_HandleTypeDef *backup_spi; // Backup SPI handle
    GPIO_TypeDef *active_cs_port;  // Active CS GPIO port
    uint16_t active_cs_pin;        // Active CS GPIO pin
    GPIO_TypeDef *backup_cs_port;  // Backup CS GPIO port
    uint16_t backup_cs_pin;        // Backup CS GPIO pin
    uint16_t pec_error_count;      // Consecutive PEC errors on active channel
    uint16_t pec_success_count;    // Consecutive successes (for clearing errors)
    uint8_t current_channel;       // 0=SPI1, 1=SPI2
    uint16_t failover_count;       // Total number of failovers (diagnostic)
    uint32_t last_failover_tick;   // Tick count of last failover (for lockout)
    bool failover_locked;          // True if in lockout period
} spi_redundancy_state_t;

// Global redundancy state
extern spi_redundancy_state_t g_spi_redundancy;

// Initialize redundancy system
void FEB_spi_init_redundancy(void);

// Report PEC error/success (triggers failover logic)
void FEB_spi_report_pec_error(void);
void FEB_spi_report_pec_success(void);

// Query redundancy status
uint8_t FEB_spi_get_active_channel(void); // Returns 1 or 2
uint16_t FEB_spi_get_failover_count(void);

// Force failover (for testing)
void FEB_spi_force_failover(void);

#endif // ISOSPI_MODE_REDUNDANT

// ********************************** Mode-Specific Configuration *****************

#if (ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
// Redundant mode: Use variables from redundancy state
#define FEB_ACTIVE_SPI (g_spi_redundancy.active_spi)
#define FEB_ACTIVE_CS_PORT (g_spi_redundancy.active_cs_port)
#define FEB_ACTIVE_CS_PIN (g_spi_redundancy.active_cs_pin)

#elif (ISOSPI_MODE == ISOSPI_MODE_SPI1_ONLY)
// SPI1 only mode: Use SPI1 hardware
#define FEB_ACTIVE_SPI FEB_SPI1_HANDLE
#define FEB_ACTIVE_CS_PORT FEB_SPI1_CS_PORT
#define FEB_ACTIVE_CS_PIN FEB_SPI1_CS_PIN

#elif (ISOSPI_MODE == ISOSPI_MODE_SPI2_ONLY)
// SPI2 only mode: Use SPI2 hardware
#define FEB_ACTIVE_SPI FEB_SPI2_HANDLE
#define FEB_ACTIVE_CS_PORT FEB_SPI2_CS_PORT
#define FEB_ACTIVE_CS_PIN FEB_SPI2_CS_PIN

#else
#error "Invalid ISOSPI_MODE selected in FEB_Const.h"
#endif

// ********************************** SPI Hardware Functions **********************

// Chip Select Control
static inline void FEB_cs_low(void)
{
    HAL_GPIO_WritePin(FEB_ACTIVE_CS_PORT, FEB_ACTIVE_CS_PIN, GPIO_PIN_RESET);
}

static inline void FEB_cs_high(void)
{
    HAL_GPIO_WritePin(FEB_ACTIVE_CS_PORT, FEB_ACTIVE_CS_PIN, GPIO_PIN_SET);
}

// SPI Write Function
static inline void FEB_spi_write_array(uint16_t len, uint8_t *data)
{
    HAL_SPI_Transmit(FEB_ACTIVE_SPI, data, len, FEB_SPI_TIMEOUT_MS);
}

// SPI Write Then Read Function
static inline void FEB_spi_write_read(uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len)
{
    HAL_SPI_Transmit(FEB_ACTIVE_SPI, tx_data, tx_len, FEB_SPI_TIMEOUT_MS);
    HAL_SPI_Receive(FEB_ACTIVE_SPI, rx_data, rx_len, FEB_SPI_TIMEOUT_MS);
}

// SPI Read Single Byte Function
static inline uint8_t FEB_spi_read_byte(uint8_t dummy_byte)
{
    uint8_t rx_byte = 0;
    HAL_SPI_TransmitReceive(FEB_ACTIVE_SPI, &dummy_byte, &rx_byte, 1, FEB_SPI_TIMEOUT_MS);
    return rx_byte;
}

// ********************************** isoSPI Wake-Up Function *********************

// Wake ADBMS6830B from sleep mode
// isoSPI requires CS pulse >400ns for wake-up, then 300us delay
static inline void wakeup_sleep(uint8_t total_ic)
{
    (void)total_ic; // Unused parameter, kept for API compatibility

    // Pulse CS low for wake-up
    FEB_cs_low();

    // Short delay >400ns (a few microseconds)
    for (volatile int i = 0; i < 100; i++)
    {
        __NOP();
    }

    FEB_cs_high();

    // Wait 300us minimum for ADBMS to wake up
    // Using 1ms for safety (osDelay is FreeRTOS-aware)
    osDelay(1);
}

#endif /* INC_FEB_HW_H_ */
