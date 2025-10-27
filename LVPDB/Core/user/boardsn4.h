#pragma once
#include "stm32f4xx_hal.h"

// For SN4 specific pin assignments and peripheral mappings (for the migration process of what I should keep)

#define BOARD_CAN               CAN1
#define BOARD_CAN_AF            GPIO_AF9_CAN1
#define BOARD_CAN_PORT          GPIO?   =
#define BOARD_CAN_RX_PIN        GPIO_PIN_?
#define BOARD_CAN_TX_PIN        GPIO_PIN_?

#define BOARD_UART              USART?
#define BOARD_I2C               I2C?

#define BOARD_TMUX_ADDR         0x70
#define BOARD_VSENSE_ADC_CH     -1
#define BOARD_TSENSE_ADC_CH     -1
