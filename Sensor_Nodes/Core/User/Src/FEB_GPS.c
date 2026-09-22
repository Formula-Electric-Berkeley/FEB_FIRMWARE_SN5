/**
 ******************************************************************************
 * @file           : FEB_GPS.c
 * @brief          : GPS driver implementation for MTK3339 using FEB_UART and LwGPS
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_GPS.h"
#include "feb_uart.h"
#include "feb_log.h"
#include "main.h"
#include "lwgps/lwgps.h"
#include <string.h>
#include <stdio.h>

#define TAG_GPS "[GPS]"

/* External HAL handles */
extern UART_HandleTypeDef huart4;
extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_uart4_tx;

/* Buffer sizes */
#define GPS_TX_BUFFER_SIZE 256
#define GPS_RX_BUFFER_SIZE 1024

/* Baud negotiation.
 *
 * UART4 is configured for GPS_BAUD_FAST in Sensor_Nodes.ioc, because 9600 baud
 * (960 B/s) cannot carry GGA+GSA+RMC (~210 B) at anything above ~4 Hz. The
 * MTK3339 keeps a PMTK251-set rate across an MCU-only reset but reverts to
 * GPS_BAUD_DEFAULT whenever GPS_EN power-cycles it, so neither rate can be
 * assumed at boot — probe for traffic and command the module up if needed. */
#define GPS_BAUD_DEFAULT 9600U
#define GPS_BAUD_FAST 115200U
#define GPS_BOOT_SETTLE_MS 300U /* module needs to come up after GPS_EN rises */
#define GPS_PROBE_WINDOW_MS 600U

/* Static buffers */
static uint8_t gps_tx_buffer[GPS_TX_BUFFER_SIZE];
static uint8_t gps_rx_buffer[GPS_RX_BUFFER_SIZE];

/* LwGPS handle */
static lwgps_t gps_handle;

/* Latest GPS data */
static FEB_GPS_Data_t gps_data;
static bool gps_data_updated = false;

/* Module state */
static bool gps_initialized = false;
static bool gps_had_fix = false; /* Track previous fix state for change detection */
static uint32_t gps_baud = GPS_BAUD_FAST;
static volatile bool gps_sentence_seen = false; /* set by the line callback, read by the probe */

/* Forward declaration */
static void gps_rx_line_callback(const char *line, size_t len);

/**
 * @brief Match an NMEA sentence type, ignoring the talker ID.
 *
 * Sentences are "$ttXXX,..." — two talker chars (GP, GN, GL, ... depending on
 * which constellations the module is using) then a three-char type. Matching on
 * the type alone keeps this working if the talker changes.
 */
static inline bool nmea_is_sentence(const char *line, size_t len, const char *type)
{
  return (len >= 6u) && (memcmp(&line[3], type, 3u) == 0);
}

/**
 * @brief Block until a well-formed NMEA sentence arrives, or the window closes.
 *
 * Used only during init to decide which rate the module is currently talking at
 * — a wrong-baud line decodes as noise and never passes the '$' test.
 */
static bool gps_listen_for_nmea(uint32_t window_ms)
{
  gps_sentence_seen = false;

  const uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < window_ms)
  {
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_2);
    if (gps_sentence_seen)
    {
      return true;
    }
  }
  return false;
}

/**
 * @brief Bring the link up at GPS_BAUD_FAST, whatever rate the module booted at.
 *
 * @return the rate actually in use — GPS_BAUD_FAST on success, GPS_BAUD_DEFAULT
 *         if the module had to be left slow. Never returns with the MCU and the
 *         module on different rates, so GPS degrades rather than going dark.
 */
