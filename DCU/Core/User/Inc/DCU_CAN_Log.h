/**
 ******************************************************************************
 * @file           : DCU_CAN_Log.h
 * @brief          : Raw CAN frame capture and CSV logging to SD card
 * @author         : Formula Electric @ Berkeley
 *
 * Captures every frame received on CAN1 and CAN2 via wildcard callbacks
 * registered with feb_can, queues them, and writes a CSV file per boot to
 * the SD card.
 *
 * Pipeline (all FreeRTOS):
 *
 *   wildcard cb (canDispatchTask)
 *        │ drop-oldest enqueue
 *        ▼
 *   canLogQueue (256 × DCU_CAN_Frame_t)
 *        │
 *        ▼
 *   canLogTask: format CSV → 4 KB batch buffer → DCU_SD_Append
 *
 * The canLogTask is the only entity that touches the SD card via this path;
 * it serializes through the project's `sdTask` like all other SD users.
 ******************************************************************************
 */

#ifndef DCU_CAN_LOG_H
#define DCU_CAN_LOG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

  /**
   * @brief Captured CAN frame, sized to match the canLogQueue item width (24 B).
   *
   * The struct layout is asserted at build time inside DCU_CAN_Log.c so any
   * change here that breaks the queue contract fails loudly. The tag name
   * `struct DCU_CAN_Frame` is referenced by DCU_CAN_Filter.h via forward decl.
   */
  struct DCU_CAN_Frame
  {
    uint32_t ts_ms;    /**< HAL_GetTick() at the moment the frame was queued */
    uint32_t can_id;   /**< 11-bit or 29-bit identifier */
    uint8_t data[8];   /**< Payload; bytes beyond dlc are zeroed */
    uint8_t dlc;       /**< 0..8 */
    uint8_t bus;       /**< 1 = CAN1, 2 = CAN2 */
    uint8_t id_type;   /**< 0 = standard 11-bit, 1 = extended 29-bit */
    uint8_t reserved_; /**< Padding to keep sizeof == 24 */
  };

  typedef struct DCU_CAN_Frame DCU_CAN_Frame_t;

  /**
   * @brief Print `dcu|can|log` status to the console.
   *
   * Emits: active filename, frames written, drops, queue depth.
   */
  void DCU_CAN_Log_PrintStats(void);

  /** @return true once the SD card is open and the CSV header has been written. */
  bool DCU_CAN_Log_IsActive(void);

  /** @return Number of frames dropped from canLogQueue (queue-full events). */
  uint32_t DCU_CAN_Log_GetDropCount(void);

  /** @return Number of CSV rows successfully written to the SD card. */
  uint32_t DCU_CAN_Log_GetWrittenCount(void);

  /** @return Current canLogQueue depth (frames awaiting CSV format). */
  uint32_t DCU_CAN_Log_GetQueueDepth(void);

  /** @return Active CSV filename, or "(none)" if logging has not started. */
  const char *DCU_CAN_Log_GetFilename(void);

#ifdef __cplusplus
}
#endif

#endif /* DCU_CAN_LOG_H */
