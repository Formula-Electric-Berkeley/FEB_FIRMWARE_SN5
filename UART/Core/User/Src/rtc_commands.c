/**
 ******************************************************************************
 * @file           : rtc_commands.c
 * @brief          : RTC Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "rtc_commands.h"
#include "feb_console.h"
#include "feb_rtc.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_rtc(int argc, char *argv[]);
static void print_rtc_help(void);
static void cmd_get(void);
static void cmd_time(void);
static void cmd_date(void);
static void cmd_set(int argc, char *argv[]);
static void cmd_settime(int argc, char *argv[]);
static void cmd_setdate(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptor
 * ============================================================================ */

const FEB_Console_Cmd_t rtc_cmd = {
    .name = "rtc",
    .help = "RTC commands: rtc|get, rtc|time, rtc|date, rtc|set|YYYY|MM|DD|HH|MM|SS, rtc|settime|HH|MM|SS, "
            "rtc|setdate|YYYY|MM|DD",
    .handler = cmd_rtc,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void RTC_RegisterCommands(void)
{
  FEB_Console_Register(&rtc_cmd);
}

/* ============================================================================
 * Private Helper Functions
 * ============================================================================ */

static int strcasecmp_local(const char *s1, const char *s2)
{
  while (*s1 && *s2)
  {
    int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (diff != 0)
      return diff;
    s1++;
    s2++;
  }
  return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static void print_status_error(FEB_RTC_Status_t status)
{
  switch (status)
  {
  case FEB_RTC_ERROR:
    FEB_Console_Printf("Error: RTC HAL operation failed\r\n");
    break;
  case FEB_RTC_INVALID_ARG:
    FEB_Console_Printf("Error: Invalid argument\r\n");
    break;
  case FEB_RTC_TIMEOUT:
    FEB_Console_Printf("Error: RTC busy (mutex timeout)\r\n");
    break;
  default:
    FEB_Console_Printf("Error: Unknown error (%d)\r\n", status);
    break;
  }
}

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

static void print_rtc_help(void)
{
  FEB_Console_Printf("RTC Commands:\r\n");
  FEB_Console_Printf("  rtc|get                      - Display current date/time\r\n");
  FEB_Console_Printf("  rtc|time                     - Display time only\r\n");
  FEB_Console_Printf("  rtc|date                     - Display date only\r\n");
  FEB_Console_Printf("  rtc|set|YYYY|MM|DD|HH|MM|SS  - Set full date/time\r\n");
  FEB_Console_Printf("  rtc|settime|HH|MM|SS         - Set time only\r\n");
  FEB_Console_Printf("  rtc|setdate|YYYY|MM|DD       - Set date only\r\n");
}

static void cmd_rtc(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_rtc_help();
    return;
  }

  const char *subcmd = argv[1];

  if (strcasecmp_local(subcmd, "get") == 0)
  {
    cmd_get();
  }
  else if (strcasecmp_local(subcmd, "time") == 0)
  {
    cmd_time();
  }
  else if (strcasecmp_local(subcmd, "date") == 0)
  {
    cmd_date();
  }
  else if (strcasecmp_local(subcmd, "set") == 0)
  {
    cmd_set(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "settime") == 0)
  {
    cmd_settime(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "setdate") == 0)
  {
    cmd_setdate(argc - 1, argv + 1);
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_rtc_help();
  }
}

static void cmd_get(void)
{
  FEB_RTC_DateTime_t dt;
  FEB_RTC_Status_t status = FEB_RTC_GetDateTime(&dt);

  if (status != FEB_RTC_OK)
  {
    print_status_error(status);
    return;
  }

  char datetime_buf[24];
  FEB_RTC_FormatDateTime(&dt, datetime_buf, sizeof(datetime_buf));

  FEB_Console_Printf("%s (%s)\r\n", datetime_buf, FEB_RTC_GetWeekdayName(dt.weekday));
}

static void cmd_time(void)
{
  FEB_RTC_DateTime_t dt;
  FEB_RTC_Status_t status = FEB_RTC_GetDateTime(&dt);

  if (status != FEB_RTC_OK)
  {
    print_status_error(status);
    return;
  }

  char time_buf[12];
  FEB_RTC_FormatTime(&dt, time_buf, sizeof(time_buf));

  FEB_Console_Printf("%s\r\n", time_buf);
}

static void cmd_date(void)
{
  FEB_RTC_DateTime_t dt;
  FEB_RTC_Status_t status = FEB_RTC_GetDateTime(&dt);

  if (status != FEB_RTC_OK)
  {
    print_status_error(status);
    return;
  }

  char date_buf[12];
  FEB_RTC_FormatDate(&dt, date_buf, sizeof(date_buf));

  FEB_Console_Printf("%s (%s)\r\n", date_buf, FEB_RTC_GetWeekdayName(dt.weekday));
}

static void cmd_set(int argc, char *argv[])
{
  /* Expect: set|YYYY|MM|DD|HH|MM|SS (7 args including "set") */
  if (argc < 7)
  {
    FEB_Console_Printf("Usage: rtc|set|YYYY|MM|DD|HH|MM|SS\r\n");
    FEB_Console_Printf("Example: rtc|set|2026|02|13|14|30|00\r\n");
    return;
  }

  FEB_RTC_DateTime_t dt;
  dt.year = (uint16_t)strtoul(argv[1], NULL, 10);
  dt.month = (uint8_t)strtoul(argv[2], NULL, 10);
  dt.day = (uint8_t)strtoul(argv[3], NULL, 10);
  dt.hours = (uint8_t)strtoul(argv[4], NULL, 10);
  dt.minutes = (uint8_t)strtoul(argv[5], NULL, 10);
  dt.seconds = (uint8_t)strtoul(argv[6], NULL, 10);

  FEB_RTC_Status_t status = FEB_RTC_SetDateTime(&dt);

  if (status != FEB_RTC_OK)
  {
    print_status_error(status);
    return;
  }

  /* Read back and display */
  status = FEB_RTC_GetDateTime(&dt);
  if (status == FEB_RTC_OK)
  {
    char datetime_buf[24];
    FEB_RTC_FormatDateTime(&dt, datetime_buf, sizeof(datetime_buf));
    FEB_Console_Printf("RTC set to: %s (%s)\r\n", datetime_buf, FEB_RTC_GetWeekdayName(dt.weekday));
  }
  else
  {
    FEB_Console_Printf("RTC set (readback failed)\r\n");
  }
}

static void cmd_settime(int argc, char *argv[])
{
  /* Expect: settime|HH|MM|SS (4 args including "settime") */
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: rtc|settime|HH|MM|SS\r\n");
    FEB_Console_Printf("Example: rtc|settime|14|30|00\r\n");
    return;
  }

  uint8_t hours = (uint8_t)strtoul(argv[1], NULL, 10);
  uint8_t minutes = (uint8_t)strtoul(argv[2], NULL, 10);
  uint8_t seconds = (uint8_t)strtoul(argv[3], NULL, 10);

  FEB_RTC_Status_t status = FEB_RTC_SetTime(hours, minutes, seconds);

  if (status != FEB_RTC_OK)
  {
    print_status_error(status);
    return;
  }

  FEB_Console_Printf("Time set to: %02u:%02u:%02u\r\n", hours, minutes, seconds);
}

static void cmd_setdate(int argc, char *argv[])
{
  /* Expect: setdate|YYYY|MM|DD (4 args including "setdate") */
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: rtc|setdate|YYYY|MM|DD\r\n");
    FEB_Console_Printf("Example: rtc|setdate|2026|02|13\r\n");
    return;
  }

  uint16_t year = (uint16_t)strtoul(argv[1], NULL, 10);
  uint8_t month = (uint8_t)strtoul(argv[2], NULL, 10);
  uint8_t day = (uint8_t)strtoul(argv[3], NULL, 10);

  FEB_RTC_Status_t status = FEB_RTC_SetDate(day, month, year);

  if (status != FEB_RTC_OK)
  {
    print_status_error(status);
    return;
  }

  FEB_Console_Printf("Date set to: %04u-%02u-%02u\r\n", year, month, day);
}
