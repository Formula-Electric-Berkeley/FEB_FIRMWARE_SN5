/**
 ******************************************************************************
 * @file           : FEB_CAN_IRTSSensorConfig.h
 * @brief          : One-shot CAN burst to program an Infrared Tire Temperature
 *                   Sensor (IRTS). On request, transmits a fixed 8-byte config
 *                   frame at 1 Hz for 15 s, then stops automatically.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Console: SN|IRTS|send  (or IRTS|send) starts the burst; SN|IRTS|stop aborts
 * it early; SN|IRTS|status reports remaining time. The CAN ID and the 8 payload
 * bytes are edited in FEB_CAN_IRTSSensorConfig.c (see the EDIT HERE block).
 ******************************************************************************
 */

#ifndef INC_FEB_CAN_IRTSSENSORCONFIG_H_
#define INC_FEB_CAN_IRTSSENSORCONFIG_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the IRTS sensor-config module. Call once at startup, after
 *        FEB_CAN_Init(). The burst starts idle.
 */
void FEB_CAN_IRTSSensorConfig_Init(void);

/**
 * @brief Begin the config burst: send the frame immediately, then once per
 *        second until 15 s have elapsed. Calling again while active restarts
 *        the 15 s window from now.
 */
void FEB_CAN_IRTSSensorConfig_Start(void);

/**
 * @brief Abort the burst early (no-op if already idle).
 */
void FEB_CAN_IRTSSensorConfig_Stop(void);

/**
 * @brief True while the burst is running.
 */
bool FEB_CAN_IRTSSensorConfig_IsActive(void);

/**
 * @brief Milliseconds remaining in the current burst (0 when idle).
 */
uint32_t FEB_CAN_IRTSSensorConfig_RemainingMs(void);

/**
 * @brief Number of frames sent so far in the current/last burst.
 */
uint32_t FEB_CAN_IRTSSensorConfig_SentCount(void);

/**
 * @brief Service the burst. Call every main-loop iteration; it self-gates on
 *        the 1 Hz cadence and stops itself after the window closes.
 */
void FEB_CAN_IRTSSensorConfig_Tick(void);

#endif /* INC_FEB_CAN_IRTSSENSORCONFIG_H_ */
