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

#define FEB_CAN_ID_BMS_STATE                    0x100  /* BMS state machine status */
#define FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE      0x101  /* Pack voltage */
#define FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE  0x102  /* Pack temperature */

/* ========================================================================== */
/*                           RMS (Motor Controller) IDs                       */
/* ========================================================================== */

#define FEB_CAN_ID_RMS_TORQUE                   0x0C0  /* Torque command (to RMS) */
#define FEB_CAN_ID_RMS_PARAM                    0x0C1  /* Parameter command (to RMS) */
#define FEB_CAN_ID_RMS_VOLTAGE                  0x0A0  /* Voltage info (from RMS) */
#define FEB_CAN_ID_RMS_MOTOR                    0x0A5  /* Motor speed/position (from RMS) */

/* ========================================================================== */
/*                           PCU CAN MESSAGE IDs                              */
/* ========================================================================== */

#define FEB_CAN_ID_PCU_HEARTBEAT                0x200  /* PCU heartbeat/status */
#define FEB_CAN_ID_BSPD_STATUS                  0x201  /* BSPD status from PCU */
#define FEB_CAN_ID_BRAKE_DATA                   0x202  /* Brake sensor data from PCU */
#define FEB_CAN_ID_TPS_DATA                     0x203  /* TPS2482 voltage/current data */

#ifdef __cplusplus
}
#endif

#endif /* __FEB_CAN_IDS_H */
