/**
 * @file    rfm95.c
 * @brief   RFM95W LoRa Radio Driver Implementation (Simplified - No LoRaWAN)
 *
 * Based on stm32-hal-rfm95 by Henri Heimann, stripped of LoRaWAN functionality.
 * Key difference: Uses separate HAL_SPI_Transmit + HAL_SPI_Receive calls
 * for better timing characteristics with the RFM95.
 *
 * @see     https://github.com/henriheimann/stm32-hal-rfm95
 */

#include "rfm95.h"
#include "feb_log.h"
#include <string.h>

#define TAG "[RFM95]"

/* ============================================================================
 * Private: SPI Communication
 *
 * Uses separate transmit and receive calls as recommended by the reference
 * library for reliable RFM95 communication.
 * ============================================================================ */

static inline void nss_select(rfm95_handle_t *handle)
{
    HAL_GPIO_WritePin(handle->nss_port, handle->nss_pin, GPIO_PIN_RESET);
}

static inline void nss_deselect(rfm95_handle_t *handle)
{
    HAL_GPIO_WritePin(handle->nss_port, handle->nss_pin, GPIO_PIN_SET);
}

/**
 * @brief Read register(s) using separate transmit and receive calls
 *
 * The reference library uses this approach for better timing:
 *   1. Transmit the register address
 *   2. Receive the data bytes
 */
static bool read_register_internal(rfm95_handle_t *handle, uint8_t reg,
                                   uint8_t *buffer, size_t length)
{
    uint8_t addr = reg & 0x7F;  /* Clear write bit */
    HAL_StatusTypeDef status;

    nss_select(handle);

    /* Send register address */
    status = HAL_SPI_Transmit(handle->spi_handle, &addr, 1, RFM95_SPI_TIMEOUT);
    if (status != HAL_OK) {
        nss_deselect(handle);
        return false;
    }

    /* Receive data */
    status = HAL_SPI_Receive(handle->spi_handle, buffer, length, RFM95_SPI_TIMEOUT);

    nss_deselect(handle);

    return (status == HAL_OK);
}

/**
 * @brief Write register(s) using transmit only
 */
static bool write_register_internal(rfm95_handle_t *handle, uint8_t reg,
                                    const uint8_t *buffer, size_t length)
{
    uint8_t addr = reg | 0x80;  /* Set write bit */
    HAL_StatusTypeDef status;

    nss_select(handle);

    /* Send register address */
    status = HAL_SPI_Transmit(handle->spi_handle, &addr, 1, RFM95_SPI_TIMEOUT);
    if (status != HAL_OK) {
        nss_deselect(handle);
        return false;
    }

    /* Send data */
    status = HAL_SPI_Transmit(handle->spi_handle, (uint8_t *)buffer, length, RFM95_SPI_TIMEOUT);

    nss_deselect(handle);

    return (status == HAL_OK);
}

/* ============================================================================
 * Public: Low-level register access
 * ============================================================================ */

uint8_t rfm95_read_register(rfm95_handle_t *handle, uint8_t reg)
{
    uint8_t value = 0;
    read_register_internal(handle, reg, &value, 1);
    return value;
}

void rfm95_write_register(rfm95_handle_t *handle, uint8_t reg, uint8_t value)
{
    write_register_internal(handle, reg, &value, 1);
}

/* ============================================================================
 * Private: Mode control
 * ============================================================================ */

static void set_mode(rfm95_handle_t *handle, uint8_t mode)
{
    rfm95_write_register(handle, RFM95_REG_OP_MODE,
                         RFM95_MODE_LONG_RANGE_MODE | mode);
}

static void clear_irq_flags(rfm95_handle_t *handle)
{
    rfm95_write_register(handle, RFM95_REG_IRQ_FLAGS, 0xFF);
}

/* ============================================================================
 * Public: Configuration
 * ============================================================================ */

void rfm95_get_default_config(rfm95_config_t *config)
{
    if (config == NULL) return;

    config->frequency        = RFM95_DEFAULT_FREQUENCY;
    config->tx_power         = RFM95_DEFAULT_TX_POWER;
    config->spreading_factor = RFM95_DEFAULT_SPREADING_FACTOR;
    config->bandwidth        = RFM95_DEFAULT_BANDWIDTH;
    config->coding_rate      = RFM95_DEFAULT_CODING_RATE;
    config->sync_word        = 0x12;  /* Private network */
    config->preamble_length  = 8;
    config->crc_enabled      = true;
}

