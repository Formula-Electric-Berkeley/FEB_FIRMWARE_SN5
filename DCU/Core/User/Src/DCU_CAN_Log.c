/**
 ******************************************************************************
 * @file           : DCU_CAN_Log.c
 * @brief          : Raw CAN → SD card CSV logger
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_CAN_Log.h"

#include "DCU_CAN_Filter.h"
#include "DCU_SD.h"
#include "cmsis_os.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG_CAN_LOG "[CAN_LOG]"

/* The .ioc declares canLogQueue with item type DCU_CAN_Frame_t, so its width
 * tracks this struct automatically. The assertion below documents the layout
 * we expect (4-byte alignment, no trailing padding on a 32-bit ARM target).
 * If you alter the struct and this fires, regen from CubeMX picks up the new
 * size — but double-check the wildcard producer-consumer assumptions first. */
_Static_assert(sizeof(DCU_CAN_Frame_t) == 20, "DCU_CAN_Frame_t layout changed — review log pipeline");

/* Tunables ----------------------------------------------------------------- */

#define DCU_CAN_LOG_IDX_PATH "0:canlog.idx"
#define DCU_CAN_LOG_FILENAME_TEMPLATE "0:canlog_%04u.csv"
#define DCU_CAN_LOG_FILENAME_MAX 24U

#define DCU_CAN_LOG_HEADER "timestamp_ms,bus,can_id,dlc,d0,d1,d2,d3,d4,d5,d6,d7\r\n"

#define DCU_CAN_LOG_FLUSH_BUF_BYTES 4096U
#define DCU_CAN_LOG_FLUSH_THRESHOLD_BYTES 3072U
#define DCU_CAN_LOG_FLUSH_INTERVAL_MS 1000U
#define DCU_CAN_LOG_LINE_BUF_BYTES 80U
#define DCU_CAN_LOG_SD_MOUNT_TIMEOUT_MS 5000U
#define DCU_CAN_LOG_SD_IO_TIMEOUT_MS 5000U
#define DCU_CAN_LOG_MAX_SESSION_ID 9999U

/* External RTOS handles defined in freertos.c ----------------------------- */
extern osMessageQueueId_t canLogQueueHandle;

/* Module state ------------------------------------------------------------- */

static char s_filename[DCU_CAN_LOG_FILENAME_MAX];
static volatile bool s_active = false;
static volatile uint32_t s_written_count = 0;
static volatile uint32_t s_drop_count = 0;

static uint8_t s_flush_buf[DCU_CAN_LOG_FLUSH_BUF_BYTES];
static size_t s_flush_used = 0;

/* Wildcard RX path -------------------------------------------------------- */

static void enqueue_frame(uint8_t bus, uint32_t can_id, uint8_t id_type, const uint8_t *data, uint8_t length)
{
  DCU_CAN_Frame_t frame;
  frame.ts_ms = HAL_GetTick();
  frame.can_id = can_id;
  frame.dlc = (length > 8U) ? 8U : length;
  frame.bus = bus;
  frame.id_type = id_type;
  frame.reserved_ = 0;

  memcpy(frame.data, data, frame.dlc);
  for (uint8_t i = frame.dlc; i < 8U; i++)
  {
    frame.data[i] = 0;
  }

  /* Non-blocking put — if full, drop the oldest item so the latest frame still
   * lands. Drops are a diagnostic signal, not normal steady-state behaviour. */
  if (osMessageQueuePut(canLogQueueHandle, &frame, 0U, 0U) != osOK)
  {
    DCU_CAN_Frame_t discarded;
    (void)osMessageQueueGet(canLogQueueHandle, &discarded, NULL, 0U);
    (void)osMessageQueuePut(canLogQueueHandle, &frame, 0U, 0U);
    s_drop_count++;
  }
}

static void wildcard_callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                              const uint8_t *data, uint8_t length, void *user_data)
{
  (void)user_data;
  const uint8_t bus = (instance == FEB_CAN_INSTANCE_1) ? 1U : 2U;
  const uint8_t id_type_byte = (id_type == FEB_CAN_ID_EXT) ? 1U : 0U;
  enqueue_frame(bus, can_id, id_type_byte, data, length);
}