static uint32_t gps_negotiate_baud(void)
{
  /* Case 1: warm reset — the module kept its fast rate from a previous run. */
  if (gps_listen_for_nmea(GPS_PROBE_WINDOW_MS))
  {
    return GPS_BAUD_FAST;
  }

  /* Case 2: cold start — it reverted to 9600. Drop down and command it up. */
  LOG_T(TAG_GPS, "No NMEA at %lu, trying %lu", (unsigned long)GPS_BAUD_FAST, (unsigned long)GPS_BAUD_DEFAULT);
  if (FEB_UART_SetBaudRate(FEB_UART_INSTANCE_2, GPS_BAUD_DEFAULT) != FEB_UART_OK)
  {
    LOG_E(TAG_GPS, "Cannot switch UART to %lu", (unsigned long)GPS_BAUD_DEFAULT);
    return GPS_BAUD_FAST; /* UART is untouched, so it is still at the fast rate */
  }

  const bool heard_slow = gps_listen_for_nmea(GPS_PROBE_WINDOW_MS);
  if (!heard_slow)
  {
    /* Silent at both rates. Still send the command — the module may just have
     * been mid-boot — but report the slow rate so the caller can back off. */
    LOG_W(TAG_GPS, "No NMEA at either baud; module may be absent");
  }

  FEB_GPS_SendPMTKCommand("PMTK251,115200");
  HAL_Delay(250); /* let the command drain and the module re-latch */

  if (FEB_UART_SetBaudRate(FEB_UART_INSTANCE_2, GPS_BAUD_FAST) != FEB_UART_OK)
  {
    return GPS_BAUD_DEFAULT;
  }
  if (gps_listen_for_nmea(GPS_PROBE_WINDOW_MS))
  {
    return GPS_BAUD_FAST;
  }

  /* PMTK251 did not take. Go back to whatever we could actually hear so the
   * module stays usable at 1 Hz instead of silent at 10 Hz. */
  LOG_W(TAG_GPS, "PMTK251 did not take; staying at %lu", (unsigned long)GPS_BAUD_DEFAULT);
  (void)FEB_UART_SetBaudRate(FEB_UART_INSTANCE_2, GPS_BAUD_DEFAULT);
  return GPS_BAUD_DEFAULT;
}

/**
 * @brief Initialize the GPS subsystem
 */
int FEB_GPS_Init(void)
{
  if (gps_initialized)
  {
    return 0;
  }

  /* Initialize LwGPS parser */
  lwgps_init(&gps_handle);

  /* Clear GPS data */
  memset(&gps_data, 0, sizeof(gps_data));

  /* Configure FEB_UART Instance 2 for GPS */
  FEB_UART_Config_t uart_cfg = {
      .huart = &huart4,
      .hdma_tx = &hdma_uart4_tx,
      .hdma_rx = &hdma_uart4_rx,
      .tx_buffer = gps_tx_buffer,
      .tx_buffer_size = sizeof(gps_tx_buffer),
      .rx_buffer = gps_rx_buffer,
      .rx_buffer_size = sizeof(gps_rx_buffer),
      .get_tick_ms = HAL_GetTick,
  };

  int result = FEB_UART_Init(FEB_UART_INSTANCE_2, &uart_cfg);
  if (result != FEB_UART_OK)
  {
    LOG_E(TAG_GPS, "UART init failed: %d", result);
    return result;
  }

  /* Set line callback for NMEA sentence processing */
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_2, gps_rx_line_callback);

  /* Enable GPS module */
  FEB_GPS_SetEnabled(true);

  /* Mark initialized before negotiating: SendPMTKCommand refuses to transmit
   * until this is set, and the negotiation needs it. */
  gps_initialized = true;
  gps_baud = GPS_BAUD_FAST;

  HAL_Delay(GPS_BOOT_SETTLE_MS);
  gps_baud = gps_negotiate_baud();

  LOG_T(TAG_GPS, "GPS initialized @ %lu baud", (unsigned long)gps_baud);

  return 0;
}

uint32_t FEB_GPS_GetBaudRate(void)
{
  return gps_baud;
}

uint32_t FEB_GPS_GetLastUpdateMs(void)
{
  /* Deliberately does not go through FEB_GPS_GetLatestData(): that call
   * consumes the "new data" flag. This accessor is what lets callers detect a
   * new fix without racing every other reader for that single flag. */
  return gps_data.last_update_ms;
}

bool FEB_GPS_IsFastLink(void)
{
  return gps_baud >= GPS_BAUD_FAST;
}

/**
 * @brief Deinitialize GPS and disable the module
 */
void FEB_GPS_DeInit(void)
{
  if (!gps_initialized)
  {
    return;
  }

  /* Disable GPS module first */
  FEB_GPS_SetEnabled(false);

  /* Deinit UART */
  FEB_UART_DeInit(FEB_UART_INSTANCE_2);

  gps_initialized = false;
  LOG_T(TAG_GPS, "GPS deinitialized");
}

/**
 * @brief Process received GPS data
 */
void FEB_GPS_Process(void)
{
  if (!gps_initialized)
  {
    return;
  }

  /* Process any pending UART data */
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_2);
}

/**
 * @brief Line callback for NMEA sentences
 */
