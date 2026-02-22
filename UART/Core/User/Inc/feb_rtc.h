/**
 ******************************************************************************
 * @file           : feb_rtc.h
 * @brief          : FEB RTC Helper Functions
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides thread-safe RTC access for FreeRTOS tasks:
 *   - Get current date/time
 *   - Set date/time
 *   - Format date/time as strings
 *
 * Thread Safety:
 *   All functions use a mutex to protect RTC access. Safe to call from
 *   any FreeRTOS task concurrently. Do NOT call from ISR context.
 *
 * Usage:
 *   FEB_RTC_DateTime_t dt;
 *   if (FEB_RTC_GetDateTime(&dt) == FEB_RTC_OK) {
 *     char buf[20];
 *     FEB_RTC_FormatTime(&dt, buf, sizeof(buf));
 *     FEB_Console_Printf("Time: %s\r\n", buf);
 *   }
 *
 ******************************************************************************
 */

#ifndef FEB_RTC_H
#define FEB_RTC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

  /* ============================================================================
   * Return Codes
   * ============================================================================ */

  typedef enum
  {
    FEB_RTC_OK = 0,           /**< Success */
    FEB_RTC_ERROR = -1,       /**< HAL error */
    FEB_RTC_INVALID_ARG = -2, /**< Invalid argument */
    FEB_RTC_TIMEOUT = -3,     /**< Mutex timeout */
  } FEB_RTC_Status_t;

  /* ============================================================================
   * Data Types
   * ============================================================================ */

  /**
   * @brief Combined date/time structure
   *
   * All values are in binary format (not BCD).
   */
  typedef struct
  {
    /* Time */
    uint8_t hours;   /**< 0-23 */
    uint8_t minutes; /**< 0-59 */
    uint8_t seconds; /**< 0-59 */

    /* Date */
    uint8_t day;     /**< 1-31 */
    uint8_t month;   /**< 1-12 */
    uint16_t year;   /**< Full year (e.g., 2026) */
    uint8_t weekday; /**< 1=Monday, 7=Sunday */
  } FEB_RTC_DateTime_t;

  /* ============================================================================
   * Initialization
   * ============================================================================ */

  /**
   * @brief Initialize RTC helper module
   *
   * Creates mutex for thread-safe access. Call after FreeRTOS scheduler starts.
   *
   * @return FEB_RTC_OK on success
   */
  FEB_RTC_Status_t FEB_RTC_Init(void);

  /* ============================================================================
   * Get Functions
   * ============================================================================ */

  /**
   * @brief Get current date and time
   *
   * Thread-safe. Acquires mutex before HAL calls.
   *
   * @param dt Output structure (must not be NULL)
   * @return FEB_RTC_OK on success
   */
  FEB_RTC_Status_t FEB_RTC_GetDateTime(FEB_RTC_DateTime_t *dt);

  /**
   * @brief Get current time only
   *
   * @param hours   Output hours (0-23), can be NULL
   * @param minutes Output minutes (0-59), can be NULL
   * @param seconds Output seconds (0-59), can be NULL
   * @return FEB_RTC_OK on success
   */
  FEB_RTC_Status_t FEB_RTC_GetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);

  /**
   * @brief Get current date only
   *
   * @param day   Output day (1-31), can be NULL
   * @param month Output month (1-12), can be NULL
   * @param year  Output year (full, e.g., 2026), can be NULL
   * @return FEB_RTC_OK on success
   */
  FEB_RTC_Status_t FEB_RTC_GetDate(uint8_t *day, uint8_t *month, uint16_t *year);

  /* ============================================================================
   * Set Functions
   * ============================================================================ */

  /**
   * @brief Set date and time
   *
   * Thread-safe. Validates input before setting.
   *
   * @param dt Input structure with new date/time
   * @return FEB_RTC_OK on success, FEB_RTC_INVALID_ARG if values out of range
   */
  FEB_RTC_Status_t FEB_RTC_SetDateTime(const FEB_RTC_DateTime_t *dt);

  /**
   * @brief Set time only
   *
   * @param hours   Hours (0-23)
   * @param minutes Minutes (0-59)
   * @param seconds Seconds (0-59)
   * @return FEB_RTC_OK on success
   */
  FEB_RTC_Status_t FEB_RTC_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

  /**
   * @brief Set date only
   *
   * @param day   Day (1-31)
   * @param month Month (1-12)
   * @param year  Year (full, e.g., 2026)
   * @return FEB_RTC_OK on success
   */
  FEB_RTC_Status_t FEB_RTC_SetDate(uint8_t day, uint8_t month, uint16_t year);

  /* ============================================================================
   * Formatting Functions
   * ============================================================================ */

  /**
   * @brief Format date/time as ISO 8601 string
   *
   * Output format: "YYYY-MM-DD HH:MM:SS"
   *
   * @param dt     Date/time to format
   * @param buffer Output buffer (min 20 bytes)
   * @param size   Buffer size
   * @return Number of characters written (excluding null), or -1 on error
   */
  int FEB_RTC_FormatDateTime(const FEB_RTC_DateTime_t *dt, char *buffer, size_t size);

  /**
   * @brief Format time as string
   *
   * Output format: "HH:MM:SS"
   *
   * @param dt     Date/time to format
   * @param buffer Output buffer (min 9 bytes)
   * @param size   Buffer size
   * @return Number of characters written
   */
  int FEB_RTC_FormatTime(const FEB_RTC_DateTime_t *dt, char *buffer, size_t size);

  /**
   * @brief Format date as string
   *
   * Output format: "YYYY-MM-DD"
   *
   * @param dt     Date/time to format
   * @param buffer Output buffer (min 11 bytes)
   * @param size   Buffer size
   * @return Number of characters written
   */
  int FEB_RTC_FormatDate(const FEB_RTC_DateTime_t *dt, char *buffer, size_t size);

  /**
   * @brief Get weekday name
   *
   * @param weekday Weekday number (1=Monday, 7=Sunday)
   * @return Weekday name string, or "???" if invalid
   */
  const char *FEB_RTC_GetWeekdayName(uint8_t weekday);

#ifdef __cplusplus
}
#endif

#endif /* FEB_RTC_H */