static bool register_wildcards(void)
{
  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_WILDCARD,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = wildcard_callback,
      .user_data = NULL,
  };
  if (FEB_CAN_RX_Register(&params) < 0)
  {
    LOG_E(TAG_CAN_LOG, "Failed to register CAN1 wildcard");
    return false;
  }

  params.instance = FEB_CAN_INSTANCE_2;
  if (FEB_CAN_RX_Register(&params) < 0)
  {
    LOG_W(TAG_CAN_LOG, "Failed to register CAN2 wildcard (continuing with CAN1 only)");
  }
  return true;
}

/* CSV row formatter ------------------------------------------------------- */

/* Returns bytes written into `out`, excluding the null terminator. -1 on truncation. */
static int format_row(char *out, size_t out_size, const DCU_CAN_Frame_t *f)
{
  static const char hex[] = "0123456789ABCDEF";

  char data_field[32];
  size_t pos = 0;
  for (uint8_t i = 0; i < 8U; i++)
  {
    if (i > 0)
    {
      data_field[pos++] = ',';
    }
    if (i < f->dlc)
    {
      data_field[pos++] = hex[(f->data[i] >> 4) & 0x0F];
      data_field[pos++] = hex[f->data[i] & 0x0F];
    }
  }
  data_field[pos] = '\0';

  int n = snprintf(out, out_size, "%lu,%u,0x%lX,%u,%s\r\n", (unsigned long)f->ts_ms, (unsigned)f->bus,
                   (unsigned long)f->can_id, (unsigned)f->dlc, data_field);
  if (n < 0 || (size_t)n >= out_size)
  {
    return -1;
  }
  return n;
}

/* SD-side setup (runs inside canLogTask, after scheduler started) --------- */

static uint16_t pick_session_id(void)
{
  char idx_str[16] = {0};
  uint32_t bytes_read = 0;
  uint32_t session = 0;

  FRESULT r = DCU_SD_Read(DCU_CAN_LOG_IDX_PATH, (uint8_t *)idx_str, sizeof(idx_str) - 1U, &bytes_read,
                          DCU_CAN_LOG_SD_IO_TIMEOUT_MS);
  if (r == FR_OK && bytes_read > 0U)
  {
    idx_str[bytes_read] = '\0';
    /* atoi stops at first non-digit, so trailing CR/LF is fine. */
    long parsed = atol(idx_str);
    if (parsed > 0L && parsed <= (long)DCU_CAN_LOG_MAX_SESSION_ID)
    {
      session = (uint32_t)parsed;
    }
  }

  session++;
  if (session > DCU_CAN_LOG_MAX_SESSION_ID)
  {
    session = 1U;
  }

  char out[16];
  int n = snprintf(out, sizeof(out), "%lu\r\n", (unsigned long)session);
  if (n > 0)
  {
    FRESULT w = DCU_SD_Write(DCU_CAN_LOG_IDX_PATH, (const uint8_t *)out, (uint32_t)n, DCU_CAN_LOG_SD_IO_TIMEOUT_MS);
    if (w != FR_OK)
    {
      LOG_W(TAG_CAN_LOG, "Could not persist session index: %s (next boot may reuse this id)", DCU_SD_FresultString(w));
    }
  }

  return (uint16_t)session;
}

static bool prepare_sd_file(void)
{
  FRESULT r = DCU_SD_Mount(DCU_CAN_LOG_SD_MOUNT_TIMEOUT_MS);
  if (r != FR_OK)
  {
    LOG_E(TAG_CAN_LOG, "Mount failed: %s (%d)", DCU_SD_FresultString(r), (int)r);
    return false;
  }

  uint16_t session = pick_session_id();
  int n = snprintf(s_filename, sizeof(s_filename), DCU_CAN_LOG_FILENAME_TEMPLATE, (unsigned)session);
  if (n <= 0 || (size_t)n >= sizeof(s_filename))
  {
    LOG_E(TAG_CAN_LOG, "Filename overflow (session=%u)", (unsigned)session);
    s_filename[0] = '\0';
    return false;
  }

  r = DCU_SD_Write(s_filename, (const uint8_t *)DCU_CAN_LOG_HEADER, (uint32_t)(sizeof(DCU_CAN_LOG_HEADER) - 1U),
                   DCU_CAN_LOG_SD_IO_TIMEOUT_MS);
  if (r != FR_OK)
  {
    LOG_E(TAG_CAN_LOG, "Header write failed: %s (%d)", DCU_SD_FresultString(r), (int)r);
    s_filename[0] = '\0';
    return false;
  }

  LOG_I(TAG_CAN_LOG, "Logging to %s", s_filename);
  return true;
}

