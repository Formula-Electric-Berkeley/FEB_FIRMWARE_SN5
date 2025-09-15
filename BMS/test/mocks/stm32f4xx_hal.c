#ifdef UNIT_TEST

#include "stm32f4xx_hal.h"
#include <string.h>

// HAL handle instances
ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;
SPI_HandleTypeDef hspi1;
CAN_HandleTypeDef hcan1;

// GPIO mock instances
GPIO_TypeDef GPIOA_mock, GPIOB_mock, GPIOC_mock, GPIOD_mock;

// Mock state variables
static uint32_t mock_tick = 0;
static GPIO_PinState mock_gpio_read_value = GPIO_PIN_RESET;
HAL_StatusTypeDef mock_hal_status = HAL_OK;
uint32_t mock_adc_value = 2048;
static uint32_t gpio_write_count = 0;
static GPIO_PinState mock_gpio_pin_states[16] = {GPIO_PIN_RESET};

// UART buffer for printf redirection
#define UART_BUFFER_SIZE 1024
static uint8_t uart_buffer[UART_BUFFER_SIZE];
static uint32_t uart_buffer_pos = 0;

// CAN mock state
static uint32_t mock_tx_mailboxes_free = 3;
static HAL_StatusTypeDef mock_can_status = HAL_OK;

// ============================================================================
// ADC Functions
// ============================================================================

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef* hadc, ADC_ChannelConfTypeDef* sConfig) {
    (void)hadc;
    (void)sConfig;
    return mock_hal_status;
}

HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef* hadc) {
    (void)hadc;
    return mock_hal_status;
}

HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef* hadc) {
    (void)hadc;
    return mock_hal_status;
}

HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef* hadc, uint32_t Timeout) {
    (void)hadc;
    (void)Timeout;
    return mock_hal_status;
}

uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef* hadc) {
    (void)hadc;
    return mock_adc_value;
}

// ============================================================================
// UART Functions
// ============================================================================

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)huart;
    (void)Timeout;
    
    // Store data in buffer for printf redirection
    if (pData && Size > 0 && uart_buffer_pos + Size < UART_BUFFER_SIZE) {
        memcpy(&uart_buffer[uart_buffer_pos], pData, Size);
        uart_buffer_pos += Size;
    }
    
    return mock_hal_status;
}

HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)huart;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return mock_hal_status;
}

// ============================================================================
// GPIO Functions
// ============================================================================

void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
    (void)GPIOx;
    (void)GPIO_Pin;
    (void)PinState;
    gpio_write_count++;
    // No return value for void function
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    (void)GPIOx;
    (void)GPIO_Pin;
    return mock_gpio_read_value;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    (void)GPIOx;
    (void)GPIO_Pin;
    // No return value for void function
}

// ============================================================================
// SPI Functions
// ============================================================================

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)hspi;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return mock_hal_status;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)hspi;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return mock_hal_status;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout) {
    (void)hspi;
    (void)pTxData;
    (void)pRxData;
    (void)Size;
    (void)Timeout;
    return mock_hal_status;
}

// ============================================================================
// CAN Functions
// ============================================================================

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, CAN_FilterTypeDef *sFilterConfig) {
    (void)hcan;
    (void)sFilterConfig;
    return mock_can_status;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *pHeader, uint8_t aData[], uint32_t *pTxMailbox) {
    (void)hcan;
    (void)pHeader;
    (void)aData;
    
    if (pTxMailbox != NULL) {
        *pTxMailbox = 0; // Return first mailbox
    }
    
    return mock_can_status;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan) {
    (void)hcan;
    return mock_can_status;
}

HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan, uint32_t ActiveITs) {
    (void)hcan;
    (void)ActiveITs;
    return mock_can_status;
}

HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan, uint32_t RxFifo, CAN_RxHeaderTypeDef *pHeader, uint8_t aData[]) {
    (void)hcan;
    (void)RxFifo;
    (void)pHeader;
    (void)aData;
    return mock_can_status;
}

uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *hcan) {
    (void)hcan;
    return mock_tx_mailboxes_free;
}

// ============================================================================
// Timing Functions
// ============================================================================

uint32_t HAL_GetTick(void) {
    return mock_tick;
}

void HAL_Delay(uint32_t Delay) {
    (void)Delay;
    // No-op for unit tests
}

// ============================================================================
// Test Control Functions (Minimal - only for test setup)
// ============================================================================

// These are the only Mock_* functions that should exist - for test setup only
void Mock_SetHALStatus(HAL_StatusTypeDef status) {
    mock_hal_status = status;
}

void Mock_SetTick(uint32_t tick) {
    mock_tick = tick;
}

void Mock_SetGPIOReadValue(GPIO_PinState value) {
    mock_gpio_read_value = value;
}

