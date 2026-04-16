/**
 ******************************************************************************
 * @file           : DCU_SD.c
 * @brief          : DCU SD card smoke test
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_SD.h"
#include "fatfs.h"
#include "ff.h"
#include "main.h"
#include "feb_log.h"
#include <stdio.h>
#include <string.h>

#define TAG_SD "[SD]"
#define DCU_SD_TEST_FILE "dcu_sd_smoke.txt"

static const char *dcu_sd_fresult_string(FRESULT result)
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

void DCU_SD_RunSmokeTest(void)
{
  LOG_I(TAG_SD, "Mounting SD card on %s", USERPath);

  FRESULT result = f_mount(&USERFatFS, USERPath, 1);
  if (result != FR_OK)
  {
    LOG_E(TAG_SD, "Mount failed: %s (%d)", dcu_sd_fresult_string(result), (int)result);
    return;
  }

  FATFS *fs = NULL;
  DWORD free_clusters = 0;
  result = f_getfree(USERPath, &free_clusters, &fs);
  if (result == FR_OK && fs != NULL)
  {
    uint32_t total_sectors = (uint32_t)(fs->n_fatent - 2U) * (uint32_t)fs->csize;
    uint32_t free_sectors = (uint32_t)free_clusters * (uint32_t)fs->csize;
    LOG_I(TAG_SD, "Card ready: total=%lu KB free=%lu KB",
          (unsigned long)(total_sectors / 2U),
          (unsigned long)(free_sectors / 2U));
  }
  else
  {
    LOG_W(TAG_SD, "f_getfree failed: %s (%d)", dcu_sd_fresult_string(result), (int)result);
  }

  FIL file = {0};
  UINT bytes_written = 0;
  char write_buffer[96];
  int write_len = snprintf(write_buffer, sizeof(write_buffer), "DCU SD smoke test tick=%lu\r\n",
                           (unsigned long)HAL_GetTick());
  if (write_len < 0)
  {
    LOG_E(TAG_SD, "Failed to format smoke test payload");
    (void)f_mount(NULL, USERPath, 1);
    return;
  }

  result = f_open(&file, DCU_SD_TEST_FILE, FA_CREATE_ALWAYS | FA_WRITE);
  if (result != FR_OK)
  {
    LOG_E(TAG_SD, "Open for write failed: %s (%d)", dcu_sd_fresult_string(result), (int)result);
    (void)f_mount(NULL, USERPath, 1);
    return;
  }

  result = f_write(&file, write_buffer, (UINT)write_len, &bytes_written);
  if (result != FR_OK || bytes_written != (UINT)write_len)
  {
    LOG_E(TAG_SD, "Write failed: %s (%d), bytes=%u", dcu_sd_fresult_string(result), (int)result,
          (unsigned int)bytes_written);
    (void)f_close(&file);
    (void)f_mount(NULL, USERPath, 1);
    return;
  }

  result = f_sync(&file);
  if (result != FR_OK)
  {
    LOG_E(TAG_SD, "Sync failed: %s (%d)", dcu_sd_fresult_string(result), (int)result);
    (void)f_close(&file);
    (void)f_mount(NULL, USERPath, 1);
    return;
  }

  (void)f_close(&file);

  char read_buffer[96] = {0};
  UINT bytes_read = 0;

  result = f_open(&file, DCU_SD_TEST_FILE, FA_READ);
  if (result != FR_OK)
  {
    LOG_E(TAG_SD, "Open for read failed: %s (%d)", dcu_sd_fresult_string(result), (int)result);
    (void)f_mount(NULL, USERPath, 1);
    return;
  }

  result = f_read(&file, read_buffer, sizeof(read_buffer) - 1U, &bytes_read);
  if (result != FR_OK)
  {
    LOG_E(TAG_SD, "Read failed: %s (%d)", dcu_sd_fresult_string(result), (int)result);
    (void)f_close(&file);
    (void)f_mount(NULL, USERPath, 1);
    return;
  }

  read_buffer[bytes_read] = '\0';
  LOG_I(TAG_SD, "Smoke test passed: wrote %u bytes, read back \"%s\"",
        (unsigned int)bytes_written,
        read_buffer);

  (void)f_close(&file);
  (void)f_mount(NULL, USERPath, 1);
}
