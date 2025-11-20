// ********************************** Includes ***********************************

#include "FEB_HW.h"
#include "FreeRTOS.h"
#include "task.h"

// Only compile redundancy code when in redundant mode
#if (ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)

// ********************************** Global State *******************************

spi_redundancy_state_t g_spi_redundancy = {0};

// ********************************** Private Functions **************************

// Perform the actual failover operation
static void perform_failover(void) {
    // Swap active and backup channels
    SPI_HandleTypeDef *temp_spi = g_spi_redundancy.active_spi;
    g_spi_redundancy.active_spi = g_spi_redundancy.backup_spi;
    g_spi_redundancy.backup_spi = temp_spi;

    GPIO_TypeDef *temp_port = g_spi_redundancy.active_cs_port;
    g_spi_redundancy.active_cs_port = g_spi_redundancy.backup_cs_port;
    g_spi_redundancy.backup_cs_port = temp_port;

    uint16_t temp_pin = g_spi_redundancy.active_cs_pin;
    g_spi_redundancy.active_cs_pin = g_spi_redundancy.backup_cs_pin;
    g_spi_redundancy.backup_cs_pin = temp_pin;

    // Update channel indicator
    g_spi_redundancy.current_channel = (g_spi_redundancy.current_channel == 0) ? 1 : 0;

    // Reset error counters
    g_spi_redundancy.pec_error_count = 0;
    g_spi_redundancy.pec_success_count = 0;

    // Update failover tracking
    g_spi_redundancy.failover_count++;
    g_spi_redundancy.last_failover_tick = xTaskGetTickCount();
    g_spi_redundancy.failover_locked = true;
}

// Check if failover lockout period has expired
static bool is_lockout_expired(void) {
    if (!g_spi_redundancy.failover_locked) {
        return true;
    }

    uint32_t current_tick = xTaskGetTickCount();
    uint32_t elapsed_ms = (current_tick - g_spi_redundancy.last_failover_tick) * portTICK_PERIOD_MS;

    if (elapsed_ms >= ISOSPI_FAILOVER_LOCKOUT_MS) {
        g_spi_redundancy.failover_locked = false;
        return true;
    }

    return false;
}

// ********************************** Public Functions ***************************

void FEB_spi_init_redundancy(void) {
    // Set primary channel based on configuration
    if (ISOSPI_PRIMARY_CHANNEL == 1) {
        // SPI1 is primary
        g_spi_redundancy.active_spi = FEB_SPI1_HANDLE;
        g_spi_redundancy.active_cs_port = FEB_SPI1_CS_PORT;
        g_spi_redundancy.active_cs_pin = FEB_SPI1_CS_PIN;
        g_spi_redundancy.backup_spi = FEB_SPI2_HANDLE;
        g_spi_redundancy.backup_cs_port = FEB_SPI2_CS_PORT;
        g_spi_redundancy.backup_cs_pin = FEB_SPI2_CS_PIN;
        g_spi_redundancy.current_channel = 0;
    } else {
        // SPI2 is primary
        g_spi_redundancy.active_spi = FEB_SPI2_HANDLE;
        g_spi_redundancy.active_cs_port = FEB_SPI2_CS_PORT;
        g_spi_redundancy.active_cs_pin = FEB_SPI2_CS_PIN;
        g_spi_redundancy.backup_spi = FEB_SPI1_HANDLE;
        g_spi_redundancy.backup_cs_port = FEB_SPI1_CS_PORT;
        g_spi_redundancy.backup_cs_pin = FEB_SPI1_CS_PIN;
        g_spi_redundancy.current_channel = 1;
    }

    // Initialize counters
    g_spi_redundancy.pec_error_count = 0;
    g_spi_redundancy.pec_success_count = 0;
    g_spi_redundancy.failover_count = 0;
    g_spi_redundancy.last_failover_tick = 0;
    g_spi_redundancy.failover_locked = false;
}

void FEB_spi_report_pec_error(void) {
    // Increment consecutive error count
    g_spi_redundancy.pec_error_count++;
    g_spi_redundancy.pec_success_count = 0;  // Reset success counter

    // Check if we should failover
    if (g_spi_redundancy.pec_error_count >= ISOSPI_FAILOVER_PEC_THRESHOLD) {
        // Check if lockout period has expired
        if (is_lockout_expired()) {
            perform_failover();
        }
    }
}

void FEB_spi_report_pec_success(void) {
    // Increment consecutive success count
    g_spi_redundancy.pec_success_count++;

    // After a few successes, clear error count (hysteresis)
    if (g_spi_redundancy.pec_success_count >= 3) {
        g_spi_redundancy.pec_error_count = 0;
    }
}

uint8_t FEB_spi_get_active_channel(void) {
    // Return 1 for SPI1, 2 for SPI2
    return (g_spi_redundancy.current_channel == 0) ? 1 : 2;
}

uint16_t FEB_spi_get_failover_count(void) {
    return g_spi_redundancy.failover_count;
}

void FEB_spi_force_failover(void) {
    // Force failover regardless of lockout (for testing)
    perform_failover();
}

#endif // ISOSPI_MODE_REDUNDANT