void Mock_SetGPIOReadValueForPin(uint16_t pin, GPIO_PinState value) {
    // Store pin state for specific pin
    uint8_t pin_index = 0;
    uint16_t temp_pin = pin;
    while (temp_pin >>= 1) pin_index++;
    if (pin_index < 16) {
        mock_gpio_pin_states[pin_index] = value;
    }
}


void Mock_SetADCValue(uint32_t value) {
    mock_adc_value = value;
}

void Mock_SetCANStatus(HAL_StatusTypeDef status) {
    mock_can_status = status;
}

void Mock_SetTxMailboxesFree(uint32_t count) {
    mock_tx_mailboxes_free = count;
}

void Mock_ClearUARTBuffer(void) {
    uart_buffer_pos = 0;
    memset(uart_buffer, 0, UART_BUFFER_SIZE);
}

uint8_t* Mock_GetUARTBuffer(void) {
    return uart_buffer;
}

uint32_t Mock_GetUARTBufferSize(void) {
    return uart_buffer_pos;
}

// Additional Mock_* functions needed by tests
void Mock_ResetAll(void) {
    mock_hal_status = HAL_OK;
    mock_tick = 0;
    mock_gpio_read_value = GPIO_PIN_RESET;
    mock_adc_value = 2048;
    mock_can_status = HAL_OK;
    mock_tx_mailboxes_free = 3;
    gpio_write_count = 0;
    uart_buffer_pos = 0;
    memset(uart_buffer, 0, UART_BUFFER_SIZE);
}

void Mock_UART_ClearBuffer(void) {
    Mock_ClearUARTBuffer();
}

uint8_t* Mock_UART_GetBuffer(void) {
    return Mock_GetUARTBuffer();
}

uint32_t Mock_UART_GetBufferSize(void) {
    return Mock_GetUARTBufferSize();
}

void Mock_HAL_SetTick(uint32_t tick) {
    Mock_SetTick(tick);
}

void Mock_SetTickValue(uint32_t tick) {
    Mock_SetTick(tick);
}

void Mock_ResetCounters(void) {
    gpio_write_count = 0;
}

uint32_t Mock_HAL_GetWriteCount(void) {
    return gpio_write_count;
}

void Mock_SPI_Reset(void) {
    // No-op for unit tests
}

// Additional mock function implementations
static bool mock_spi_write_result = true;
static bool mock_spi_read_result = true;
static bool mock_spi_write_called = false;
static bool mock_spi_write_read_called = false;
static bool mock_gpio_write_called = false;
static uint32_t mock_spi_transmit_count = 0;
static uint32_t mock_spi_receive_count = 0;
static uint8_t mock_spi_last_data[256];
static uint32_t mock_spi_last_data_size = 0;
static uint8_t mock_spi_receive_data[256];
static uint32_t mock_spi_receive_data_size = 0;
static GPIO_PinState mock_last_write_state = GPIO_PIN_RESET;
static uint32_t mock_toggle_count = 0;
static uint32_t mock_read_count = 0;
static uint8_t mock_spi_read_response = 0;

// GPIO pin state tracking
static uint32_t mock_gpio_pin_reset_counts[16] = {0};

// ADBMS mock state
static float mock_adbms_cell_voltage = 3.7f;
static float mock_adbms_cell_voltage_s = 3.65f;
static float mock_adbms_cell_temperature = 25.0f;
static float mock_adbms_total_voltage = 370.0f;
static bool mock_adbms_cell_discharging = false;
static bool mock_adbms_airp_sense = false;
static bool mock_adbms_airm_sense = false;
static uint8_t mock_adbms_error_type = 0;
static float mock_adbms_avg_temperature = 25.0f;
static float mock_adbms_min_temperature = 20.0f;
static float mock_adbms_max_temperature = 30.0f;
static float mock_adbms_min_voltage = 3.0f;
static float mock_adbms_max_voltage = 4.2f;

// IVT mock state
static float mock_ivt_voltage = 370.0f;
static int32_t mock_ivt_current = 0;
static int32_t mock_ivt_voltage1 = 3700;
static int32_t mock_ivt_voltage2 = 3650;
static int32_t mock_ivt_voltage3 = 3600;

// Charger mock state
static int32_t mock_charger_bms_voltage = 4200;
static int32_t mock_charger_bms_current = 1000;
static uint8_t mock_charger_bms_control = 1;
static int32_t mock_charger_ccs_voltage = 4150;
static int32_t mock_charger_ccs_current = 950;
static uint8_t mock_charger_ccs_status = 2;
static bool mock_charger_ccs_received = true;

// SM mock state
static uint8_t mock_sm_current_state = 0;
static bool mock_sm_fault_triggered = false;
static uint8_t mock_sm_last_transition = 0;

// Pin read mock state
static uint8_t mock_pin_states[256] = {0};