static void gps_rx_line_callback(const char *line, size_t len)
{
  if (len == 0)
  {
    LOG_T(TAG_GPS, "RX: empty line");
    return;
  }

  if (line[0] != '$')
  {
    LOG_T(TAG_GPS, "RX: non-NMEA data (len=%zu, first=0x%02X)", len, (uint8_t)line[0]);
    return;
  }

  /* Any well-formed sentence proves the link is at the right baud. */
  gps_sentence_seen = true;

  /* Log raw NMEA sentence */
  LOG_T(TAG_GPS, "RX: %.*s", (int)len, line);

  /* Feed the NMEA sentence to LwGPS parser */
  /* Append \r because lwgps requires it to commit parsed data from temp storage */
  char line_with_cr[256];
  size_t parse_len = len;
  const char *parse_buf = line;
  if (len < sizeof(line_with_cr) - 2)
  {
    memcpy(line_with_cr, line, len);
    line_with_cr[len] = '\r';
    line_with_cr[len + 1] = '\0';
    parse_len = len + 1;
    parse_buf = line_with_cr;
  }
  uint8_t result = lwgps_process(&gps_handle, parse_buf, parse_len);

  if (result == 0)
  {
    LOG_W(TAG_GPS, "Parse failed for: %.20s...", line);
    return;
  }

  LOG_T(TAG_GPS, "Parsed %d statement(s)", result);

  /* Commit once per fix cycle, triggered by RMC.
   *
   * This used to gate on the UTC timestamp changing, but NMEA time has only
   * one-second resolution and lwgps_t stores h/m/s as uint8_t with no
   * fractional field — so above 1 Hz every fix after the first in a given
   * second was silently dropped, capping the whole pipeline at 1 Hz no matter
   * what PMTK220 asked for.
   *
   * The module emits one RMC per fix cycle, after GGA and GSA, so committing on
   * RMC captures a complete, self-consistent snapshot (position + time from RMC,
   * altitude from GGA, DOP from GSA) exactly once per cycle, at any update rate.
   * Every sentence is still parsed; only the snapshot is gated. */
  if (!nmea_is_sentence(line, len, "RMC"))
  {
    return;
  }

  /* Update our data structure from LwGPS */
  gps_data.latitude = gps_handle.latitude;
  gps_data.longitude = gps_handle.longitude;
  gps_data.altitude = gps_handle.altitude;
  gps_data.speed_kmh = lwgps_to_speed(gps_handle.speed, LWGPS_SPEED_KPH);
  gps_data.course = gps_handle.course;

  gps_data.hours = gps_handle.hours;
  gps_data.minutes = gps_handle.minutes;
  gps_data.seconds = gps_handle.seconds;

  gps_data.day = gps_handle.date;
  gps_data.month = gps_handle.month;
  gps_data.year = gps_handle.year;

  gps_data.fix = gps_handle.fix;
  gps_data.fix_mode = gps_handle.fix_mode;
  gps_data.sats_in_use = gps_handle.sats_in_use;
  gps_data.sats_in_view = gps_handle.sats_in_view;

  gps_data.hdop = gps_handle.dop_h;
  gps_data.vdop = gps_handle.dop_v;
  gps_data.pdop = gps_handle.dop_p;

  gps_data.valid = gps_handle.is_valid;
  gps_data.has_fix = (gps_handle.fix >= 1) && (gps_handle.fix_mode >= 2);
  gps_data.last_update_ms = HAL_GetTick();

  gps_data_updated = true;

  /* Log fix status changes */
  if (gps_data.has_fix && !gps_had_fix)
  {
    LOG_T(TAG_GPS, "Fix acquired: mode=%d, sats=%d", gps_data.fix_mode, gps_data.sats_in_use);
  }
  else if (!gps_data.has_fix && gps_had_fix)
  {
    LOG_T(TAG_GPS, "Fix lost");
  }
  gps_had_fix = gps_data.has_fix;

  /* Log detailed position data */
  LOG_T(TAG_GPS, "Pos: lat=%.6f, lon=%.6f, alt=%.1fm", gps_data.latitude, gps_data.longitude, gps_data.altitude);
  LOG_T(TAG_GPS, "Fix: type=%d, mode=%d, valid=%d, sats=%d/%d", gps_data.fix, gps_data.fix_mode, gps_data.valid,
        gps_data.sats_in_use, gps_data.sats_in_view);
  LOG_T(TAG_GPS, "DOP: h=%.2f, v=%.2f, p=%.2f", gps_data.hdop, gps_data.vdop, gps_data.pdop);
  LOG_T(TAG_GPS, "Motion: speed=%.1f km/h, course=%.1f deg", gps_data.speed_kmh, gps_data.course);
  LOG_T(TAG_GPS, "Time: %02d:%02d:%02d UTC, Date: %02d/%02d/20%02d", gps_data.hours, gps_data.minutes, gps_data.seconds,
        gps_data.month, gps_data.day, gps_data.year);
}

