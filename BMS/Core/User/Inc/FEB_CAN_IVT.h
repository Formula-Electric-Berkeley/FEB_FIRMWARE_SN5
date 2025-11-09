// #ifndef INC_FEB_CAN_IVT_H_
// #define INC_FEB_CAN_IVT_H_

// // ********************************** CAN IVT Definitions ************************
// // Stub file for CAN and IVT (Isabellenh├╝tte current/voltage/temperature sensor)
// // IVT-S sensors are commonly used in battery management systems

// #include <stdint.h>

// // IVT sensor data structure (placeholder)
// typedef struct {
//     float current_A;
//     float voltage_V;
//     float temperature_C;
//     uint32_t timestamp_ms;
//     uint8_t valid;
// } ivt_data_t;

// // CAN message structure already defined in bms_tasks.h

// /**
//  * @brief Get pack voltage from IVT sensor (V1 measurement)
//  * @return Pack voltage in volts
//  * @note STUB: Returns placeholder value for now
//  *       TODO: Implement CAN-based IVT communication
//  */
// float FEB_IVT_V1_Voltage(void);

// /**
//  * @brief Initialize IVT sensor communication
//  * @note STUB: Does nothing for now
//  */
// void FEB_IVT_Init(void);

// #endif /* INC_FEB_CAN_IVT_H_ */
