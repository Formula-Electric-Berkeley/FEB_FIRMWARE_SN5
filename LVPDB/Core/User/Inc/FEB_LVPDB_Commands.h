/**
 ******************************************************************************
 * @file           : FEB_LVPDB_Commands.h
 * @brief          : LVPDB Custom Console Commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Custom console commands for the LVPDB board. All commands are namespaced
 * under the "LVPDB" prefix.
 *
 * Commands:
 *   - LVPDB              : Show LVPDB command help
 *   - LVPDB|status       : Show all TPS2482 chip status
 *   - LVPDB|enable|<chip>  : Enable a TPS chip by name or index
 *   - LVPDB|disable|<chip> : Disable a TPS chip by name or index
 *   - LVPDB|read|<chip>|<reg>  : Read a TPS register
 *   - LVPDB|write|<chip>|<reg>|<value> : Write a TPS register
 *
 * Chip names (case-insensitive):
 *   LV (0)      - Low Voltage Source (NOT controllable)
 *   SH (1)      - Shutdown Source
 *   LT (2)      - Laptop Branch
 *   BM_L (3)    - Braking Servo, Lidar
 *   SM (4)      - Steering Motor
 *   AF1_AF2 (5) - Accumulator Fans
 *   CP_RF (6)   - Coolant Pump + Radiator Fans
 *
 * Register names: config, shunt, bus, power, current, cal, mask, alert, id
 *
 * Usage:
 *   Call LVPDB_RegisterCommands() after FEB_Console_Init() to add
 *   these custom commands to the console.
 *
 ******************************************************************************
 */

#ifndef FEB_LVPDB_COMMANDS_H
#define FEB_LVPDB_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /* ============================================================================
   * Command Descriptor
   * ============================================================================ */

  extern const FEB_Console_Cmd_t lvpdb_cmd;

  /* ============================================================================
   * Registration Function
   * ============================================================================ */

  /**
   * @brief Register all LVPDB custom commands
   *
   * Call after FEB_Console_Init() to add board-specific commands.
   */
  void LVPDB_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_LVPDB_COMMANDS_H */
