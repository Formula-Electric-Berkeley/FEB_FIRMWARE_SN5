/**
 ******************************************************************************
 * @file           : FEB_GPS.h
 * @brief          : GPS driver for MTK3339 using FEB_UART and LwGPS
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides GPS functionality for the Sensor Node:
 *   - Uses FEB_UART Instance 2 for UART4 (DMA-based NMEA reception)
 *   - LwGPS library for NMEA parsing
 *   - GPS_EN pin (PD2) control for power management
 *   - PMTK command interface for module configuration
 *
 * Hardware:
 *   - UART4: PC10 (TX), PC11 (RX) @ 9600 baud
 *   - DMA1_Stream2 (RX), DMA1_Stream4 (TX)
 *   - GPS_EN: PD2 (active high)
 *
 ******************************************************************************
 */

#ifndef FEB_GPS_H
#define FEB_GPS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  /**
   * @brief GPS data structure with all parsed information
   */
  typedef struct
  {
    /* Position */
    double latitude;  /**< Latitude in degrees (negative = South) */
    double longitude; /**< Longitude in degrees (negative = West) */
    double altitude;  /**< Altitude in meters above MSL */

    /* Motion */
    double speed_kmh; /**< Ground speed in km/h */
    double course;    /**< Course over ground in degrees */

    /* Time (UTC) */
    uint8_t hours;   /**< UTC hours (0-23) */
    uint8_t minutes; /**< UTC minutes (0-59) */
    uint8_t seconds; /**< UTC seconds (0-59) */

    /* Date (UTC) */
    uint8_t day;   /**< UTC day of month (1-31) */
    uint8_t month; /**< UTC month (1-12) */
    uint8_t year;  /**< UTC year (2-digit, e.g., 24 = 2024) */

    /* Fix Quality */
    uint8_t fix;          /**< Fix type: 0=Invalid, 1=GPS, 2=DGPS, 3=PPS */
    uint8_t fix_mode;     /**< Fix mode: 1=No fix, 2=2D, 3=3D */
    uint8_t sats_in_use;  /**< Satellites used in solution */
    uint8_t sats_in_view; /**< Satellites visible */

    /* Accuracy */
    double hdop; /**< Horizontal dilution of precision */
    double vdop; /**< Vertical dilution of precision */
    double pdop; /**< Position dilution of precision */

    /* Status */
    bool valid;              /**< True if GPS signal is valid */
    bool has_fix;            /**< True if we have at least a 2D fix */
    uint32_t last_update_ms; /**< Timestamp of last valid update */
  } FEB_GPS_Data_t;

  /**
   * @brief Initialize the GPS subsystem
   *
   * Configures UART4 via FEB_UART Instance 2, initializes LwGPS,
   * and enables the GPS module via GPS_EN pin.
   *
   * @return 0 on success, negative error code on failure
   */
  int FEB_GPS_Init(void);

  /**
   * @brief Deinitialize GPS and disable the module
   *
   * Stops UART reception and disables GPS_EN pin.
   */
  void FEB_GPS_DeInit(void);

  /**
   * @brief Process received GPS data
   *
   * Must be called periodically from the main loop. Processes any
   * pending UART data and updates the internal GPS state.
   */
  void FEB_GPS_Process(void);

  /**
   * @brief Get the latest parsed GPS data
   *
   * @param data Pointer to structure to fill with current GPS data
   * @return true if data was updated since last call, false otherwise
   */
  bool FEB_GPS_GetLatestData(FEB_GPS_Data_t *data);

  /**
   * @brief Check if GPS has a valid fix
   *
   * @return true if GPS has at least a 2D fix
   */
  bool FEB_GPS_HasFix(void);

  /**
   * @brief Send a PMTK command to the GPS module
   *
   * Automatically calculates and appends the checksum.
   *
   * @param cmd Command without $ prefix or checksum (e.g., "PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0")
   * @return 0 on success, negative error code on failure
   */
  int FEB_GPS_SendPMTKCommand(const char *cmd);

  /**
   * @brief Set GPS update rate
   *
   * @param hz Update rate in Hz (1, 5, or 10)
   * @return 0 on success, negative error code on failure
   */
  int FEB_GPS_SetUpdateRate(uint8_t hz);

  /**
   * @brief Configure which NMEA sentences to output
   *
   * @param gga Enable GGA (position, altitude, satellites)
   * @param gsa Enable GSA (DOP and active satellites)
   * @param gsv Enable GSV (satellites in view)
   * @param rmc Enable RMC (position, velocity, time)
   * @return 0 on success, negative error code on failure
   */
  int FEB_GPS_ConfigureOutput(bool gga, bool gsa, bool gsv, bool rmc);

  /**
   * @brief Enable or disable the GPS module
   *
   * Controls the GPS_EN pin (PD2).
   *
   * @param enable true to enable, false to disable
   */
  void FEB_GPS_SetEnabled(bool enable);

  /**
   * @brief Check if GPS module is enabled
   *
   * @return true if GPS_EN pin is high
   */
  bool FEB_GPS_IsEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_GPS_H */
