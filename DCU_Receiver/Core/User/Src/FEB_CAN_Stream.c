/**
 * @file    FEB_CAN_Stream.c
 * @brief   Live CAN-frame console stream for DCU_Receiver
 * @author  Formula Electric @ Berkeley
 *
 * Kept deliberately in lock-step with the DCU's streaming path in
 * DCU_CAN_Log.c: same `can` row formatter, same SetStream transaction model
 * (two `done`s to drain the off-band stream), same CSV command names. That is
 * what lets a host app treat the DCU and the DCU_Receiver interchangeably.
 */

#include "FEB_CAN_Stream.h"

#include "feb_console.h"

#include <stdio.h>
#include <string.h>

/* Stream state — written from the console (command) task, read from the radio
 * task that calls EmitFrame. Both are FreeRTOS tasks; the bool/flag accesses are
 * single-word and the tx_id is only swapped while toggling, matching the DCU's
 * (equally lightweight) approach. */
static volatile bool s_stream_active = false;
static char s_stream_tx_id[FEB_CSV_TX_ID_MAX_LEN + 1] = {0};

/* Format the CAN-frame body as bus,can_id,dlc,d0..d7 — identical to the DCU's
 * format_row(). No leading timestamp, no CRLF. Returns bytes written or -1. */
static int format_row(char *out, size_t out_size, uint8_t bus, uint32_t can_id, uint8_t dlc, const uint8_t *data)
{
  static const char hex[] = "0123456789ABCDEF";

  if (dlc > 8U)
  {
    dlc = 8U;
  }

  char data_field[32];
  size_t pos = 0;
  for (uint8_t i = 0; i < 8U; i++)
  {
    if (i > 0)
    {
      data_field[pos++] = ',';
    }
    if (i < dlc)
    {
      data_field[pos++] = hex[(data[i] >> 4) & 0x0F];
      data_field[pos++] = hex[data[i] & 0x0F];
    }
  }
  data_field[pos] = '\0';

  int n = snprintf(out, out_size, "%u,0x%lX,%u,%s", (unsigned)bus, (unsigned long)can_id, (unsigned)dlc, data_field);
  if (n < 0 || (size_t)n >= out_size)
  {
    return -1;
  }
  return n;
}

void FEB_CAN_Stream_EmitFrame(uint8_t bus, uint32_t can_id, uint8_t dlc, const uint8_t *data)
{
  if (!s_stream_active || s_stream_tx_id[0] == '\0')
  {
    return;
  }

  char row[64];
  if (format_row(row, sizeof(row), bus, can_id, dlc, data) > 0)
  {
    (void)FEB_Console_CsvEmitAs(s_stream_tx_id, "can", "%s", row);
  }
}

void FEB_CAN_Stream_SetStream(bool on, const char *tx_id)
{
  if (on)
  {
    /* Close any prior session under a different tx_id with a `done` first. */
    if (s_stream_active && s_stream_tx_id[0] != '\0' &&
        (tx_id == NULL || strncmp(s_stream_tx_id, tx_id, sizeof(s_stream_tx_id)) != 0))
    {
      (void)FEB_Console_CsvEmitAs(s_stream_tx_id, "done", NULL);
    }
    if (tx_id != NULL)
    {
      strncpy(s_stream_tx_id, tx_id, sizeof(s_stream_tx_id) - 1U);
      s_stream_tx_id[sizeof(s_stream_tx_id) - 1U] = '\0';
    }
    else
    {
      s_stream_tx_id[0] = '\0';
    }
    s_stream_active = true;
  }
  else
  {
    if (s_stream_active && s_stream_tx_id[0] != '\0')
    {
      (void)FEB_Console_CsvEmitAs(s_stream_tx_id, "done", NULL);
    }
    s_stream_active = false;
    s_stream_tx_id[0] = '\0';
  }
}

bool FEB_CAN_Stream_IsStreaming(void)
{
  return s_stream_active;
}

const char *FEB_CAN_Stream_GetTxId(void)
{
  return s_stream_tx_id;
}

/* ============================================================================
 * CSV-protocol handlers (can-stream-on/off/status) — same names/semantics as
 * the DCU's, registered top-level so `<board>|csv|<tx>|can-stream-on` works.
 * ============================================================================ */

static void csv_handle_stream_on(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  char tx_id[FEB_CSV_TX_ID_MAX_LEN + 1];
  if (!FEB_Console_CsvCurrentTxId(tx_id, sizeof(tx_id)))
  {
    (void)FEB_Console_CsvError("error", "no active tx_id");
    return;
  }
  FEB_CAN_Stream_SetStream(true, tx_id);
}

static void csv_handle_stream_off(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_CAN_Stream_SetStream(false, NULL);
}

static void csv_handle_stream_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  if (s_stream_active)
  {
    (void)FEB_Console_CsvEmit("status", "on,%s", s_stream_tx_id);
  }
  else
  {
    (void)FEB_Console_CsvEmit("status", "off");
  }
}

static const FEB_Console_Cmd_t s_csv_stream_on = {
    .name = "can-stream-on",
    .help = "Begin streaming reconstructed CAN frames as `can,...` rows",
    .handler = NULL,
    .csv_handler = csv_handle_stream_on,
    .hidden = true,
};

static const FEB_Console_Cmd_t s_csv_stream_off = {
    .name = "can-stream-off",
    .help = "Stop the active CAN-frame stream",
    .handler = NULL,
    .csv_handler = csv_handle_stream_off,
    .hidden = true,
};

static const FEB_Console_Cmd_t s_csv_stream_status = {
    .name = "can-stream-status",
    .help = "Report whether CAN-frame streaming is active",
    .handler = NULL,
    .csv_handler = csv_handle_stream_status,
    .hidden = true,
};

void FEB_CAN_Stream_RegisterCsvHandlers(void)
{
  (void)FEB_Console_Register(&s_csv_stream_on);
  (void)FEB_Console_Register(&s_csv_stream_off);
  (void)FEB_Console_Register(&s_csv_stream_status);
}
