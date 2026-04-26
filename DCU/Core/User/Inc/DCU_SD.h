/**
 ******************************************************************************
 * @file           : DCU_SD.h
 * @brief          : DCU SD card management — buffered task + request API
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * The SD card is owned by a single FreeRTOS task (`sdTask`). Other tasks call
 * the public API below; each call posts a request onto `sdRequestQueue` and
 * blocks on a per-call semaphore until the SD task drains it. This guarantees
 * single-threaded access to the FATFS layer and the underlying SPI peripheral.
 ******************************************************************************
 */

#ifndef DCU_SD_H
#define DCU_SD_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"
#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* ============================================================================
   * Request queue payload
   * ==========================================================================*/

  typedef enum
  {
    DCU_SD_OP_MOUNT = 0,
    DCU_SD_OP_UNMOUNT,
    DCU_SD_OP_GET_INFO,
    DCU_SD_OP_LS,
    DCU_SD_OP_READ,
    DCU_SD_OP_WRITE,
    DCU_SD_OP_APPEND,
    DCU_SD_OP_DELETE,
    DCU_SD_OP_SMOKE_TEST,
    DCU_SD_OP_BENCH,
  } DCU_SD_OpType_t;

  typedef struct
  {
    DCU_SD_OpType_t op;

    /* Path / filename for ops that need it. Caller-owned, must outlive request. */
    const char *path;

    /* Data buffer. For READ this is destination; for WRITE/APPEND it is source. */
    uint8_t *buffer;
    uint32_t buffer_len;

    /* For READ: receives the actual byte count. For GET_INFO: total/free KB. */
    uint32_t *out_a;
    uint32_t *out_b;
    uint8_t *out_fs_type;

    /* Result is written here when the SD task completes the operation. */
    FRESULT result;

    /* The thread that posted the request — sdTask wakes it via thread flags
     * when the operation completes. No allocation per call. */
    osThreadId_t caller;
  } DCU_SD_Request_t;

  /* Thread flag bit used by sdTask to signal completion to the caller. */
#define DCU_SD_FLAG_DONE (1U << 24)

  /* ============================================================================
   * Public API — every call routes through sdTask. Returns FR_TIMEOUT on
   * queue post failure or when the SD task takes longer than `timeout_ms`.
   * ==========================================================================*/

  FRESULT DCU_SD_Mount(uint32_t timeout_ms);
  FRESULT DCU_SD_Unmount(uint32_t timeout_ms);

  /** Capacity report. Either pointer may be NULL. fs_type uses FATFS FS_FAT32 etc. */
  FRESULT DCU_SD_GetInfo(uint32_t *total_kb, uint32_t *free_kb, uint8_t *fs_type, uint32_t timeout_ms);

  /** List a directory. The SD task prints entries via FEB_Console_Printf. */
  FRESULT DCU_SD_Ls(const char *path, uint32_t timeout_ms);

  /** Read up to max_len bytes; *out_bytes receives the actual count. */
  FRESULT DCU_SD_Read(const char *path, uint8_t *buffer, uint32_t max_len, uint32_t *out_bytes, uint32_t timeout_ms);

  /** Truncating write. */
  FRESULT DCU_SD_Write(const char *path, const uint8_t *data, uint32_t len, uint32_t timeout_ms);

  /** Append (creates the file if missing). */
  FRESULT DCU_SD_Append(const char *path, const uint8_t *data, uint32_t len, uint32_t timeout_ms);

  FRESULT DCU_SD_Delete(const char *path, uint32_t timeout_ms);

  /** Self-contained smoke test (mount + write + read + unmount). */
  void DCU_SD_RunSmokeTest(void);

  /** 64 KB write+read benchmark; prints throughput via FEB_Console_Printf. */
  void DCU_SD_RunBenchmark(void);

  /** Convert a FRESULT into a printable string. */
  const char *DCU_SD_FresultString(FRESULT result);

  /* ============================================================================
   * Task entry point — defined here as `As weak` in the .ioc; implementation
   * lives in DCU_SD.c.
   * ==========================================================================*/

  void StartSdTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* DCU_SD_H */
