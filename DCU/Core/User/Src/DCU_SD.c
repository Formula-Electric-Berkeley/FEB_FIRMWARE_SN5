/**
 ******************************************************************************
 * @file           : DCU_SD.c
 * @brief          : DCU SD card management — buffered task + request API
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_SD.h"
#include "fatfs.h"
#include "ff.h"
#include "main.h"
#include "feb_log.h"
#include "feb_console.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

#define TAG_SD "[SD]"
#define DCU_SD_TEST_FILE "dcu_sd_smoke.txt"
#define DCU_SD_BENCH_FILE "bench.bin"
#define DCU_SD_BENCH_BYTES (64UL * 1024UL)

extern osMessageQueueId_t sdRequestQueueHandle;

static bool s_mounted = false;

const char *DCU_SD_FresultString(FRESULT result)
{
  switch (result)
  {
  case FR_OK:
    return "FR_OK";
  case FR_DISK_ERR:
    return "FR_DISK_ERR";
  case FR_INT_ERR:
    return "FR_INT_ERR";
  case FR_NOT_READY:
    return "FR_NOT_READY";
  case FR_NO_FILE:
    return "FR_NO_FILE";
  case FR_NO_PATH:
    return "FR_NO_PATH";
  case FR_INVALID_NAME:
    return "FR_INVALID_NAME";
  case FR_DENIED:
    return "FR_DENIED";
  case FR_EXIST:
    return "FR_EXIST";
  case FR_INVALID_OBJECT:
    return "FR_INVALID_OBJECT";
  case FR_WRITE_PROTECTED:
    return "FR_WRITE_PROTECTED";
  case FR_INVALID_DRIVE:
    return "FR_INVALID_DRIVE";
  case FR_NOT_ENABLED:
    return "FR_NOT_ENABLED";
  case FR_NO_FILESYSTEM:
    return "FR_NO_FILESYSTEM";
  case FR_MKFS_ABORTED:
    return "FR_MKFS_ABORTED";
  case FR_TIMEOUT:
    return "FR_TIMEOUT";
  case FR_LOCKED:
    return "FR_LOCKED";
  case FR_NOT_ENOUGH_CORE:
    return "FR_NOT_ENOUGH_CORE";
  case FR_TOO_MANY_OPEN_FILES:
    return "FR_TOO_MANY_OPEN_FILES";
  case FR_INVALID_PARAMETER:
    return "FR_INVALID_PARAMETER";
  default:
    return "FR_UNKNOWN";
  }
}

/* ============================================================================
 * SD task — owns FATFS access. Pulls requests from sdRequestQueue and
 * dispatches to the per-op handler.
 * ==========================================================================*/

static FRESULT sd_op_mount(void)
{
  if (s_mounted)
    return FR_OK;
  FRESULT r = f_mount(&USERFatFS, USERPath, 1);
  if (r == FR_OK)
    s_mounted = true;
  return r;
}

static FRESULT sd_op_unmount(void)
{
  FRESULT r = f_mount(NULL, USERPath, 1);
  s_mounted = false;
  return r;
}

static FRESULT sd_op_get_info(uint32_t *total_kb, uint32_t *free_kb, uint8_t *fs_type)
{
  if (!s_mounted)
  {
    FRESULT r = sd_op_mount();
    if (r != FR_OK)
      return r;
  }
  FATFS *fs = NULL;
  DWORD free_clusters = 0;
  FRESULT r = f_getfree(USERPath, &free_clusters, &fs);
  if (r != FR_OK || fs == NULL)
    return r;
  uint32_t total_sectors = (uint32_t)(fs->n_fatent - 2U) * (uint32_t)fs->csize;
  uint32_t free_sectors = (uint32_t)free_clusters * (uint32_t)fs->csize;
  if (total_kb)
    *total_kb = total_sectors / 2U;
  if (free_kb)
    *free_kb = free_sectors / 2U;
  if (fs_type)
    *fs_type = (uint8_t)fs->fs_type;
  return FR_OK;
}

static FRESULT sd_op_ls(const char *path)
{
  if (!s_mounted)
  {
    FRESULT r = sd_op_mount();
    if (r != FR_OK)
      return r;
  }
  DIR dir;
  FRESULT r = f_opendir(&dir, path ? path : "/");
  if (r != FR_OK)
    return r;
  FILINFO fno;
  uint32_t count = 0;
  for (;;)
  {
    r = f_readdir(&dir, &fno);
    if (r != FR_OK || fno.fname[0] == '\0')
      break;
    bool is_dir = (fno.fattrib & AM_DIR) != 0;
    FEB_Console_Printf("  %s%-32s %lu bytes\r\n", is_dir ? "[D] " : "    ", fno.fname, (unsigned long)fno.fsize);
    count++;
  }
  f_closedir(&dir);
  FEB_Console_Printf("Total: %lu entries\r\n", (unsigned long)count);
  return FR_OK;
}

