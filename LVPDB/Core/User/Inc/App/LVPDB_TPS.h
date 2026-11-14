/**
 ******************************************************************************
 * @file           : LVPDB_TPS.h
 * @brief          : TPS2482 power rail management
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef LVPDB_TPS_H
#define LVPDB_TPS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* HAL first: feb_tps.h names I2C_HandleTypeDef and GPIO_TypeDef without
 * including the HAL header itself. */
#include <stm32f4xx_hal.h>

#include "feb_tps.h"

#include <stdbool.h>
#include <stdint.h>

  /*
   * The LVPDB has multiple TPS chips on the bus. These are the addresses of
   * each of the TPS chips. The naming conventions is as follows:
   *		LV - Low Voltage Source (sda-scl)
   *		SH - Shutdown Source (sda-sda)
   *		AF1_AF2 - Accumulator Fans 1 Branch (gnd-vs)
   *		CP - Coolant Pump Branch ()
   *    RF - Radiator Fans Branch (vs-scl)
   */

#define NUM_TPS2482 5

#define LV_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL)     // A1:SDA  A0:SCL
#define SH_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND)     // A1:GND  A0:GND
#define AF1_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS) // A1:GND  A0:VS
#define CP_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_VS, FEB_TPS_PIN_SCL)      // A1:VS   A0:SCL
#define RF_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA)     // A1:GND  A0:SDA

// All TPS2482 implementation share the same WSR52L000FEA .002 ohm shunt resistor
#define R_SHUNT (double)(.002) // Ohm

// Fuse max are the current ratings for the fuses, and are used as current maximums
// Fuse max can be exceeded safely in "peak current" scenarios so maybe not best metric
#define BATTERY_FUSE_MAX (double)(30) // A from +24BAT -> +24GLV
#define LV_FUSE_MAX (double)(5)       // A from +24GLV -> +24V
#define SH_FUSE_MAX (double)(5)       // TODO: ??
#define AF1_FUSE_MAX (double)(12)
#define CP_FUSE_MAX (double)(7)
#define RF_FUSE_MAX (double)(7) // TODO ?????

#define FLOAT_TO_UINT16_T(n) ((uint16_t)(n * 1000))                                           // for voltage (mV)
#define FLOAT_TO_INT16_T(n) ((int16_t)(n * 1000))                                             // for voltage (mV)
#define SIGN_MAGNITUDE(n) (int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)) // for current reg

// Current LSB values (fuse max gives resolution at cost of range)
#define LV_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(LV_FUSE_MAX)
#define SH_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(SH_FUSE_MAX)
#define AF1_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(AF1_FUSE_MAX)
#define CP_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(CP_FUSE_MAX)
#define RF_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(RF_FUSE_MAX)

// Power LSB values
#define LV_POWER_LSB FEB_TPS_CALC_POWER_LSB(LV_CURRENT_LSB)
#define SH_POWER_LSB FEB_TPS_CALC_POWER_LSB(SH_CURRENT_LSB)
#define AF1_POWER_LSB FEB_TPS_CALC_POWER_LSB(AF1_CURRENT_LSB)
#define CP_POWER_LSB FEB_TPS_CALC_POWER_LSB(CP_CURRENT_LSB)
#define RF_POWER_LSB FEB_TPS_CALC_POWER_LSB(RF_CURRENT_LSB)

  /* ============================================================================
   * Device Handles and Measurement Data
   * ============================================================================ */

  // Device handles (in order: LV, SH, AF1_AF2, CP, RF)
  extern FEB_TPS_Handle_t tps_handles[NUM_TPS2482];

  // Converted values
  extern int16_t tps2482_current[NUM_TPS2482];
  extern uint16_t tps2482_bus_voltage[NUM_TPS2482];
  extern double tps2482_shunt_voltage[NUM_TPS2482];

  // Raw measurement data (for backward compatibility with CAN transmission)
  // Note: current and shunt voltage are now sign-corrected by the library
  extern int16_t tps2482_shunt_voltage_raw[NUM_TPS2482];

  // Exported arrays for console commands (populated from tps_device_configs)
  extern uint8_t tps2482_i2c_addresses[NUM_TPS2482];
  extern GPIO_TypeDef *tps2482_en_ports[NUM_TPS2482 - 1]; // No EN for LV
  extern uint16_t tps2482_en_pins[NUM_TPS2482 - 1];
  extern GPIO_TypeDef *tps2482_pg_ports[NUM_TPS2482];
  extern uint16_t tps2482_pg_pins[NUM_TPS2482];

  /* ============================================================================
   * API Functions
   * ============================================================================ */

  /**
   * @brief Initialize the TPS library, register every chip, and set the startup rail state
   * @note Must be called after FEB_TPS_Init()'s I2C mutex exists (i.e. after MX_FREERTOS_Init)
   */
  void LVPDB_TPS_Setup(void);

  /**
   * @brief Whether at least one TPS2482 registered successfully
   */
  bool LVPDB_TPS_IsInitialized(void);

  /**
   * @brief Poll every registered chip and refresh the converted measurements
   * @note Takes the TPS data mutex internally
   */
  void LVPDB_TPS_Poll(void);

  void LVPDB_TPS_SetRail(uint8_t index, bool enable);

  /**
   * @brief Whether the last poll of a chip succeeded
   * @param index Device index (0..NUM_TPS2482-1)
   */
  bool LVPDB_TPS_PollOk(uint8_t index);

  /**
   * @brief Whether a chip's last power-good read was asserted
   * @param index Device index (0..NUM_TPS2482-1)
   */
  bool LVPDB_TPS_PowerGood(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* LVPDB_TPS_H */
