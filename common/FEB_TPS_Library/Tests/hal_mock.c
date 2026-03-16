/**
 * @file hal_mock.c
 * @brief HAL mock implementations for host-based unit testing
 */

#include "hal_mock.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Mock State
 * ============================================================================ */

#define MAX_QUEUE 64
#define MAX_GPIO_ENTRIES 32

/* I2C read response queue */
static I2C_ReadResponse_t read_queue[MAX_QUEUE];
static int read_queue_head = 0;
static int read_queue_tail = 0;
static int read_call_count = 0;

/* I2C write response queue */
static I2C_WriteResponse_t write_queue[MAX_QUEUE];
static int write_queue_head = 0;
static int write_queue_tail = 0;

/* I2C write record log */
static I2C_WriteRecord_t write_records[MAX_QUEUE];
static int write_record_count = 0;

/* GPIO state table */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState read_state;   /* returned by HAL_GPIO_ReadPin */
    GPIO_PinState written_state;/* last value passed to HAL_GPIO_WritePin */
    bool has_written;
} GPIO_Entry_t;

static GPIO_Entry_t gpio_table[MAX_GPIO_ENTRIES];
static int gpio_table_count = 0;
static int gpio_write_call_count = 0;

/* ============================================================================
 * Mock Control Implementation
 * ============================================================================ */

void hal_mock_reset(void)
{
    memset(read_queue, 0, sizeof(read_queue));
    read_queue_head = 0;
    read_queue_tail = 0;
    read_call_count = 0;

    memset(write_queue, 0, sizeof(write_queue));
    write_queue_head = 0;
    write_queue_tail = 0;

    memset(write_records, 0, sizeof(write_records));
    write_record_count = 0;

    memset(gpio_table, 0, sizeof(gpio_table));
    gpio_table_count = 0;
    gpio_write_call_count = 0;
}

void hal_mock_i2c_read_push(HAL_StatusTypeDef status, uint8_t msb, uint8_t lsb)
{
    if (read_queue_tail < MAX_QUEUE) {
        read_queue[read_queue_tail].status = status;
        read_queue[read_queue_tail].data[0] = msb;
        read_queue[read_queue_tail].data[1] = lsb;
        read_queue_tail++;
    }
}

void hal_mock_i2c_write_push(HAL_StatusTypeDef status)
{
    if (write_queue_tail < MAX_QUEUE) {
        write_queue[write_queue_tail].status = status;
        write_queue_tail++;
    }
}

int hal_mock_i2c_write_count(void)
{
    return write_record_count;
}

I2C_WriteRecord_t hal_mock_i2c_write_get(int index)
{
    if (index >= 0 && index < write_record_count) {
        return write_records[index];
    }
    I2C_WriteRecord_t empty = {0};
    return empty;
}

int hal_mock_i2c_read_count(void)
{
    return read_call_count;
}

void hal_mock_gpio_set_pin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    /* Find or create entry */
    for (int i = 0; i < gpio_table_count; i++) {
        if (gpio_table[i].port == port && gpio_table[i].pin == pin) {
            gpio_table[i].read_state = state;
            return;
        }
    }
    if (gpio_table_count < MAX_GPIO_ENTRIES) {
        gpio_table[gpio_table_count].port = port;
        gpio_table[gpio_table_count].pin = pin;
        gpio_table[gpio_table_count].read_state = state;
        gpio_table_count++;
    }
}

GPIO_PinState hal_mock_gpio_get_written(GPIO_TypeDef *port, uint16_t pin)
{
    for (int i = 0; i < gpio_table_count; i++) {
        if (gpio_table[i].port == port && gpio_table[i].pin == pin) {
            return gpio_table[i].written_state;
        }
    }
    return GPIO_PIN_RESET;
}

int hal_mock_gpio_write_count(void)
{
    return gpio_write_call_count;
}

/* ============================================================================
 * HAL Function Mock Implementations
 * ============================================================================ */

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c,
                                    uint16_t DevAddress,
                                    uint16_t MemAddress,
                                    uint16_t MemAddSize,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout)
{
    (void)hi2c; (void)DevAddress; (void)MemAddSize; (void)Size; (void)Timeout;
    (void)MemAddress;

    read_call_count++;

    if (read_queue_head < read_queue_tail) {
        I2C_ReadResponse_t resp = read_queue[read_queue_head++];
        if (resp.status == HAL_OK && pData != NULL) {
            pData[0] = resp.data[0];
            pData[1] = resp.data[1];
        }
        return resp.status;
    }

    /* Default: success with zero data */
    if (pData != NULL) {
        pData[0] = 0;
        pData[1] = 0;
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                     uint16_t DevAddress,
                                     uint16_t MemAddress,
                                     uint16_t MemAddSize,
                                     uint8_t *pData,
                                     uint16_t Size,
                                     uint32_t Timeout)
{
    (void)hi2c; (void)MemAddSize; (void)Size; (void)Timeout;

    /* Record the write */
    if (write_record_count < MAX_QUEUE) {
        write_records[write_record_count].dev_addr = DevAddress;
        write_records[write_record_count].mem_addr = (uint8_t)MemAddress;
        if (pData != NULL) {
            write_records[write_record_count].value =
                ((uint16_t)pData[0] << 8) | pData[1];
        }
        write_record_count++;
    }

    /* Check for queued response */
    if (write_queue_head < write_queue_tail) {
        return write_queue[write_queue_head++].status;
    }

    /* Default: success */
    return HAL_OK;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
    gpio_write_call_count++;

    /* Find or create entry */
    for (int i = 0; i < gpio_table_count; i++) {
        if (gpio_table[i].port == GPIOx && gpio_table[i].pin == GPIO_Pin) {
            gpio_table[i].written_state = PinState;
            gpio_table[i].has_written = true;
            return;
        }
    }
    if (gpio_table_count < MAX_GPIO_ENTRIES) {
        gpio_table[gpio_table_count].port = GPIOx;
        gpio_table[gpio_table_count].pin = GPIO_Pin;
        gpio_table[gpio_table_count].written_state = PinState;
        gpio_table[gpio_table_count].has_written = true;
        gpio_table_count++;
    }
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    for (int i = 0; i < gpio_table_count; i++) {
        if (gpio_table[i].port == GPIOx && gpio_table[i].pin == GPIO_Pin) {
            return gpio_table[i].read_state;
        }
    }
    return GPIO_PIN_RESET;
}

void HAL_Delay(uint32_t Delay)
{
    (void)Delay; /* no-op on host */
}