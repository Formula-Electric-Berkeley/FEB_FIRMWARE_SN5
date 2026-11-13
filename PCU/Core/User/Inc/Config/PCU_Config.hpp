#pragma once

#include <cstdint>

// Constants Needed
constexpr uint16_t MAX_TORQUE = 225;

constexpr float APPS_IMPLAUSIBILITY_PCT = 0.10f;
constexpr float APPS_RANGE_MARGIN_PCT = 0.10f;
constexpr float BRAKE_PRESSED_PCT = 0.10f;

constexpr uint32_t PCU_SAMPLE_PERIOD_MS = 10;
constexpr uint32_t APPS_IMPLAUSIBILITY_TIME_MS = 100;

constexpr uint16_t UART_TX_BUF_SIZE = 512;
constexpr uint16_t UART_RX_BUF_SIZE = 256;

constexpr float ADC_VREF = 3.3f;
constexpr uint16_t ADC_MAX_COUNTS = 4095;
constexpr float DIVIDER_RATIO = 3.3f / (1.69f + 3.3f);

constexpr float BRAKE_SENSOR_MIN_V = 0.5f;
constexpr float BRAKE_SENSOR_MAX_V = 4.5f;
constexpr uint16_t BRAKE_MIN_VALID_RAW = BRAKE_SENSOR_MIN_V * DIVIDER_RATIO / ADC_VREF * ADC_MAX_COUNTS;
constexpr uint16_t BRAKE_MAX_VALID_RAW = BRAKE_SENSOR_MAX_V * DIVIDER_RATIO / ADC_VREF * ADC_MAX_COUNTS;

// Calibration
constexpr uint16_t MIN_APPS1 = 3226;
constexpr uint16_t MAX_APPS1 = 2717;

constexpr uint16_t MIN_APPS2 = 1328;
constexpr uint16_t MAX_APPS2 = 2085;

constexpr uint16_t MIN_BRAKE1 = 410;
constexpr uint16_t MAX_BRAKE1 = 3691;

constexpr uint16_t MIN_BRAKE2 = 410;
constexpr uint16_t MAX_BRAKE2 = 3691;