static FRESULT sd_op_read(const char *path, uint8_t *buf, uint32_t max_len, uint32_t *out_bytes)
{
  if (!s_mounted)
  {
    FRESULT r = sd_op_mount();
    if (r != FR_OK)
      return r;
  }
  if (out_bytes)
    *out_bytes = 0;
  FIL fp;
  FRESULT r = f_open(&fp, path, FA_READ);
  if (r != FR_OK)
    return r;
  UINT n = 0;
  r = f_read(&fp, buf, (UINT)max_len, &n);
  f_close(&fp);
  if (out_bytes)
    *out_bytes = (uint32_t)n;
  return r;
}

static FRESULT sd_op_write(const char *path, const uint8_t *buf, uint32_t len, bool append)
{
  if (!s_mounted)
  {
    FRESULT r = sd_op_mount();
    if (r != FR_OK)
      return r;
  }
  FIL fp;
  BYTE mode = append ? (FA_OPEN_APPEND | FA_WRITE) : (FA_CREATE_ALWAYS | FA_WRITE);
  FRESULT r = f_open(&fp, path, mode);
  if (r != FR_OK)
    return r;
  UINT n = 0;
  r = f_write(&fp, buf, (UINT)len, &n);
  if (r == FR_OK && n != len)
    r = FR_DISK_ERR;
  f_close(&fp);
  return r;
}

static FRESULT sd_op_delete(const char *path)
{
  if (!s_mounted)
  {
    FRESULT r = sd_op_mount();
    if (r != FR_OK)
      return r;
  }
  return f_unlink(path);
}

/* Self-contained smoke test — runs entirely inside sdTask context. */
static FRESULT sd_op_smoke(void)
{
  LOG_I(TAG_SD, "Mounting SD card on %s", USERPath);
  FRESULT r = sd_op_mount();
  if (r != FR_OK)
  {
    LOG_E(TAG_SD, "Mount failed: %s (%d)", DCU_SD_FresultString(r), (int)r);
    return r;
  }

  uint32_t total_kb = 0, free_kb = 0;
  uint8_t fs_type = 0;
  r = sd_op_get_info(&total_kb, &free_kb, &fs_type);
  if (r == FR_OK)
    LOG_I(TAG_SD, "Card ready: total=%lu KB free=%lu KB", (unsigned long)total_kb, (unsigned long)free_kb);

  char payload[96];
  int n = snprintf(payload, sizeof(payload), "DCU SD smoke test tick=%lu\r\n", (unsigned long)HAL_GetTick());
  if (n < 0)
  {
    (void)sd_op_unmount();
    return FR_INT_ERR;
  }
  r = sd_op_write(DCU_SD_TEST_FILE, (const uint8_t *)payload, (uint32_t)n, false);
  if (r != FR_OK)
  {
    LOG_E(TAG_SD, "Write failed: %s (%d)", DCU_SD_FresultString(r), (int)r);
    (void)sd_op_unmount();
    return r;
  }

  uint8_t readbuf[96];
  uint32_t bytes_read = 0;
  r = sd_op_read(DCU_SD_TEST_FILE, readbuf, sizeof(readbuf) - 1U, &bytes_read);
  if (r != FR_OK)
  {
    LOG_E(TAG_SD, "Read failed: %s (%d)", DCU_SD_FresultString(r), (int)r);
    (void)sd_op_unmount();
    return r;
  }
  readbuf[bytes_read] = '\0';
  LOG_I(TAG_SD, "Smoke test passed: wrote %d bytes, read back \"%s\"", n, (char *)readbuf);

  return sd_op_unmount();
}

static FRESULT sd_op_bench(void)
{
  if (!s_mounted)
  {
    FRESULT r = sd_op_mount();
    if (r != FR_OK)
      return r;
  }

  uint8_t chunk[512];
  for (size_t i = 0; i < sizeof(chunk); i++)
    chunk[i] = (uint8_t)(i & 0xFF);

  FIL fp;
  FRESULT r = f_open(&fp, DCU_SD_BENCH_FILE, FA_CREATE_ALWAYS | FA_WRITE);
  if (r != FR_OK)
    return r;

  uint32_t t0 = HAL_GetTick();
  uint32_t written_total = 0;
  while (written_total < DCU_SD_BENCH_BYTES)
  {
    UINT got = 0;
    r = f_write(&fp, chunk, sizeof(chunk), &got);
    if (r != FR_OK || got == 0)
      break;
    written_total += got;
  }
  f_close(&fp);
  uint32_t t1 = HAL_GetTick();
  if (r != FR_OK)
    return r;

  r = f_open(&fp, DCU_SD_BENCH_FILE, FA_READ);
  if (r != FR_OK)
    return r;
  uint32_t t2 = HAL_GetTick();
  uint32_t read_total = 0;
  for (;;)
  {
    UINT got = 0;
    r = f_read(&fp, chunk, sizeof(chunk), &got);
    if (r != FR_OK || got == 0)
      break;
    read_total += got;
  }
  f_close(&fp);
  uint32_t t3 = HAL_GetTick();

  uint32_t w_ms = (t1 > t0) ? (t1 - t0) : 1U;
  uint32_t r_ms = (t3 > t2) ? (t3 - t2) : 1U;
  FEB_Console_Printf("Wrote %lu bytes in %lu ms (%lu KB/s)\r\n", (unsigned long)written_total, (unsigned long)w_ms,
                     (unsigned long)(written_total / w_ms));
  FEB_Console_Printf("Read  %lu bytes in %lu ms (%lu KB/s)\r\n", (unsigned long)read_total, (unsigned long)r_ms,
                     (unsigned long)(read_total / r_ms));
  return FR_OK;
}

