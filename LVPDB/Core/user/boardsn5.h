#pragma once
#include "stm32f4xx_hal.h"

// ---- Pins/instances in SN5
#define BOARD_CAN               CAN1
#define BOARD_CAN_AF            GPIO_AF9_CAN1
#define BOARD_CAN_PORT          GPIOA
#define BOARD_CAN_RX_PIN        GPIO_PIN_11
#define BOARD_CAN_TX_PIN        GPIO_PIN_12

#define BOARD_UART              USART2
#define BOARD_I2C               I2C1           
#define BOARD_TMUX_ADDR         0x70            // TMUX1208 for testing

#define BOARD_VSENSE_ADC_CH     -1             
#define BOARD_TSENSE_ADC_CH     -1

