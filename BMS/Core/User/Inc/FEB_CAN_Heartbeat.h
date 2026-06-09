/**
 * @file FEB_CAN_Heartbeat.h
 * @brief CAN-presence (heartbeat) tracking for the BMS
 * @author Formula Electric @ Berkeley
 *
 * Tracks which subsystems are alive on the bus by timestamping their heartbeat
 * frames (0xD0-0xD5, defined in the shared CAN library). Drives the
 * BATTERY_FREE <-> LV_POWER transitions (spec "Only Charger on CAN" /
 * "Other subsystems on CAN"). This is the SN5-idiom equivalent of SN4's
 * FEB_COMBINED_STATUS()/FAck scheme: simple last-seen freshness rather than
 * failed-ack counters.
 */

#ifndef INC_FEB_CAN_HEARTBEAT_H_
#define INC_FEB_CAN_HEARTBEAT_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  FEB_HB_PCU = 0,
  FEB_HB_DASH,
  FEB_HB_LVPDB,
  FEB_HB_DCU,
  FEB_HB_FSN,
  FEB_HB_RSN,
  FEB_HB_COUNT
} FEB_HB_Device_t;

/**
 * @brief Register heartbeat RX. Call from the CAN RX task before
 *        FEB_CAN_Filter_UpdateFromRegistry().
 */
void FEB_CAN_Heartbeat_Init(void);

/** @brief True if @p dev sent a heartbeat within @p timeout_ms. */
bool FEB_CAN_Heartbeat_DevFresh(FEB_HB_Device_t dev, uint32_t timeout_ms);

/**
 * @brief True if "other subsystems" (DASH or PCU) are present on CAN.
 *        Mirrors SN4 FEB_COMBINED_STATUS(), which keys on DASH/PCU.
 */
bool FEB_CAN_Heartbeat_OthersPresent(uint32_t timeout_ms);

#endif /* INC_FEB_CAN_HEARTBEAT_H_ */
