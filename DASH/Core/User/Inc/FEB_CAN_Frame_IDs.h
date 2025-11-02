#ifndef INC_FEB_CAN_FRAME_IDS_H_
#define INC_FEB_CAN_FRAME_IDS_H_

// **************************************** CAN Frame IDs ****************************************
// These IDs correspond to specific CAN messages on the vehicle network

// BMS (Battery Management System) Frames
#define FEB_CAN_BMS_STATE_FRAME_ID 0x10
#define FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID 0x11
#define FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID 0x12
#define FEB_CAN_ACCUMULATOR_FAULTS_FRAME_ID 0x13

// DASH (Dashboard) Frames
#define FEB_CAN_DASH_IO_FRAME_ID 0x20
#define FEB_CAN_DASH_HEARTBEAT_FRAME_ID 0x21
#define FEB_CAN_DASH_TPS_FRAME_ID 0x22

// PCU (Pedal Control Unit) Frames
#define FEB_CAN_BRAKE_FRAME_ID 0x30

// LVPDB (Low Voltage Power Distribution Board) Frames
#define FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID 0x40

// RMS (Motor Controller) Frames
#define FEB_CAN_RMS_COMMAND_FRAME_ID 0xC0

#endif /* INC_FEB_CAN_FRAME_IDS_H_ */

