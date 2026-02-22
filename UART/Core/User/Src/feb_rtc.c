/**
 ******************************************************************************
 * @file           : feb_rtc.c
 * @brief          : FEB RTC Helper Functions Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_rtc.h"
#include "cmsis_os2.h"
#include "rtc.h"
#include <stdio.h>

/* ============================================================================
 * Private Variables
 * ============================================================================ */

static osMutexId_t rtc_mutex = NULL;

static const osMutexAttr_t rtc_mutex_attr = {
    .name = "rtc_mutex",
    .attr_bits = osMutexRecursive | osMutexPrioInherit,
    .cb_mem = NULL,
    .cb_size = 0,
};

static const char *weekday_names[] = {"???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

/* Mutex timeout in ticks */
#define RTC_MUTEX_TIMEOUT_MS 100

/* ============================================================================
 * Private Functions
 * ============================================================================ */

static bool validate_time(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  return (hours <= 23) && (minutes <= 59) && (seconds <= 59);
}

static bool validate_date(uint8_t day, uint8_t month, uint16_t year)
{
  if (month < 1 || month > 12)
    return false;
  if (day < 1 || day > 31)
    return false;
  if (year < 2000 || year > 2099)
    return false;
  return true;
}

static bool acquire_mutex(void)
{
  if (rtc_mutex == NULL)
    return false;
  return osMutexAcquire(rtc_mutex, RTC_MUTEX_TIMEOUT_MS) == osOK;
}

static void release_mutex(void)
{
  if (rtc_mutex != NULL)
  {
    osMutexRelease(rtc_mutex);
  }
}

/* Calculate weekday using Zeller-like formula (1=Mon, 7=Sun) */
static uint8_t calculate_weekday(uint8_t day, uint8_t month, uint16_t year)
{
  /* Adjust for Jan/Feb being months 13/14 of previous year */
  uint16_t y = year;
  uint8_t m = month;
  if (m < 3)
  {
    m += 12;
    y--;
  }

  /* Zeller's congruence adapted for Monday=1 */
  int k = y % 100;
  int j = y / 100;
  int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;

  /* Convert from Zeller (0=Sat) to our format (1=Mon, 7=Sun) */
  int weekday = ((h + 5) % 7) + 1;
  return (uint8_t)weekday;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

FEB_RTC_Status_t FEB_RTC_Init(void)
{
  if (rtc_mutex != NULL)
  {
    return FEB_RTC_OK; /* Already initialized */
  }

  rtc_mutex = osMutexNew(&rtc_mutex_attr);
  if (rtc_mutex == NULL)
  {
    return FEB_RTC_ERROR;
  }

  return FEB_RTC_OK;
}

/* ============================================================================
 * Get Functions
 * ============================================================================ */

FEB_RTC_Status_t FEB_RTC_GetDateTime(FEB_RTC_DateTime_t *dt)
{
  if (dt == NULL)
  {
    return FEB_RTC_INVALID_ARG;
  }

  if (!acquire_mutex())
  {
    return FEB_RTC_TIMEOUT;
  }

  RTC_TimeTypeDef time;
  RTC_DateTypeDef date;

  /* IMPORTANT: Must read time first, then date, per STM32 HAL requirement */
  HAL_StatusTypeDef status = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  if (status != HAL_OK)
  {
    release_mutex();
    return FEB_RTC_ERROR;
  }

  status = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
  if (status != HAL_OK)
  {
    release_mutex();
    return FEB_RTC_ERROR;
  }

  release_mutex();

  /* Fill output structure */
  dt->hours = time.Hours;
  dt->minutes = time.Minutes;
  dt->seconds = time.Seconds;
  dt->day = date.Date;
  dt->month = date.Month;
  dt->year = 2000 + date.Year; /* RTC stores year as 0-99 */
  dt->weekday = date.WeekDay;

  return FEB_RTC_OK;
}

FEB_RTC_Status_t FEB_RTC_GetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
  FEB_RTC_DateTime_t dt;
  FEB_RTC_Status_t status = FEB_RTC_GetDateTime(&dt);
  if (status != FEB_RTC_OK)
  {
    return status;
  }

  if (hours != NULL)
    *hours = dt.hours;
  if (minutes != NULL)
    *minutes = dt.minutes;
  if (seconds != NULL)
    *seconds = dt.seconds;

  return FEB_RTC_OK;
}

FEB_RTC_Status_t FEB_RTC_GetDate(uint8_t *day, uint8_t *month, uint16_t *year)
{
  FEB_RTC_DateTime_t dt;
  FEB_RTC_Status_t status = FEB_RTC_GetDateTime(&dt);
  if (status != FEB_RTC_OK)
  {
    return status;
  }

  if (day != NULL)
    *day = dt.day;
  if (month != NULL)
    *month = dt.month;
  if (year != NULL)
    *year = dt.year;

  return FEB_RTC_OK;
}

/* ============================================================================
 * Set Functions
 * ============================================================================ */

FEB_RTC_Status_t FEB_RTC_SetDateTime(const FEB_RTC_DateTime_t *dt)
{
  if (dt == NULL)
  {
    return FEB_RTC_INVALID_ARG;
  }

  if (!validate_time(dt->hours, dt->minutes, dt->seconds))
  {
    return FEB_RTC_INVALID_ARG;
  }

  if (!validate_date(dt->day, dt->month, dt->year))
  {
    return FEB_RTC_INVALID_ARG;
  }

  if (!acquire_mutex())
  {
    return FEB_RTC_TIMEOUT;
  }

  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  time.Hours = dt->hours;
  time.Minutes = dt->minutes;
  time.Seconds = dt->seconds;

  date.Date = dt->day;
  date.Month = dt->month;
  date.Year = dt->year - 2000; /* RTC stores year as 0-99 */
  date.WeekDay = calculate_weekday(dt->day, dt->month, dt->year);

  HAL_StatusTypeDef status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
  if (status != HAL_OK)
  {
    release_mutex();
    return FEB_RTC_ERROR;
  }

  status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
  if (status != HAL_OK)
  {
    release_mutex();
    return FEB_RTC_ERROR;
  }

  release_mutex();
  return FEB_RTC_OK;
}

FEB_RTC_Status_t FEB_RTC_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  if (!validate_time(hours, minutes, seconds))
  {
    return FEB_RTC_INVALID_ARG;
  }

  if (!acquire_mutex())
  {
    return FEB_RTC_TIMEOUT;
  }

  RTC_TimeTypeDef time = {0};
  time.Hours = hours;
  time.Minutes = minutes;
  time.Seconds = seconds;

  HAL_StatusTypeDef status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);

  release_mutex();

  return (status == HAL_OK) ? FEB_RTC_OK : FEB_RTC_ERROR;
}

