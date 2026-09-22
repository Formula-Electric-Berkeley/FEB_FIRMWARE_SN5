/**
 ******************************************************************************
 * @file           : FEB_CAN_Heartbeat.h
 * @brief          : Node liveness + fault heartbeat (0xD4 FRONT / 0xD5 REAR).
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Every other node on the bus publishes a heartbeat (PCU 0xD0, DASH 0xD1,
 * LVPDB 0xD2, DCU 0xD3) and the BMS already registers an RX filter covering
 * 0xD0-0xD7, mapping 0xD4/0xD5 to FEB_HB_FSN / FEB_HB_RSN. Until this module
 * existed those two slots never went fresh, so the BMS saw both sensor nodes as
 * permanently timed out.
 *
 * The frame carries 64 single-bit error flags; the allocated ones are named in
 * common/FEB_CAN_Library_SN4/msg_defs/sensor_nodes_messages.py and grouped by
 * byte (device faults, data freshness, CAN health). FRONT and REAR share one
 * layout, so a single decoder handles either node.
 *
 * Latched init results come in through FEB_CAN_Heartbeat_SetFault(); everything
 * else is sampled live in the Tick from the drivers and the CAN library.
 *
 ******************************************************************************
 */

#ifndef INC_FEB_CAN_HEARTBEAT_H_
#define INC_FEB_CAN_HEARTBEAT_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Faults that are latched at init rather than sampled every tick.
 *
 * These reflect one-shot outcomes (a driver that failed to come up) which
 * cannot be re-derived later, so FEB_Init() records them as it goes.
 */
typedef enum
{
  FEB_SN_FAULT_IMU_INIT = 0,
  FEB_SN_FAULT_MAG_INIT,
  FEB_SN_FAULT_GPS_INIT,
  FEB_SN_FAULT_COUNT
} FEB_SN_Fault_t;

/** @brief Clear all latched faults. Call once before the drivers come up. */
void FEB_CAN_Heartbeat_Init(void);

/** @brief Latch (or clear) an init-time fault. */
void FEB_CAN_Heartbeat_SetFault(FEB_SN_Fault_t fault, bool asserted);

/** @brief Read back a latched fault (for the console). */
bool FEB_CAN_Heartbeat_GetFault(FEB_SN_Fault_t fault);

/**
 * @brief Sample the live fault sources and transmit the heartbeat.
 *
 * Call on a fixed cadence (10 Hz) — consumers key on arrival time, so this must
 * not be gated on anything that could go quiet.
 */
void FEB_CAN_Heartbeat_Tick(void);

#endif /* INC_FEB_CAN_HEARTBEAT_H_ */