/**
 * @brief Get the latest parsed GPS data
 */
bool FEB_GPS_GetLatestData(FEB_GPS_Data_t *data)
{
  if (data == NULL)
  {
    return false;
  }

  *data = gps_data;

  bool was_updated = gps_data_updated;
  gps_data_updated = false;

  return was_updated;
}

/**
 * @brief Check if GPS has a valid fix
 */
bool FEB_GPS_HasFix(void)
{
  return gps_data.has_fix;
}

/**
 * @brief Calculate PMTK checksum (XOR of all chars between $ and *)
 */
static uint8_t pmtk_checksum(const char *cmd)
{
  uint8_t checksum = 0;
  while (*cmd)
  {
    checksum ^= (uint8_t)*cmd++;
  }
  return checksum;
}

/**
 * @brief Send a PMTK command to the GPS module
 */
int FEB_GPS_SendPMTKCommand(const char *cmd)
{
  if (!gps_initialized)
  {
    LOG_W(TAG_GPS, "SendPMTK: not initialized");
    return -1;
  }
  if (cmd == NULL)
  {
    LOG_W(TAG_GPS, "SendPMTK: NULL command");
    return -1;
  }

  /* Calculate checksum */
  uint8_t checksum = pmtk_checksum(cmd);

  LOG_T(TAG_GPS, "TX: $%s*%02X", cmd, checksum);

  /* Format and send: $<cmd>*XX\r\n */
  int result = FEB_UART_Printf(FEB_UART_INSTANCE_2, "$%s*%02X\r\n", cmd, checksum);
  if (result < 0)
  {
    LOG_E(TAG_GPS, "PMTK send failed: %d", result);
  }
  return result;
}

/**
 * @brief Set GPS update rate
 */
int FEB_GPS_SetUpdateRate(uint8_t hz)
{
  uint16_t period_ms;

  switch (hz)
  {
  case 1:
    period_ms = 1000;
    break;
  case 5:
    period_ms = 200;
    break;
  case 10:
    period_ms = 100;
    break;
  default:
    LOG_W(TAG_GPS, "Invalid update rate: %d Hz (must be 1, 5, or 10)", hz);
    return -1;
  }

  LOG_T(TAG_GPS, "Setting update rate to %d Hz (%d ms)", hz, period_ms);

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "PMTK220,%u", period_ms);

  return FEB_GPS_SendPMTKCommand(cmd);
}

/**
 * @brief Configure which NMEA sentences to output
 *
 * PMTK314 field order:
 * GLL, RMC, VTG, GGA, GSA, GSV, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
 */
int FEB_GPS_ConfigureOutput(bool gga, bool gsa, bool gsv, bool rmc)
{
  LOG_T(TAG_GPS, "Configuring output: GGA=%d, GSA=%d, GSV=%d, RMC=%d", gga, gsa, gsv, rmc);

  char cmd[64];

  /* Build PMTK314 command with desired sentence frequencies */
  snprintf(cmd, sizeof(cmd), "PMTK314,0,%d,0,%d,%d,%d,0,0,0,0,0,0,0,0,0,0,0,0,0", rmc ? 1 : 0, /* RMC */
           gga ? 1 : 0,                                                                        /* GGA */
           gsa ? 1 : 0,                                                                        /* GSA */
           gsv ? 1 : 0);                                                                       /* GSV */

  return FEB_GPS_SendPMTKCommand(cmd);
}

/**
 * @brief Enable or disable the GPS module
 */
void FEB_GPS_SetEnabled(bool enable)
{
  LOG_T(TAG_GPS, "GPS %s", enable ? "enabled" : "disabled");

  HAL_GPIO_WritePin(GPS_EN_GPIO_Port, GPS_EN_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);

  if (!enable)
  {
    /* Clear stale GPS data when disabling to prevent false fix reports */
    lwgps_init(&gps_handle);
    memset(&gps_data, 0, sizeof(gps_data));
    gps_data_updated = false;
    gps_had_fix = false;
    LOG_T(TAG_GPS, "Cleared GPS data and fix state");
  }
}

/**
 * @brief Check if GPS module is enabled
 */
bool FEB_GPS_IsEnabled(void)
{
  return HAL_GPIO_ReadPin(GPS_EN_GPIO_Port, GPS_EN_Pin) == GPIO_PIN_SET;
}
