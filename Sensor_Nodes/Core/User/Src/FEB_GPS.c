#include "FEB_GPS.h"

#include <stdlib.h>
#include <string.h>

#define FEB_GPS_ENABLE_ACTIVE_HIGH 1U

static UART_HandleTypeDef *s_gps_uart = NULL;
static GPIO_TypeDef *s_gps_en_port = NULL;
static uint16_t s_gps_en_pin = 0U;

static volatile uint8_t s_rx_byte = 0U;
static volatile bool s_line_ready = false;
static char s_work_line[FEB_GPS_LINE_MAX_LEN];
static char s_ready_line[FEB_GPS_LINE_MAX_LEN];
static uint16_t s_work_idx = 0U;
static FEB_GPS_Fix_t s_last_fix = {0.0f, 0.0f, 0U};

static bool starts_with(const char *s, const char *prefix)
{
  return (strncmp(s, prefix, strlen(prefix)) == 0);
}

static float nmea_coord_to_decimal(const char *coord)
{
  const char *dot_pos;
  int digits_before_dot;
  int deg_len;
  char deg_part[8];
  int deg;
  float min;

  if (coord == NULL)
  {
    return 0.0f;
  }

  dot_pos = strchr(coord, '.');
  if ((dot_pos == NULL) || ((dot_pos - coord) < 3))
  {
    return 0.0f;
  }

  digits_before_dot = (int)(dot_pos - coord);
  deg_len = digits_before_dot - 2;
  if ((deg_len <= 0) || (deg_len >= (int)sizeof(deg_part)))
  {
    return 0.0f;
  }

  (void)memcpy(deg_part, coord, (size_t)deg_len);
  deg_part[deg_len] = '\0';
  deg = atoi(deg_part);
  min = (float)atof(coord + deg_len);

  return (float)deg + (min / 60.0f);
}

static bool parse_gga(char *line, FEB_GPS_Fix_t *out_fix)
{
  char *saveptr = NULL;
  char *token;
  char *field[8] = {0};
  uint8_t i;
  float lat;
  float lon;

  token = strtok_r(line, ",", &saveptr);
  i = 0U;
  while ((token != NULL) && (i < 8U))
  {
    field[i] = token;
    i++;
    token = strtok_r(NULL, ",", &saveptr);
  }

  if (i < 7U)
  {
    return false;
  }

  if ((field[2] == NULL) || (field[3] == NULL) || (field[4] == NULL) || (field[5] == NULL) || (field[6] == NULL))
  {
    return false;
  }

  if (field[6][0] == '0')
  {
    return false;
  }

  lat = nmea_coord_to_decimal(field[2]);
  lon = nmea_coord_to_decimal(field[4]);

  if ((field[3][0] == 'S') || (field[3][0] == 's'))
  {
    lat = -lat;
  }
  if ((field[5][0] == 'W') || (field[5][0] == 'w'))
  {
    lon = -lon;
  }

  out_fix->latitude_deg = lat;
  out_fix->longitude_deg = lon;
  out_fix->valid = 1U;
  return true;
}

static bool parse_rmc(char *line, FEB_GPS_Fix_t *out_fix)
{
  char *saveptr = NULL;
  char *token;
  char *field[8] = {0};
  uint8_t i;
  float lat;
  float lon;

  token = strtok_r(line, ",", &saveptr);
  i = 0U;
  while ((token != NULL) && (i < 8U))
  {
    field[i] = token;
    i++;
    token = strtok_r(NULL, ",", &saveptr);
  }

  if (i < 7U)
  {
    return false;
  }

  if ((field[2] == NULL) || (field[3] == NULL) || (field[4] == NULL) || (field[5] == NULL) || (field[6] == NULL))
  {
    return false;
  }

  if ((field[2][0] != 'A') && (field[2][0] != 'a'))
  {
    return false;
  }

  lat = nmea_coord_to_decimal(field[3]);
  lon = nmea_coord_to_decimal(field[5]);

  if ((field[4][0] == 'S') || (field[4][0] == 's'))
  {
    lat = -lat;
  }
  if ((field[6][0] == 'W') || (field[6][0] == 'w'))
  {
    lon = -lon;
  }

  out_fix->latitude_deg = lat;
  out_fix->longitude_deg = lon;
  out_fix->valid = 1U;
  return true;
}

void FEB_GPS_Init(UART_HandleTypeDef *huart, GPIO_TypeDef *en_port, uint16_t en_pin)
{
  s_gps_uart = huart;
  s_gps_en_port = en_port;
  s_gps_en_pin = en_pin;
  s_line_ready = false;
  s_work_idx = 0U;
  s_work_line[0] = '\0';
  s_ready_line[0] = '\0';
}