FEB_RTC_Status_t FEB_RTC_SetDate(uint8_t day, uint8_t month, uint16_t year)
{
  if (!validate_date(day, month, year))
  {
    return FEB_RTC_INVALID_ARG;
  }

  if (!acquire_mutex())
  {
    return FEB_RTC_TIMEOUT;
  }

  RTC_DateTypeDef date = {0};
  date.Date = day;
  date.Month = month;
  date.Year = year - 2000;
  date.WeekDay = calculate_weekday(day, month, year);

  HAL_StatusTypeDef status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);

  release_mutex();

  return (status == HAL_OK) ? FEB_RTC_OK : FEB_RTC_ERROR;
}

/* ============================================================================
 * Formatting Functions
 * ============================================================================ */

int FEB_RTC_FormatDateTime(const FEB_RTC_DateTime_t *dt, char *buffer, size_t size)
{
  if (dt == NULL || buffer == NULL || size < 20)
  {
    return -1;
  }

  return snprintf(buffer, size, "%04u-%02u-%02u %02u:%02u:%02u", dt->year, dt->month, dt->day, dt->hours, dt->minutes,
                  dt->seconds);
}

int FEB_RTC_FormatTime(const FEB_RTC_DateTime_t *dt, char *buffer, size_t size)
{
  if (dt == NULL || buffer == NULL || size < 9)
  {
    return -1;
  }

  return snprintf(buffer, size, "%02u:%02u:%02u", dt->hours, dt->minutes, dt->seconds);
}

int FEB_RTC_FormatDate(const FEB_RTC_DateTime_t *dt, char *buffer, size_t size)
{
  if (dt == NULL || buffer == NULL || size < 11)
  {
    return -1;
  }

  return snprintf(buffer, size, "%04u-%02u-%02u", dt->year, dt->month, dt->day);
}

const char *FEB_RTC_GetWeekdayName(uint8_t weekday)
{
  if (weekday < 1 || weekday > 7)
  {
    return weekday_names[0]; /* "???" */
  }
  return weekday_names[weekday];
}
