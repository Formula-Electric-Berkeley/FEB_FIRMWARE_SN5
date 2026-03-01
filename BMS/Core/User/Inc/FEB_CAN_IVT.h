/**
 * @file FEB_CAN_IVT.h
 * @brief IVT (Isabellenhutte) Current/Voltage Sensor CAN Interface
 * @author Formula Electric @ Berkeley
 *
 * Receives IVT-S sensor data over CAN:
 * - 0x521: Current
 * - 0x522: Voltage 1 (pack voltage, used for precharge monitoring)
 * - 0x523: Voltage 2
 * - 0x524: Voltage 3
 * - 0x525: Temperature
 *
 * The IVT sensor provides high-accuracy current and voltage measurements
 * for battery management and motor control systems.
 */

#ifndef INC_FEB_CAN_IVT_H_
#define INC_FEB_CAN_IVT_H_

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * IVT CAN Message IDs (Isabellenhutte IVT-S standard)
 * ============================================================================ */

#define FEB_CAN_ID_IVT_COMMAND 0x411
#define FEB_CAN_ID_IVT_DEBUG 0x510
#define FEB_CAN_ID_IVT_RESPONSE 0x511
#define FEB_CAN_ID_IVT_CURRENT 0x521
#define FEB_CAN_ID_IVT_VOLTAGE_1 0x522
#define FEB_CAN_ID_IVT_VOLTAGE_2 0x523
#define FEB_CAN_ID_IVT_VOLTAGE_3 0x524
#define FEB_CAN_ID_IVT_TEMPERATURE 0x525
#define FEB_CAN_ID_IVT_POWER 0x526
#define FEB_CAN_ID_IVT_CURRENT_COUNTER 0x527
#define FEB_CAN_ID_IVT_ENERGY_COUNTER 0x528

/* ============================================================================
 * IVT Data Structure
 * ============================================================================ */

typedef struct
{
  volatile float current_mA;      /* Pack current in milliamps */
  volatile float voltage_1_mV;    /* Voltage 1 in millivolts (pack voltage) */
  volatile float voltage_2_mV;    /* Voltage 2 in millivolts */
  volatile float voltage_3_mV;    /* Voltage 3 in millivolts */
  volatile float temperature_C;   /* Temperature in Celsius */
  volatile uint32_t last_rx_tick; /* Timestamp of last received message */
} FEB_CAN_IVT_Data_t;

/* ============================================================================
 * Public Interface
 * ============================================================================ */

/**
 * @brief Initialize IVT CAN reception
 * @note Registers callbacks for IVT CAN message IDs
 */
void FEB_CAN_IVT_Init(void);

/**
 * @brief Get pack voltage from IVT sensor (V1)
 * @return Pack voltage in volts
 * @note Returns 0.0 if data is stale (>1000ms old)
 */
float FEB_CAN_IVT_GetVoltage(void);

/**
 * @brief Get pack current from IVT sensor
 * @return Pack current in amps
 */
float FEB_CAN_IVT_GetCurrent(void);

/**
 * @brief Get temperature from IVT sensor
 * @return Temperature in Celsius
 */
float FEB_CAN_IVT_GetTemperature(void);

/**
 * @brief Check if IVT data is fresh
 * @param timeout_ms Maximum age of data in milliseconds
 * @return true if data is fresh, false if stale or never received
 */
bool FEB_CAN_IVT_IsDataFresh(uint32_t timeout_ms);

/**
 * @brief Get raw IVT data structure
 * @return Pointer to IVT data (read-only)
 */
const FEB_CAN_IVT_Data_t *FEB_CAN_IVT_GetData(void);

#endif /* INC_FEB_CAN_IVT_H_ */
