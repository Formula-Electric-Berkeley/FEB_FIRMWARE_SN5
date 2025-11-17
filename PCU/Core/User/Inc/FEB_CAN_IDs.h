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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           BMS CAN MESSAGE IDs                              */
/* ========================================================================== */
#define FEB_CAN_ID_BMS_STATE ((uint32_t)0x00)
#define FEB_CAN_BMS_CELL_DATA_FRAME_ID ((uint32_t)0x01)
#define FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE ((uint32_t)0x02)
#define FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE ((uint32_t)0x03)
#define FEB_CAN_ACCUMULATOR_FAULTS_FRAME_ID ((uint32_t)0x04)

/* ========================================================================== */
/*                           PCU CAN MESSAGE IDs                              */
/* ========================================================================== */
#define FEB_CAN_ID_BRAKE_DATA ((uint32_t)0x09)
#define FEB_CAN_ID_BSPD_STATUS ((uint32_t)0x0a)
#define FEB_CAN_ID_DASH_IO ((uint32_t)0x10)

/* ========================================================================== */
/*                           LVPDB CAN MESSAGE IDs                            */
/* ========================================================================== */
#define FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID ((uint32_t)0x16)
#define FEB_CAN_LVPDB_COOLANT_FANS_SHUTDOWN_FRAME_ID ((uint32_t)0x17)
#define FEB_CAN_LVPDB_AUTONOMOUS_FRAME_ID ((uint32_t)0x18)

/* ========================================================================== */
/*                           SENSOR CAN MESSAGE IDs                           */
/* ========================================================================== */
#define FEB_CAN_LINEAR_POTENTIOMETER_FRONT_FRAME_ID ((uint32_t)0x1e)
#define FEB_CAN_LINEAR_POTENTIOMETER_REAR_FRAME_ID ((uint32_t)0x1f)
#define FEB_CAN_FRONT_LEFT_TIRE_TEMP_FRAME_ID ((uint32_t)0x20)
#define FEB_CAN_FRONT_RIGHT_TIRE_TEMP_FRAME_ID ((uint32_t)0x21)
#define FEB_CAN_REAR_LEFT_TIRE_TEMP_FRAME_ID ((uint32_t)0x22)
#define FEB_CAN_REAR_RIGHT_TIRE_TEMP_FRAME_ID ((uint32_t)0x23)
#define FEB_CAN_WSS_FRONT_DATA_FRAME_ID ((uint32_t)0x24)
#define FEB_CAN_WSS_REAR_DATA_FRAME_ID ((uint32_t)0x25)
#define FEB_CAN_DART_TACH_MEASUREMENTS_1234_FRAME_ID ((uint32_t)0x2d)
#define FEB_CAN_DART_TACH_MEASUREMENTS_5_FRAME_ID ((uint32_t)0x2e)

/* ========================================================================== */
/*                           TPS CAN MESSAGE IDs                              */
/* ========================================================================== */
#define FEB_CAN_BBB_TPS_FRAME_ID ((uint32_t)0x34)
#define FEB_CAN_ID_TPS_DATA ((uint32_t)0x35)
#define FEB_CAN_DASH_TPS_FRAME_ID ((uint32_t)0x36)
#define FEB_CAN_DCU_TPS_FRAME_ID ((uint32_t)0x37)

/* ========================================================================== */
/*                           RMS (Motor Controller) IDs                       */
/* ========================================================================== */
#define FEB_CAN_ID_RMS_VOLTAGE ((uint32_t)0xa0)
#define FEB_CAN_ID_RMS_MOTOR ((uint32_t)0xa5)
#define FEB_CAN_ID_RMS_TORQUE ((uint32_t)0xc0)
#define FEB_CAN_ID_RMS_PARAM ((uint32_t)0xc1)

/* ========================================================================== */
/*                           HEARTBEAT CAN MESSAGE IDs                        */
/* ========================================================================== */
#define FEB_CAN_PCU_HEARTBEAT_FRAME_ID ((uint32_t)0xd0)
#define FEB_CAN_DASH_HEARTBEAT_FRAME_ID ((uint32_t)0xd1)
#define FEB_CAN_LVPDB_HEARTBEAT_FRAME_ID ((uint32_t)0xd2)
#define FEB_CAN_DCU_HEARTBEAT_FRAME_ID ((uint32_t)0xd3)
#define FEB_CAN_FRONT_SENSOR_HEARTBEAT_MESSAGE_FRAME_ID ((uint32_t)0xd4)
#define FEB_CAN_REAR_SENSOR_HEARTBEAT_MESSAGE_FRAME_ID ((uint32_t)0xd5)

/* ========================================================================== */
/*                           PING PONG CAN MESSAGE IDs                        */
/* ========================================================================== */
#define FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID ((uint32_t)0xe0)
#define FEB_CAN_FEB_PING_PONG_COUNTER2_FRAME_ID ((uint32_t)0xe1)
#define FEB_CAN_FEB_PING_PONG_COUNTER3_FRAME_ID ((uint32_t)0xe2)
#define FEB_CAN_FEB_PING_PONG_COUNTER4_FRAME_ID ((uint32_t)0xe3)

/* ========================================================================== */
/*                           APPS CAN MESSAGE IDs                             */
/* ========================================================================== */
#define FEB_CAN_ID_APPS_DATA ((uint32_t)0xff)

#ifdef __cplusplus
}
#endif

#endif /* __FEB_CAN_IDS_H */