/* Logger task ------------------------------------------------------------- */

static void flush_buffer(void)
{
  if (s_flush_used == 0U)
  {
    return;
  }
  FRESULT r = DCU_SD_Append(s_filename, s_flush_buf, (uint32_t)s_flush_used, DCU_CAN_LOG_SD_IO_TIMEOUT_MS);
  if (r != FR_OK)
  {
    LOG_W(TAG_CAN_LOG, "Append failed: %s (%d)", DCU_SD_FresultString(r), (int)r);
  }
  s_flush_used = 0;
}

void StartCanLogTask(void *argument)
{
  (void)argument;
  LOG_I(TAG_CAN_LOG, "canLogTask starting");

  if (!prepare_sd_file())
  {
    LOG_E(TAG_CAN_LOG, "SD prep failed — logger idle (CAN frames not captured)");
    for (;;)
    {
      osDelay(pdMS_TO_TICKS(1000));
    }
  }

  if (!register_wildcards())
  {
    LOG_E(TAG_CAN_LOG, "Wildcard registration failed — logger idle");
    for (;;)
    {
      osDelay(pdMS_TO_TICKS(1000));
    }
  }

  s_active = true;
  uint32_t last_flush_ms = HAL_GetTick();
  char line_buf[DCU_CAN_LOG_LINE_BUF_BYTES];

  for (;;)
  {
    DCU_CAN_Frame_t frame;
    const uint32_t now = HAL_GetTick();
    const uint32_t elapsed = now - last_flush_ms;
    const uint32_t wait_ms =
        (elapsed >= DCU_CAN_LOG_FLUSH_INTERVAL_MS) ? 0U : (DCU_CAN_LOG_FLUSH_INTERVAL_MS - elapsed);

    if (osMessageQueueGet(canLogQueueHandle, &frame, NULL, pdMS_TO_TICKS(wait_ms)) == osOK)
    {
      int len = format_row(line_buf, sizeof(line_buf), &frame);
      if (len > 0 && (s_flush_used + (size_t)len) <= sizeof(s_flush_buf))
      {
        memcpy(&s_flush_buf[s_flush_used], line_buf, (size_t)len);
        s_flush_used += (size_t)len;
        s_written_count++;
      }

      if (DCU_CAN_Filter_ShouldForwardToRadio(&frame))
      {
        /* Radio forwarding is wired up in DCU_CAN_Filter.c — no-op stub today. */
      }
    }

    if (s_flush_used >= DCU_CAN_LOG_FLUSH_THRESHOLD_BYTES ||
        (s_flush_used > 0U && (HAL_GetTick() - last_flush_ms) >= DCU_CAN_LOG_FLUSH_INTERVAL_MS))
    {
      flush_buffer();
      last_flush_ms = HAL_GetTick();
    }
  }
}

/* Public stats accessors -------------------------------------------------- */

bool DCU_CAN_Log_IsActive(void)
{
  return s_active;
}
uint32_t DCU_CAN_Log_GetDropCount(void)
{
  return s_drop_count;
}
uint32_t DCU_CAN_Log_GetWrittenCount(void)
{
  return s_written_count;
}

uint32_t DCU_CAN_Log_GetQueueDepth(void)
{
  return (uint32_t)osMessageQueueGetCount(canLogQueueHandle);
}

const char *DCU_CAN_Log_GetFilename(void)
{
  return (s_filename[0] != '\0') ? s_filename : "(none)";
}

void DCU_CAN_Log_PrintStats(void)
{
  LOG_I(TAG_CAN_LOG, "active=%d file=%s written=%lu drops=%lu qdepth=%lu", (int)s_active, DCU_CAN_Log_GetFilename(),
        (unsigned long)s_written_count, (unsigned long)s_drop_count, (unsigned long)DCU_CAN_Log_GetQueueDepth());
}