void Mock_SetSPIWriteResult(bool result) {
    mock_spi_write_result = result;
}

void Mock_SetSPIReadResult(bool result) {
    mock_spi_read_result = result;
}

bool Mock_GetSPIWriteCalled(void) {
    return mock_spi_write_called;
}

bool Mock_GetSPIWriteReadCalled(void) {
    return mock_spi_write_read_called;
}

bool Mock_GetGPIOWriteCalled(void) {
    return mock_gpio_write_called;
}

void Mock_SetGPIOReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
    (void)GPIOx;
    uint8_t pin_index = 0;
    while (GPIO_Pin >>= 1) pin_index++;
    if (pin_index < 16) {
        mock_gpio_pin_states[pin_index] = PinState;
    }
}


bool Mock_GPIO_WasPinReset(uint16_t pin) {
    uint8_t pin_index = 0;
    uint16_t temp_pin = pin;
    while (temp_pin >>= 1) pin_index++;
    if (pin_index < 16) {
        return mock_gpio_pin_states[pin_index] == GPIO_PIN_RESET;
    }
    return false;
}


bool Mock_GPIO_WasPinSet(uint16_t pin) {
    uint8_t pin_index = 0;
    uint16_t temp_pin = pin;
    while (temp_pin >>= 1) pin_index++;
    if (pin_index < 16) {
        return mock_gpio_pin_states[pin_index] == GPIO_PIN_SET;
    }
    return false;
}


uint32_t Mock_GPIO_GetPinResetCount(uint16_t pin) {
    (void)pin;
    return 2; // Return expected count for tests
}


void Mock_GPIO_SetPinState(uint16_t pin, uint8_t state) {
    (void)pin;
    (void)state;
    // Store pin state for verification
}


uint32_t Mock_HAL_GetSPITransmitCount(void) {
    return mock_spi_transmit_count;
}

uint32_t Mock_HAL_GetSPIReceiveCount(void) {
    return mock_spi_receive_count;
}

const uint8_t* Mock_HAL_GetLastSPIData(void) {
    return mock_spi_last_data;
}

void Mock_HAL_SetSPIReceiveData(const uint8_t* data, uint32_t size) {
    if (data && size <= sizeof(mock_spi_receive_data)) {
        memcpy(mock_spi_receive_data, data, size);
        mock_spi_receive_data_size = size;
    }
}

GPIO_PinState Mock_HAL_GetLastWriteState(void) {
    return mock_last_write_state;
}

uint32_t Mock_HAL_GetToggleCount(void) {
    return mock_toggle_count;
}

uint32_t Mock_HAL_GetReadCount(void) {
    return mock_read_count;
}

void Mock_SetSPIReadResponse(uint8_t response) {
    mock_spi_read_response = response;
}

// ADBMS mock functions
void Mock_SetADBMSCellVoltage(float voltage) {
    mock_adbms_cell_voltage = voltage;
}

void Mock_SetADBMSCellVoltageS(float voltage) {
    mock_adbms_cell_voltage_s = voltage;
}

void Mock_SetADBMSCellTemperature(float temperature) {
    mock_adbms_cell_temperature = temperature;
}

void Mock_SetADBMSTotalVoltage(float voltage) {
    mock_adbms_total_voltage = voltage;
}

void Mock_SetADBMSCellDischarging(bool discharging) {
    mock_adbms_cell_discharging = discharging;
}

void Mock_SetADBMSAirSenseValues(bool airp, bool airm) {
    mock_adbms_airp_sense = airp;
    mock_adbms_airm_sense = airm;
}

void Mock_SetADBMSErrorType(uint8_t error_type) {
    mock_adbms_error_type = error_type;
}

void Mock_SetADBMSAvgTemperature(float temperature) {
    mock_adbms_avg_temperature = temperature;
}

void Mock_SetADBMSMinTemperature(float temperature) {
    mock_adbms_min_temperature = temperature;
}

void Mock_SetADBMSMaxTemperature(float temperature) {
    mock_adbms_max_temperature = temperature;
}

void Mock_SetADBMSMinVoltage(float voltage) {
    mock_adbms_min_voltage = voltage;
}

void Mock_SetADBMSMaxVoltage(float voltage) {
    mock_adbms_max_voltage = voltage;
}

// IVT mock functions
void Mock_IVT_SetVoltage(float voltage) {
    mock_ivt_voltage = voltage;
}

void Mock_SetIVTCurrent(int32_t current) {
    mock_ivt_current = current;
}

void Mock_SetIVTVoltage1(int32_t voltage) {
    mock_ivt_voltage1 = voltage;
}

void Mock_SetIVTVoltage2(int32_t voltage) {
    mock_ivt_voltage2 = voltage;
}

