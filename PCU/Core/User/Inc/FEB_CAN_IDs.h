/**
  ******************************************************************************
  * @file           : FEB_CAN_IDs.h
  * @brief          : CAN message ID definitions for PCU
  ******************************************************************************
  * @attention
  *
  * This file contains all CAN message identifiers used in the PCU firmware.
  * IDs are organized by device/subsystem for easy reference.
  *
  ******************************************************************************
  */

#ifndef __FEB_CAN_IDS_H
#define __FEB_CAN_IDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           BMS CAN MESSAGE IDs                              */
/* ========================================================================== */
#define FEB_CAN_ID_BMS_STATE (0x00u)
#define FEB_CAN_BMS_CELL_DATA_FRAME_ID (0x01u)
#define FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE (0x02u)
#define FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE (0x03u)
#define FEB_CAN_ACCUMULATOR_FAULTS_FRAME_ID (0x04u)

/* ========================================================================== */
/*                           PCU CAN MESSAGE IDs                              */
/* ========================================================================== */
#define FEB_CAN_ID_BRAKE_DATA (0x09u)
#define FEB_CAN_ID_BSPD_STATUS (0x0au)
#define FEB_CAN_ID_DASH_IO (0x10u)

/* ========================================================================== */
/*                           LVPDB CAN MESSAGE IDs                            */
/* ========================================================================== */
#define FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID (0x16u)
#define FEB_CAN_LVPDB_COOLANT_FANS_SHUTDOWN_FRAME_ID (0x17u)
#define FEB_CAN_LVPDB_AUTONOMOUS_FRAME_ID (0x18u)

/* ========================================================================== */
/*                           SENSOR CAN MESSAGE IDs                           */
/* ========================================================================== */
#define FEB_CAN_LINEAR_POTENTIOMETER_FRONT_FRAME_ID (0x1eu)
#define FEB_CAN_LINEAR_POTENTIOMETER_REAR_FRAME_ID (0x1fu)
#define FEB_CAN_FRONT_LEFT_TIRE_TEMP_FRAME_ID (0x20u)
#define FEB_CAN_FRONT_RIGHT_TIRE_TEMP_FRAME_ID (0x21u)
#define FEB_CAN_REAR_LEFT_TIRE_TEMP_FRAME_ID (0x22u)
#define FEB_CAN_REAR_RIGHT_TIRE_TEMP_FRAME_ID (0x23u)
#define FEB_CAN_WSS_FRONT_DATA_FRAME_ID (0x24u)
#define FEB_CAN_WSS_REAR_DATA_FRAME_ID (0x25u)
#define FEB_CAN_DART_TACH_MEASUREMENTS_1234_FRAME_ID (0x2du)
#define FEB_CAN_DART_TACH_MEASUREMENTS_5_FRAME_ID (0x2eu)

/* ========================================================================== */
/*                           TPS CAN MESSAGE IDs                              */
/* ========================================================================== */
#define FEB_CAN_BBB_TPS_FRAME_ID (0x34u)
#define FEB_CAN_ID_TPS_DATA (0x35u)
#define FEB_CAN_DASH_TPS_FRAME_ID (0x36u)
#define FEB_CAN_DCU_TPS_FRAME_ID (0x37u)

/* ========================================================================== */
/*                           RMS (Motor Controller) IDs                       */
/* ========================================================================== */
#define FEB_CAN_ID_RMS_VOLTAGE (0xa0u)
#define FEB_CAN_ID_RMS_MOTOR (0xa5u)
#define FEB_CAN_ID_RMS_TORQUE (0xc0u)
#define FEB_CAN_ID_RMS_PARAM (0xc1u)

/* ========================================================================== */
/*                           HEARTBEAT CAN MESSAGE IDs                        */
/* ========================================================================== */
#define FEB_CAN_PCU_HEARTBEAT_FRAME_ID (0xd0u)
#define FEB_CAN_DASH_HEARTBEAT_FRAME_ID (0xd1u)
#define FEB_CAN_LVPDB_HEARTBEAT_FRAME_ID (0xd2u)
#define FEB_CAN_DCU_HEARTBEAT_FRAME_ID (0xd3u)
#define FEB_CAN_FRONT_SENSOR_HEARTBEAT_MESSAGE_FRAME_ID (0xd4u)
#define FEB_CAN_REAR_SENSOR_HEARTBEAT_MESSAGE_FRAME_ID (0xd5u)

/* ========================================================================== */
/*                           PING PONG CAN MESSAGE IDs                        */
/* ========================================================================== */
#define FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID (0xe0u)
#define FEB_CAN_FEB_PING_PONG_COUNTER2_FRAME_ID (0xe1u)
#define FEB_CAN_FEB_PING_PONG_COUNTER3_FRAME_ID (0xe2u)
#define FEB_CAN_FEB_PING_PONG_COUNTER4_FRAME_ID (0xe3u)

/* ========================================================================== */
/*                           APPS CAN MESSAGE IDs                             */
/* ========================================================================== */
#define FEB_CAN_ID_APPS_DATA (0xffu)

#ifdef __cplusplus
}
#endif

#endif /* __FEB_CAN_IDS_H */
