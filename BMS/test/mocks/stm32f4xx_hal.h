#ifndef MOCK_STM32F4XX_HAL_H
#define MOCK_STM32F4XX_HAL_H

#ifdef UNIT_TEST

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    HAL_OK       = 0x00U,
    HAL_ERROR    = 0x01U,
    HAL_BUSY     = 0x02U,
    HAL_TIMEOUT  = 0x03U
} HAL_StatusTypeDef;

#define ADC_CHANNEL_0    ((uint32_t)0x00000000U)
#define ADC_CHANNEL_1    ((uint32_t)0x00000001U)
#define ADC_CHANNEL_2    ((uint32_t)0x00000002U)
#define ADC_CHANNEL_3    ((uint32_t)0x00000003U)
#define ADC_CHANNEL_4    ((uint32_t)0x00000004U)
#define ADC_CHANNEL_5    ((uint32_t)0x00000005U)
#define ADC_CHANNEL_6    ((uint32_t)0x00000006U)
#define ADC_CHANNEL_7    ((uint32_t)0x00000007U)
#define ADC_CHANNEL_8    ((uint32_t)0x00000008U)
#define ADC_CHANNEL_9    ((uint32_t)0x00000009U)
#define ADC_CHANNEL_10   ((uint32_t)0x0000000AU)
#define ADC_CHANNEL_11   ((uint32_t)0x0000000BU)
#define ADC_CHANNEL_12   ((uint32_t)0x0000000CU)
#define ADC_CHANNEL_13   ((uint32_t)0x0000000DU)
#define ADC_CHANNEL_14   ((uint32_t)0x0000000EU)
#define ADC_CHANNEL_15   ((uint32_t)0x0000000FU)

#define ADC_SAMPLETIME_3CYCLES    ((uint32_t)0x00000000U)
#define ADC_SAMPLETIME_15CYCLES   ((uint32_t)0x00000001U)
#define ADC_SAMPLETIME_28CYCLES   ((uint32_t)0x00000002U)
#define ADC_SAMPLETIME_56CYCLES   ((uint32_t)0x00000003U)
#define ADC_SAMPLETIME_84CYCLES   ((uint32_t)0x00000004U)
#define ADC_SAMPLETIME_112CYCLES  ((uint32_t)0x00000005U)
#define ADC_SAMPLETIME_144CYCLES  ((uint32_t)0x00000006U)
#define ADC_SAMPLETIME_480CYCLES  ((uint32_t)0x00000007U)

typedef struct {
    void* Instance;
    void* Init;
} ADC_HandleTypeDef;

typedef struct {
    void* Instance;
    struct {
        uint32_t BaudRate;
        uint32_t WordLength;
        uint32_t StopBits;
        uint32_t Parity;
        uint32_t Mode;
        uint32_t HwFlowCtl;
        uint32_t OverSampling;
    } Init;
} UART_HandleTypeDef;

// GPIO definitions
typedef struct {
    uint32_t dummy;
} GPIO_TypeDef;

typedef enum {
    GPIO_PIN_RESET = 0U,
    GPIO_PIN_SET
} GPIO_PinState;

#define GPIO_PIN_0    ((uint16_t)0x0001U)
#define GPIO_PIN_1    ((uint16_t)0x0002U)
#define GPIO_PIN_2    ((uint16_t)0x0004U)
#define GPIO_PIN_3    ((uint16_t)0x0008U)
#define GPIO_PIN_4    ((uint16_t)0x0010U)
#define GPIO_PIN_5    ((uint16_t)0x0020U)
#define GPIO_PIN_6    ((uint16_t)0x0040U)
#define GPIO_PIN_7    ((uint16_t)0x0080U)
#define GPIO_PIN_8    ((uint16_t)0x0100U)
#define GPIO_PIN_9    ((uint16_t)0x0200U)
#define GPIO_PIN_10   ((uint16_t)0x0400U)
#define GPIO_PIN_11   ((uint16_t)0x0800U)
#define GPIO_PIN_12   ((uint16_t)0x1000U)
#define GPIO_PIN_13   ((uint16_t)0x2000U)
#define GPIO_PIN_14   ((uint16_t)0x4000U)
#define GPIO_PIN_15   ((uint16_t)0x8000U)

extern GPIO_TypeDef GPIOA_mock, GPIOB_mock, GPIOC_mock, GPIOD_mock;
#define GPIOA (&GPIOA_mock)
#define GPIOB (&GPIOB_mock)
#define GPIOC (&GPIOC_mock)
#define GPIOD (&GPIOD_mock)