/* ============================================================================
 * Public: Hardware control
 * ============================================================================ */

void rfm95_reset(rfm95_handle_t *handle)
{
    if (handle->nrst_port != NULL) {
        HAL_GPIO_WritePin(handle->nrst_port, handle->nrst_pin, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(handle->nrst_port, handle->nrst_pin, GPIO_PIN_SET);
        HAL_Delay(10);
    }
}

uint8_t rfm95_get_version(rfm95_handle_t *handle)
{
    return rfm95_read_register(handle, RFM95_REG_VERSION);
}

/* ============================================================================
 * Public: Initialization
 * ============================================================================ */

bool rfm95_init(rfm95_handle_t *handle)
{
    if (handle == NULL || handle->spi_handle == NULL) {
        return false;
    }

    /* Ensure CS is deselected */
    nss_deselect(handle);

    /* Enable module if EN pin is configured */
    if (handle->en_port != NULL) {
        HAL_GPIO_WritePin(handle->en_port, handle->en_pin, GPIO_PIN_SET);
        HAL_Delay(10);
    }

    /* Hardware reset */
    rfm95_reset(handle);

    /* Verify chip version */
    uint8_t version = rfm95_get_version(handle);
    if (version != RFM95_CHIP_VERSION) {
        return false;
    }

    /* Enter sleep mode for configuration */
    set_mode(handle, RFM95_MODE_SLEEP);
    HAL_Delay(10);

    /* Set LoRa mode (must be done in sleep) */
    rfm95_write_register(handle, RFM95_REG_OP_MODE,
                         RFM95_MODE_LONG_RANGE_MODE | RFM95_MODE_SLEEP);
    HAL_Delay(10);

    /* Apply configuration */
    rfm95_set_frequency(handle, handle->config.frequency);
    rfm95_set_power(handle, handle->config.tx_power);

    /* Configure FIFO base addresses */
    rfm95_write_register(handle, RFM95_REG_FIFO_TX_BASE_ADDR, 0x00);
    rfm95_write_register(handle, RFM95_REG_FIFO_RX_BASE_ADDR, 0x00);

    /* Set LNA boost */
    uint8_t lna = rfm95_read_register(handle, RFM95_REG_LNA);
    rfm95_write_register(handle, RFM95_REG_LNA, lna | 0x03);

    /* Configure modem
     * MODEM_CONFIG_1: [7:4] BW, [3:1] CR, [0] Implicit header mode
     * MODEM_CONFIG_2: [7:4] SF, [3] TX continuous, [2] RxPayloadCrcOn, [1:0] SymbTimeout MSB
     * MODEM_CONFIG_3: [3] LowDataRateOptimize, [2] AgcAutoOn
     */
    uint8_t modem_config_1 = (handle->config.bandwidth << 4) |
                              (handle->config.coding_rate << 1);
    uint8_t modem_config_2 = (handle->config.spreading_factor << 4) |
                              (handle->config.crc_enabled ? 0x04 : 0x00);
    uint8_t modem_config_3 = 0x04;  /* AGC on */

    /* Enable low data rate optimization for SF11/SF12 */
    if (handle->config.spreading_factor >= 11) {
        modem_config_3 |= 0x08;
    }

    rfm95_write_register(handle, RFM95_REG_MODEM_CONFIG_1, modem_config_1);
    rfm95_write_register(handle, RFM95_REG_MODEM_CONFIG_2, modem_config_2);
    rfm95_write_register(handle, RFM95_REG_MODEM_CONFIG_3, modem_config_3);

    /* Configure preamble */
    rfm95_write_register(handle, RFM95_REG_PREAMBLE_MSB,
                         (handle->config.preamble_length >> 8) & 0xFF);
    rfm95_write_register(handle, RFM95_REG_PREAMBLE_LSB,
                         handle->config.preamble_length & 0xFF);

    /* Set sync word */
    rfm95_write_register(handle, RFM95_REG_SYNC_WORD, handle->config.sync_word);

    /* Detection optimization for SF7-12 */
    if (handle->config.spreading_factor == 6) {
        rfm95_write_register(handle, RFM95_REG_DETECTION_OPTIMIZE, 0x05);
        rfm95_write_register(handle, RFM95_REG_DETECTION_THRESHOLD, 0x0C);
    } else {
        rfm95_write_register(handle, RFM95_REG_DETECTION_OPTIMIZE, 0x03);
        rfm95_write_register(handle, RFM95_REG_DETECTION_THRESHOLD, 0x0A);
    }

    /* Configure DIO mapping: DIO0 = RxDone/TxDone */
    rfm95_write_register(handle, RFM95_REG_DIO_MAPPING_1, 0x00);

    /* Clear state flags */
    handle->tx_done = false;
    handle->rx_done = false;
    handle->rx_timeout = false;
    handle->crc_error = false;
    handle->last_rssi = 0;
    handle->last_snr = 0;

    /* Enter standby */
    set_mode(handle, RFM95_MODE_STDBY);

    return true;
}

/* ============================================================================
 * Public: Configuration functions
 * ============================================================================ */

bool rfm95_set_power(rfm95_handle_t *handle, int8_t power)
{
    /* Clamp power to valid range */
    if (power < 2) power = 2;
    if (power > 20) power = 20;

    handle->config.tx_power = power;

    if (power > 17) {
        /* Use PA_DAC for high power (18-20 dBm) */
        rfm95_write_register(handle, RFM95_REG_PA_DAC, 0x87);
        rfm95_write_register(handle, RFM95_REG_PA_CONFIG,
                             RFM95_PA_BOOST | (power - 5));
    } else {
        /* Normal power mode */
        rfm95_write_register(handle, RFM95_REG_PA_DAC, 0x84);
        rfm95_write_register(handle, RFM95_REG_PA_CONFIG,
                             RFM95_PA_BOOST | (power - 2));
    }

    return true;
}

bool rfm95_set_frequency(rfm95_handle_t *handle, uint32_t frequency)
{
    handle->config.frequency = frequency;

    /* Calculate frequency register value
     * Frf = (Fstep * frf) where Fstep = Fxosc / 2^19
     * For 32 MHz crystal: frf = frequency * 2^19 / 32000000
     */
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000UL;

    rfm95_write_register(handle, RFM95_REG_FRF_MSB, (uint8_t)(frf >> 16));
    rfm95_write_register(handle, RFM95_REG_FRF_MID, (uint8_t)(frf >> 8));
    rfm95_write_register(handle, RFM95_REG_FRF_LSB, (uint8_t)(frf));

    return true;
}

/* ============================================================================
 * Public: Mode control
 * ============================================================================ */

void rfm95_standby(rfm95_handle_t *handle)
{
    set_mode(handle, RFM95_MODE_STDBY);
}

void rfm95_sleep(rfm95_handle_t *handle)
{
    set_mode(handle, RFM95_MODE_SLEEP);
}

void rfm95_start_receive(rfm95_handle_t *handle)
{
    set_mode(handle, RFM95_MODE_STDBY);

    /* Configure DIO0 for RxDone */
    rfm95_write_register(handle, RFM95_REG_DIO_MAPPING_1, 0x00);

    /* Set FIFO pointer */
    rfm95_write_register(handle, RFM95_REG_FIFO_ADDR_PTR, 0x00);

    /* Clear IRQ flags and state */
    clear_irq_flags(handle);
    handle->rx_done = false;
    handle->rx_timeout = false;
    handle->crc_error = false;

    /* Enter continuous RX */
    set_mode(handle, RFM95_MODE_RX_CONTINUOUS);
}

/* ============================================================================
 * Public: TX/RX Operations
 * ============================================================================ */

bool rfm95_transmit(rfm95_handle_t *handle, const uint8_t *data, size_t len,
                    uint32_t timeout_ms)
{
    if (handle == NULL || data == NULL || len == 0 || len > 255) {
        return false;
    }

    /* Enter standby */
    set_mode(handle, RFM95_MODE_STDBY);

    /* Configure DIO0 for TxDone */
    rfm95_write_register(handle, RFM95_REG_DIO_MAPPING_1, 0x40);

    /* Set FIFO pointer to TX base */
    rfm95_write_register(handle, RFM95_REG_FIFO_ADDR_PTR, 0x00);

    /* Write payload to FIFO */
    write_register_internal(handle, RFM95_REG_FIFO, data, len);

    /* Set payload length */
    rfm95_write_register(handle, RFM95_REG_PAYLOAD_LENGTH, (uint8_t)len);

    /* Clear IRQ flags and state */
    clear_irq_flags(handle);
    handle->tx_done = false;

    /* Start TX */
    set_mode(handle, RFM95_MODE_TX);

    /* Wait for TxDone (polling) */
    uint32_t start = HAL_GetTick();
    while (!handle->tx_done) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            set_mode(handle, RFM95_MODE_STDBY);
            return false;
        }

        /* Poll IRQ flags if interrupts not working */
        uint8_t irq = rfm95_read_register(handle, RFM95_REG_IRQ_FLAGS);
        if (irq & RFM95_IRQ_TX_DONE) {
            handle->tx_done = true;
            clear_irq_flags(handle);
        }

        HAL_Delay(1);
    }

    set_mode(handle, RFM95_MODE_STDBY);
    return true;
}