void FEB_GPS_SetPower(bool enable)
{
  GPIO_PinState pin_state;

#if FEB_GPS_ENABLE_ACTIVE_HIGH
  pin_state = enable ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
  pin_state = enable ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif

  if (s_gps_en_port != NULL)
  {
    HAL_GPIO_WritePin(s_gps_en_port, s_gps_en_pin, pin_state);
  }
}

HAL_StatusTypeDef FEB_GPS_Start(void)
{
  if (s_gps_uart == NULL)
  {
    return HAL_ERROR;
  }

  FEB_GPS_SetPower(true);
  return HAL_UART_Receive_IT(s_gps_uart, (uint8_t *)&s_rx_byte, 1U);
}

void FEB_GPS_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  char ch;

  if ((s_gps_uart == NULL) || (huart != s_gps_uart))
  {
    return;
  }

  ch = (char)s_rx_byte;

  if ((ch == '\n') && (s_work_idx > 0U))
  {
    s_work_line[s_work_idx] = '\0';
    (void)strncpy(s_ready_line, s_work_line, FEB_GPS_LINE_MAX_LEN);
    s_ready_line[FEB_GPS_LINE_MAX_LEN - 1U] = '\0';
    s_line_ready = true;
    s_work_idx = 0U;
  }
  else if (ch != '\r')
  {
    if (s_work_idx < (FEB_GPS_LINE_MAX_LEN - 1U))
    {
      s_work_line[s_work_idx] = ch;
      s_work_idx++;
    }
    else
    {
      s_work_idx = 0U;
    }
  }

  (void)HAL_UART_Receive_IT(s_gps_uart, (uint8_t *)&s_rx_byte, 1U);
}

bool FEB_GPS_ReadLine(char *out_line, uint16_t out_size)
{
  if ((out_line == NULL) || (out_size == 0U) || !s_line_ready)
  {
    return false;
  }

  __disable_irq();
  s_line_ready = false;
  (void)strncpy(out_line, s_ready_line, out_size);
  __enable_irq();

  out_line[out_size - 1U] = '\0';
  return true;
}

bool FEB_GPS_ProcessLine(const char *nmea_line)
{
  char line_copy[FEB_GPS_LINE_MAX_LEN];
  FEB_GPS_Fix_t parsed = {0.0f, 0.0f, 0U};
  bool ok = false;

  if (nmea_line == NULL)
  {
    return false;
  }

  (void)strncpy(line_copy, nmea_line, sizeof(line_copy));
  line_copy[sizeof(line_copy) - 1U] = '\0';

  if (starts_with(line_copy, "$GPGGA") || starts_with(line_copy, "$GNGGA"))
  {
    ok = parse_gga(line_copy, &parsed);
  }
  else if (starts_with(line_copy, "$GPRMC") || starts_with(line_copy, "$GNRMC"))
  {
    ok = parse_rmc(line_copy, &parsed);
  }

  if (ok)
  {
    s_last_fix = parsed;
  }

  return ok;
}

bool FEB_GPS_ProcessPendingLine(char *out_line, uint16_t out_size)
{
  bool has_line;

  has_line = FEB_GPS_ReadLine(out_line, out_size);
  if (!has_line)
  {
    return false;
  }

  return FEB_GPS_ProcessLine(out_line);
}

bool FEB_GPS_GetLastFix(FEB_GPS_Fix_t *out_fix)
{
  if ((out_fix == NULL) || (s_last_fix.valid == 0U))
  {
    return false;
  }

  *out_fix = s_last_fix;
  return true;
}

void FEB_GPS_FixToBytes(const FEB_GPS_Fix_t *fix, uint8_t out_8bytes[8])
{
  const uint8_t *lon_bytes;
  const uint8_t *lat_bytes;

  if ((fix == NULL) || (out_8bytes == NULL))
  {
    return;
  }

  lon_bytes = (const uint8_t *)&fix->longitude_deg;
  lat_bytes = (const uint8_t *)&fix->latitude_deg;

  out_8bytes[0] = lon_bytes[0];
  out_8bytes[1] = lon_bytes[1];
  out_8bytes[2] = lon_bytes[2];
  out_8bytes[3] = lon_bytes[3];
  out_8bytes[4] = lat_bytes[0];
  out_8bytes[5] = lat_bytes[1];
  out_8bytes[6] = lat_bytes[2];
  out_8bytes[7] = lat_bytes[3];
}