// SPI definitions
typedef struct {
    void* Instance;
    void* Init;
} SPI_HandleTypeDef;

// CAN definitions
typedef struct {
    void* Instance;
    void* Init;
} CAN_HandleTypeDef;

typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    uint32_t Timestamp;
    uint32_t FilterMatchIndex;
} CAN_RxHeaderTypeDef;

typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    uint32_t TransmitGlobalTime;
} CAN_TxHeaderTypeDef;

typedef struct {
    uint32_t FilterIdHigh;
    uint32_t FilterIdLow;
    uint32_t FilterMaskIdHigh;
    uint32_t FilterMaskIdLow;
    uint32_t FilterFIFOAssignment;
    uint32_t FilterBank;
    uint32_t FilterMode;
    uint32_t FilterScale;
    uint32_t FilterActivation;
    uint32_t SlaveStartFilterBank;
} CAN_FilterTypeDef;

#define CAN_FILTER_ENABLE           ((uint32_t)0x00000001U)
#define CAN_FILTER_DISABLE          ((uint32_t)0x00000000U)
#define CAN_FILTERMODE_IDMASK       ((uint32_t)0x00000000U)
#define CAN_FILTERMODE_IDLIST       ((uint32_t)0x00000001U)
#define CAN_FILTERSCALE_16BIT       ((uint32_t)0x00000000U)
#define CAN_FILTERSCALE_32BIT       ((uint32_t)0x00000001U)

// CAN FIFO definitions
#define CAN_RX_FIFO0                ((uint32_t)0x00000000U)
#define CAN_RX_FIFO1                ((uint32_t)0x00000001U)

// CAN ID type definitions
#define CAN_ID_STD                  ((uint32_t)0x00000000U)
#define CAN_ID_EXT                  ((uint32_t)0x00000004U)

// CAN RTR type definitions
#define CAN_RTR_DATA                ((uint32_t)0x00000000U)
#define CAN_RTR_REMOTE              ((uint32_t)0x00000002U)

// CAN interrupt definitions
#define CAN_IT_RX_FIFO0_MSG_PENDING ((uint32_t)0x00000004U)

// General HAL constants
#ifndef DISABLE
#define DISABLE 0U
#endif
#ifndef ENABLE  
#define ENABLE  1U
#endif

extern SPI_HandleTypeDef hspi1;
extern CAN_HandleTypeDef hcan1;

typedef struct {
    uint32_t Channel;
    uint32_t Rank;
    uint32_t SamplingTime;
    uint32_t Offset;
} ADC_ChannelConfTypeDef;

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;
extern uint32_t mock_adc_value;
extern HAL_StatusTypeDef mock_hal_status;

#define HAL_MAX_DELAY 0xFFFFFFFFU

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef* hadc, ADC_ChannelConfTypeDef* sConfig);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef* hadc);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef* hadc);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef* hadc, uint32_t Timeout);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef* hadc);

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart);

// GPIO functions
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

// SPI functions
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout);

// CAN functions
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, CAN_FilterTypeDef *sFilterConfig);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *pHeader, 
                                       uint8_t aData[], uint32_t *pTxMailbox);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan, uint32_t ActiveITs);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan, uint32_t RxFifo, CAN_RxHeaderTypeDef *pHeader, uint8_t aData[]);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *hcan);

// Timing functions
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t Delay);

// Essential Mock_* functions for test setup only
void Mock_SetHALStatus(HAL_StatusTypeDef status);
void Mock_SetTick(uint32_t tick);
void Mock_SetGPIOReadValue(GPIO_PinState value);
void Mock_SetGPIOReadValueForPin(uint16_t pin, GPIO_PinState value);
void Mock_SetADCValue(uint32_t value);
void Mock_SetCANStatus(HAL_StatusTypeDef status);
void Mock_SetTxMailboxesFree(uint32_t count);
void Mock_ClearUARTBuffer(void);
uint8_t* Mock_GetUARTBuffer(void);
uint32_t Mock_GetUARTBufferSize(void);

// Additional Mock_* functions needed by tests
void Mock_ResetAll(void);
void Mock_UART_ClearBuffer(void);
uint8_t* Mock_UART_GetBuffer(void);
uint32_t Mock_UART_GetBufferSize(void);
void Mock_HAL_SetTick(uint32_t tick);
void Mock_SetTickValue(uint32_t tick);
void Mock_ResetCounters(void);
uint32_t Mock_HAL_GetWriteCount(void);
void Mock_SPI_Reset(void);