bool rfm95_receive(rfm95_handle_t *handle, uint8_t *buffer, size_t *len,
                   uint32_t timeout_ms)
{
    if (handle == NULL || buffer == NULL || len == NULL) {
        return false;
    }

    /* Start receive mode */
    rfm95_start_receive(handle);

    /* Wait for RxDone or timeout (polling) */
    uint32_t start = HAL_GetTick();
    while (!handle->rx_done && !handle->crc_error) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            set_mode(handle, RFM95_MODE_STDBY);
            *len = 0;
            return false;
        }

        /* Poll IRQ flags if interrupts not working */
        uint8_t irq = rfm95_read_register(handle, RFM95_REG_IRQ_FLAGS);
        if (irq & RFM95_IRQ_RX_DONE) {
            if (irq & RFM95_IRQ_PAYLOAD_CRC_ERROR) {
                handle->crc_error = true;
            } else {
                handle->rx_done = true;
            }
            clear_irq_flags(handle);
        }

        HAL_Delay(1);
    }

    set_mode(handle, RFM95_MODE_STDBY);

    if (handle->crc_error) {
        *len = 0;
        return false;
    }

    /* Read packet info */
    handle->last_rssi = -157 + rfm95_read_register(handle, RFM95_REG_PKT_RSSI_VALUE);
    handle->last_snr = ((int8_t)rfm95_read_register(handle, RFM95_REG_PKT_SNR_VALUE)) / 4;

    /* Read payload length */
    uint8_t rx_len = rfm95_read_register(handle, RFM95_REG_RX_NB_BYTES);

    /* Set FIFO pointer to RX current address */
    uint8_t rx_addr = rfm95_read_register(handle, RFM95_REG_FIFO_RX_CURRENT_ADDR);
    rfm95_write_register(handle, RFM95_REG_FIFO_ADDR_PTR, rx_addr);

    /* Read payload */
    read_register_internal(handle, RFM95_REG_FIFO, buffer, rx_len);

    *len = rx_len;
    return true;
}