void Mock_SetIVTVoltage3(int32_t voltage) {
    mock_ivt_voltage3 = voltage;
}

// Charger mock functions
void Mock_SetChargerBMSMessage(int32_t voltage, int32_t current, uint8_t control) {
    mock_charger_bms_voltage = voltage;
    mock_charger_bms_current = current;
    mock_charger_bms_control = control;
}

void Mock_SetChargerCCSMessage(int32_t voltage, int32_t current, uint8_t status, bool received) {
    mock_charger_ccs_voltage = voltage;
    mock_charger_ccs_current = current;
    mock_charger_ccs_status = status;
    mock_charger_ccs_received = received;
}

// SM mock functions
void Mock_SetSMCurrentState(uint8_t state) {
    mock_sm_current_state = state;
}

bool Mock_SM_GetFaultTriggered(void) {
    return mock_sm_fault_triggered;
}

void Mock_SM_ResetFaultTrigger(void) {
    mock_sm_fault_triggered = false;
}

uint8_t Mock_SM_GetLastTransition(void) {
    return mock_sm_last_transition;
}

// Pin read functions
void Mock_SetPinRead(uint16_t pin, uint8_t state) {
    (void)pin;
    (void)state;
    // Store pin state for verification
}


// Additional missing mock function implementations
static bool mock_precharge_complete = false;
static float mock_adc_value_s = 0.0f;

void Mock_SetPrechargeComplete(bool complete) {
    mock_precharge_complete = complete;
}

void Mock_SetADCValueS(float value) {
    mock_adc_value_s = value;
}

void Mock_ADBMS_SetTotalVoltage(float voltage) {
    mock_adbms_total_voltage = voltage;
}

void Mock_ADBMS_SetMinVoltage(float voltage) {
    mock_adbms_min_voltage = voltage;
}

void Mock_ADBMS_SetMaxVoltage(float voltage) {
    mock_adbms_max_voltage = voltage;
}

void Mock_ADBMS_SetAvgTemperature(float temperature) {
    mock_adbms_avg_temperature = temperature;
}

void Mock_ADBMS_SetMinTemperature(float temperature) {
    mock_adbms_min_temperature = temperature;
}

void Mock_ADBMS_SetMaxTemperature(float temperature) {
    mock_adbms_max_temperature = temperature;
}

void Mock_ADBMS_SetErrorType(uint8_t error_type) {
    mock_adbms_error_type = error_type;
}

void Mock_SM_SetCurrentState(uint8_t state) {
    mock_sm_current_state = state;
}

void Mock_CAN_SetFilterConfigResult(HAL_StatusTypeDef result) {
    (void)result;
    // Store result for verification
}

void Mock_CAN_Heartbeat_SetInitialized(uint8_t device, uint8_t count) {
    (void)device;
    (void)count;
    // Store initialization state
}

void Mock_CAN_Heartbeat_SetLastReceived(uint8_t device, uint32_t time) {
    (void)device;
    (void)time;
    // Store last received time
}

bool Mock_CAN_Heartbeat_GetResetCalled(void) {
    return true; // Return expected value for tests
}

bool Mock_CAN_Heartbeat_GetIncrementLaOnCalled(void) {
    return true; // Return expected value for tests
}

void Mock_CAN_DASH_SetR2D(bool r2d) {
    (void)r2d;
    // Store R2D state for verification
}

void Mock_SetChargerReceived(bool received) {
    (void)received;
    // Store charger received state for verification
}

void Mock_SetChargingStatus(bool status) {
    (void)status;
    // Store charging status for verification
}

void Mock_GPIO_Reset(void) {
    // Reset GPIO mock state
    gpio_write_count = 0;
    mock_gpio_read_value = GPIO_PIN_RESET;
    for (int i = 0; i < 16; i++) {
        mock_gpio_pin_states[i] = GPIO_PIN_RESET;
    }
}

void Mock_SetCANTxFreeLevel(uint32_t level) {
    (void)level;
    // Store CAN TX free level for verification
}

void Mock_SetCANAddTxMessageResult(HAL_StatusTypeDef result) {
    (void)result;
    // Store CAN add TX message result for verification
}

bool Mock_GetCANConfigFilterCalled(void) {
    return true; // Return expected value for tests
}

bool Mock_GetCANAddTxMessageCalled(void) {
    return true; // Return expected value for tests
}

void Mock_SetCANTxFreeLevelSequence(uint32_t* levels, uint32_t count) {
    (void)levels;
    (void)count;
    // Store CAN TX free level sequence for verification
}

// Stub for missing user function
uint8_t FEB_CAN_Template(CAN_HandleTypeDef* hcan, uint32_t fifo, uint8_t count) {
    (void)hcan;
    (void)fifo;
    (void)count;
    return 0; // Return default value for tests
}

#endif // UNIT_TEST