void StartSdTask(void *argument)
{
  (void)argument;
  LOG_I(TAG_SD, "sdTask started, queue ready");

  /* Idle SD_CS high — CubeMX-generated gpio.c pulls it low at boot. */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  for (;;)
  {
    DCU_SD_Request_t *req;
    osStatus_t s = osMessageQueueGet(sdRequestQueueHandle, &req, NULL, osWaitForever);
    if (s != osOK)
    {
      osDelay(pdMS_TO_TICKS(10));
      continue;
    }

    FRESULT r = FR_INVALID_PARAMETER;
    switch (req->op)
    {
    case DCU_SD_OP_MOUNT:
      r = sd_op_mount();
      break;
    case DCU_SD_OP_UNMOUNT:
      r = sd_op_unmount();
      break;
    case DCU_SD_OP_GET_INFO:
      r = sd_op_get_info(req->out_a, req->out_b, req->out_fs_type);
      break;
    case DCU_SD_OP_LS:
      r = sd_op_ls(req->path);
      break;
    case DCU_SD_OP_READ:
      r = sd_op_read(req->path, req->buffer, req->buffer_len, req->out_a);
      break;
    case DCU_SD_OP_WRITE:
      r = sd_op_write(req->path, req->buffer, req->buffer_len, false);
      break;
    case DCU_SD_OP_APPEND:
      r = sd_op_write(req->path, req->buffer, req->buffer_len, true);
      break;
    case DCU_SD_OP_DELETE:
      r = sd_op_delete(req->path);
      break;
    case DCU_SD_OP_SMOKE_TEST:
      r = sd_op_smoke();
      break;
    case DCU_SD_OP_BENCH:
      r = sd_op_bench();
      break;
    default:
      break;
    }

    req->result = r;
    if (req->caller)
      osThreadFlagsSet(req->caller, DCU_SD_FLAG_DONE);
  }
}

/* ============================================================================
 * Public API — synchronously post a request and wait for completion via
 * caller-thread flags.
 * ==========================================================================*/

static FRESULT submit_and_wait(DCU_SD_Request_t *req, uint32_t timeout_ms)
{
  req->result = FR_NOT_READY;
  req->caller = osThreadGetId();

  /* Clear any stale DONE flag bit before posting so we wait on a fresh signal. */
  (void)osThreadFlagsClear(DCU_SD_FLAG_DONE);

  if (osMessageQueuePut(sdRequestQueueHandle, &req, 0U, timeout_ms) != osOK)
  {
    return FR_TIMEOUT;
  }

  uint32_t flags = osThreadFlagsWait(DCU_SD_FLAG_DONE, osFlagsWaitAny, timeout_ms);
  if ((flags & osFlagsError) != 0U)
  {
    return FR_TIMEOUT;
  }
  return req->result;
}

FRESULT DCU_SD_Mount(uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {.op = DCU_SD_OP_MOUNT};
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_Unmount(uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {.op = DCU_SD_OP_UNMOUNT};
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_GetInfo(uint32_t *total_kb, uint32_t *free_kb, uint8_t *fs_type, uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {
      .op = DCU_SD_OP_GET_INFO,
      .out_a = total_kb,
      .out_b = free_kb,
      .out_fs_type = fs_type,
  };
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_Ls(const char *path, uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {.op = DCU_SD_OP_LS, .path = path};
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_Read(const char *path, uint8_t *buffer, uint32_t max_len, uint32_t *out_bytes, uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {
      .op = DCU_SD_OP_READ,
      .path = path,
      .buffer = buffer,
      .buffer_len = max_len,
      .out_a = out_bytes,
  };
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_Write(const char *path, const uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {
      .op = DCU_SD_OP_WRITE,
      .path = path,
      .buffer = (uint8_t *)data,
      .buffer_len = len,
  };
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_Append(const char *path, const uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {
      .op = DCU_SD_OP_APPEND,
      .path = path,
      .buffer = (uint8_t *)data,
      .buffer_len = len,
  };
  return submit_and_wait(&req, timeout_ms);
}

FRESULT DCU_SD_Delete(const char *path, uint32_t timeout_ms)
{
  DCU_SD_Request_t req = {.op = DCU_SD_OP_DELETE, .path = path};
  return submit_and_wait(&req, timeout_ms);
}

void DCU_SD_RunSmokeTest(void)
{
  DCU_SD_Request_t req = {.op = DCU_SD_OP_SMOKE_TEST};
  (void)submit_and_wait(&req, 30000U);
}

void DCU_SD_RunBenchmark(void)
{
  DCU_SD_Request_t req = {.op = DCU_SD_OP_BENCH};
  (void)submit_and_wait(&req, 60000U);
}