/* ============================================================================
 * Public: Status
 * ============================================================================ */

int16_t rfm95_get_rssi(rfm95_handle_t *handle)
{
    return handle->last_rssi;
}

int8_t rfm95_get_snr(rfm95_handle_t *handle)
{
    return handle->last_snr;
}

/* ============================================================================
 * Public: Interrupt handling
 * ============================================================================ */

void rfm95_on_interrupt(rfm95_handle_t *handle, rfm95_interrupt_t interrupt)
{
    if (handle == NULL) return;

    uint8_t irq_flags = rfm95_read_register(handle, RFM95_REG_IRQ_FLAGS);

    switch (interrupt) {
        case RFM95_INTERRUPT_DIO0:
            /* DIO0 maps to TxDone or RxDone depending on mode */
            if (irq_flags & RFM95_IRQ_TX_DONE) {
                handle->tx_done = true;
                clear_irq_flags(handle);
            }
            if (irq_flags & RFM95_IRQ_RX_DONE) {
                if (irq_flags & RFM95_IRQ_PAYLOAD_CRC_ERROR) {
                    handle->crc_error = true;
                } else {
                    handle->rx_done = true;
                }
                clear_irq_flags(handle);
            }
            break;

        case RFM95_INTERRUPT_DIO1:
            /* DIO1 typically maps to RxTimeout */
            if (irq_flags & RFM95_IRQ_RX_TIMEOUT) {
                handle->rx_timeout = true;
                clear_irq_flags(handle);
            }
            break;

        case RFM95_INTERRUPT_DIO5:
            /* DIO5 typically maps to ModeReady, not used in basic operation */
            break;
    }
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

uint8_t rfm95_debug_spi_poll(rfm95_handle_t *handle)
{
    LOG_I(TAG, "=== SPI Test: Full-duplex TransmitReceive ===");

    uint8_t tx_buf[2] = {RFM95_REG_VERSION & 0x7F, 0x00};
    uint8_t rx_buf[2] = {0xFF, 0xFF};
    HAL_StatusTypeDef status;

    /* Log initial state */
    LOG_I(TAG, "CS pin state before: %d", HAL_GPIO_ReadPin(handle->nss_port, handle->nss_pin));

    /* Select chip */
    nss_select(handle);

    /* Small delay after CS */
    for (volatile int i = 0; i < 100; i++) {}

    LOG_I(TAG, "CS pin state after select: %d", HAL_GPIO_ReadPin(handle->nss_port, handle->nss_pin));
    LOG_I(TAG, "TX buffer: [0x%02X, 0x%02X]", tx_buf[0], tx_buf[1]);

    /* Full-duplex transfer */
    status = HAL_SPI_TransmitReceive(handle->spi_handle, tx_buf, rx_buf, 2, RFM95_SPI_TIMEOUT);

    nss_deselect(handle);

    LOG_I(TAG, "HAL status: %d", status);
    LOG_I(TAG, "RX buffer: [0x%02X, 0x%02X]", rx_buf[0], rx_buf[1]);
    LOG_I(TAG, "Version (rx[1]): 0x%02X (expected 0x12)", rx_buf[1]);

    return rx_buf[1];
}

uint8_t rfm95_debug_spi_separate(rfm95_handle_t *handle)
{
    LOG_I(TAG, "=== SPI Test: Separate Transmit + Receive ===");

    uint8_t addr = RFM95_REG_VERSION & 0x7F;
    uint8_t value = 0xFF;
    HAL_StatusTypeDef tx_status, rx_status;

    /* Log initial state */
    LOG_I(TAG, "CS pin state before: %d", HAL_GPIO_ReadPin(handle->nss_port, handle->nss_pin));

    /* Select chip */
    nss_select(handle);

    /* Small delay after CS */
    for (volatile int i = 0; i < 100; i++) {}

    LOG_I(TAG, "CS pin state after select: %d", HAL_GPIO_ReadPin(handle->nss_port, handle->nss_pin));
    LOG_I(TAG, "Sending address: 0x%02X", addr);

    /* Send address */
    tx_status = HAL_SPI_Transmit(handle->spi_handle, &addr, 1, RFM95_SPI_TIMEOUT);
    LOG_I(TAG, "TX status: %d", tx_status);

    /* Receive data */
    rx_status = HAL_SPI_Receive(handle->spi_handle, &value, 1, RFM95_SPI_TIMEOUT);
    LOG_I(TAG, "RX status: %d", rx_status);

    nss_deselect(handle);

    LOG_I(TAG, "Version: 0x%02X (expected 0x12)", value);

    return value;
}

uint8_t rfm95_debug_spi_raw(rfm95_handle_t *handle)
{
    LOG_I(TAG, "=== SPI Test: Raw register read (direct DR access) ===");

    SPI_TypeDef *spi = handle->spi_handle->Instance;
    uint8_t addr = RFM95_REG_VERSION & 0x7F;
    uint8_t dummy = 0x00;
    uint8_t value;

    LOG_I(TAG, "SPI Instance: %p", (void*)spi);
    LOG_I(TAG, "SPI SR before: 0x%04lX", (unsigned long)spi->SR);

    /* Select chip */
    nss_select(handle);
    for (volatile int i = 0; i < 100; i++) {}

    /* Wait for TXE */
    while (!(spi->SR & SPI_SR_TXE)) {}

    /* Send address byte */
    *((__IO uint8_t *)&spi->DR) = addr;

    /* Wait for RXNE */
    while (!(spi->SR & SPI_SR_RXNE)) {}
    (void)*((__IO uint8_t *)&spi->DR);  /* Discard first byte */

    /* Wait for TXE */
    while (!(spi->SR & SPI_SR_TXE)) {}

    /* Send dummy to clock in data */
    *((__IO uint8_t *)&spi->DR) = dummy;

    /* Wait for RXNE */
    while (!(spi->SR & SPI_SR_RXNE)) {}
    value = *((__IO uint8_t *)&spi->DR);

    /* Wait for not busy */
    while (spi->SR & SPI_SR_BSY) {}

    nss_deselect(handle);

    LOG_I(TAG, "SPI SR after: 0x%04lX", (unsigned long)spi->SR);
    LOG_I(TAG, "Version: 0x%02X (expected 0x12)", value);

    return value;
}

void rfm95_debug_gpio_status(rfm95_handle_t *handle)
{
    LOG_I(TAG, "=== GPIO Pin Status ===");
    LOG_I(TAG, "CS  (NSS):  %d (should be 1=HIGH when idle)",
          HAL_GPIO_ReadPin(handle->nss_port, handle->nss_pin));
    LOG_I(TAG, "RST (NRST): %d (should be 1=HIGH when running)",
          HAL_GPIO_ReadPin(handle->nrst_port, handle->nrst_pin));
    if (handle->en_port != NULL) {
        LOG_I(TAG, "EN:         %d (should be 1=HIGH when enabled)",
              HAL_GPIO_ReadPin(handle->en_port, handle->en_pin));
    }

    /* Also log SPI status */
    SPI_TypeDef *spi = handle->spi_handle->Instance;
    LOG_I(TAG, "SPI SR: 0x%04lX (TXE=%d, RXNE=%d, BSY=%d)",
          (unsigned long)spi->SR,
          (spi->SR & SPI_SR_TXE) ? 1 : 0,
          (spi->SR & SPI_SR_RXNE) ? 1 : 0,
          (spi->SR & SPI_SR_BSY) ? 1 : 0);
    LOG_I(TAG, "SPI CR1: 0x%04lX", (unsigned long)spi->CR1);
}

void rfm95_debug_reset(rfm95_handle_t *handle)
{
    LOG_I(TAG, "=== Hardware Reset Sequence ===");

    LOG_I(TAG, "RST pin before: %d", HAL_GPIO_ReadPin(handle->nrst_port, handle->nrst_pin));

    LOG_I(TAG, "Pulling RST LOW...");
    HAL_GPIO_WritePin(handle->nrst_port, handle->nrst_pin, GPIO_PIN_RESET);
    LOG_I(TAG, "RST pin now: %d", HAL_GPIO_ReadPin(handle->nrst_port, handle->nrst_pin));

    HAL_Delay(10);

    LOG_I(TAG, "Pulling RST HIGH...");
    HAL_GPIO_WritePin(handle->nrst_port, handle->nrst_pin, GPIO_PIN_SET);
    LOG_I(TAG, "RST pin now: %d", HAL_GPIO_ReadPin(handle->nrst_port, handle->nrst_pin));

    HAL_Delay(20);

    /* Try to read version */
    uint8_t version = rfm95_read_register(handle, RFM95_REG_VERSION);
    LOG_I(TAG, "Version after reset: 0x%02X (expected 0x12)", version);
}

void rfm95_debug_enable(rfm95_handle_t *handle)
{
    LOG_I(TAG, "=== Enable Pin Test ===");

    if (handle->en_port == NULL) {
        LOG_W(TAG, "EN pin not configured");
        return;
    }

    LOG_I(TAG, "EN pin before: %d", HAL_GPIO_ReadPin(handle->en_port, handle->en_pin));

    /* Disable */
    LOG_I(TAG, "Disabling module (EN=LOW)...");
    HAL_GPIO_WritePin(handle->en_port, handle->en_pin, GPIO_PIN_RESET);
    LOG_I(TAG, "EN pin now: %d", HAL_GPIO_ReadPin(handle->en_port, handle->en_pin));
    HAL_Delay(10);

    /* Try to read (should fail) */
    uint8_t ver_disabled = rfm95_read_register(handle, RFM95_REG_VERSION);
    LOG_I(TAG, "Version while disabled: 0x%02X (expected 0x00 or 0xFF)", ver_disabled);

    /* Enable */
    LOG_I(TAG, "Enabling module (EN=HIGH)...");
    HAL_GPIO_WritePin(handle->en_port, handle->en_pin, GPIO_PIN_SET);
    LOG_I(TAG, "EN pin now: %d", HAL_GPIO_ReadPin(handle->en_port, handle->en_pin));
    HAL_Delay(50);  /* Give module time to power up */

    /* Reset after power up */
    rfm95_reset(handle);

    /* Try to read (should work) */
    uint8_t ver_enabled = rfm95_read_register(handle, RFM95_REG_VERSION);
    LOG_I(TAG, "Version after enable+reset: 0x%02X (expected 0x12)", ver_enabled);
}