// Additional mock functions needed by tests
void Mock_SetSPIWriteResult(bool result);
void Mock_SetSPIReadResult(bool result);
bool Mock_GetSPIWriteCalled(void);
bool Mock_GetSPIWriteReadCalled(void);
bool Mock_GetGPIOWriteCalled(void);
void Mock_SetGPIOReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
bool Mock_GPIO_WasPinReset(uint16_t pin);
bool Mock_GPIO_WasPinSet(uint16_t pin);
uint32_t Mock_GPIO_GetPinResetCount(uint16_t pin);
void Mock_GPIO_SetPinState(uint16_t pin, uint8_t state);
uint32_t Mock_HAL_GetSPITransmitCount(void);
uint32_t Mock_HAL_GetSPIReceiveCount(void);
const uint8_t* Mock_HAL_GetLastSPIData(void);
void Mock_HAL_SetSPIReceiveData(const uint8_t* data, uint32_t size);
GPIO_PinState Mock_HAL_GetLastWriteState(void);
uint32_t Mock_HAL_GetToggleCount(void);
uint32_t Mock_HAL_GetReadCount(void);
void Mock_SetSPIReadResponse(uint8_t response);

// ADBMS mock functions
void Mock_SetADBMSCellVoltage(float voltage);
void Mock_SetADBMSCellVoltageS(float voltage);
void Mock_SetADBMSCellTemperature(float temperature);
void Mock_SetADBMSTotalVoltage(float voltage);
void Mock_SetADBMSCellDischarging(bool discharging);
void Mock_SetADBMSAirSenseValues(bool airp, bool airm);
void Mock_SetADBMSErrorType(uint8_t error_type);
void Mock_SetADBMSAvgTemperature(float temperature);
void Mock_SetADBMSMinTemperature(float temperature);
void Mock_SetADBMSMaxTemperature(float temperature);
void Mock_SetADBMSMinVoltage(float voltage);
void Mock_SetADBMSMaxVoltage(float voltage);

// IVT mock functions
void Mock_IVT_SetVoltage(float voltage);
void Mock_SetIVTCurrent(int32_t current);
void Mock_SetIVTVoltage1(int32_t voltage);
void Mock_SetIVTVoltage2(int32_t voltage);
void Mock_SetIVTVoltage3(int32_t voltage);

// Charger mock functions
void Mock_SetChargerBMSMessage(int32_t voltage, int32_t current, uint8_t control);
void Mock_SetChargerCCSMessage(int32_t voltage, int32_t current, uint8_t status, bool received);

// SM mock functions
void Mock_SetSMCurrentState(uint8_t state);
bool Mock_SM_GetFaultTriggered(void);
void Mock_SM_ResetFaultTrigger(void);
uint8_t Mock_SM_GetLastTransition(void);

// Pin read functions
void Mock_SetPinRead(uint16_t pin, uint8_t state);

// Additional missing mock functions
void Mock_SetPrechargeComplete(bool complete);
void Mock_SetADCValueS(float value);
void Mock_ADBMS_SetTotalVoltage(float voltage);
void Mock_ADBMS_SetMinVoltage(float voltage);
void Mock_ADBMS_SetMaxVoltage(float voltage);
void Mock_ADBMS_SetAvgTemperature(float temperature);
void Mock_ADBMS_SetMinTemperature(float temperature);
void Mock_ADBMS_SetMaxTemperature(float temperature);
void Mock_ADBMS_SetErrorType(uint8_t error_type);
void Mock_SM_SetCurrentState(uint8_t state);
void Mock_CAN_SetFilterConfigResult(HAL_StatusTypeDef result);
void Mock_CAN_Heartbeat_SetInitialized(uint8_t device, uint8_t count);
void Mock_CAN_Heartbeat_SetLastReceived(uint8_t device, uint32_t time);
bool Mock_CAN_Heartbeat_GetResetCalled(void);
bool Mock_CAN_Heartbeat_GetIncrementLaOnCalled(void);
void Mock_CAN_DASH_SetR2D(bool r2d);
void Mock_SetChargerReceived(bool received);
void Mock_SetChargingStatus(bool status);
void Mock_GPIO_Reset(void);
void Mock_SetCANTxFreeLevel(uint32_t level);
void Mock_SetCANAddTxMessageResult(HAL_StatusTypeDef result);
bool Mock_GetCANConfigFilterCalled(void);
bool Mock_GetCANAddTxMessageCalled(void);
void Mock_SetCANTxFreeLevelSequence(uint32_t* levels, uint32_t count);

// Stub for missing user function
uint8_t FEB_CAN_Template(CAN_HandleTypeDef* hcan, uint32_t fifo, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif // UNIT_TEST

#endif