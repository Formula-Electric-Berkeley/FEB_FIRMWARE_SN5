/**
 ******************************************************************************
 * @file           : FEB_PCU_APPS_Commands.h
 * @brief          : PCU APPS / faults console subcommand tree
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_PCU_APPS_COMMANDS_H
#define FEB_PCU_APPS_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* Register top-level CSV-addressable commands (apps, faults). The
   * FEB_PCU_Commands.c text-mode dispatcher routes PCU|apps|<sub> and
   * PCU|faults|<sub> through the Handle* helpers below for the human
   * pipe-delimited form. */
  void PCU_APPS_RegisterCommands(void);

  /* Drive periodic APPS streaming from the main loop. Cheap no-op when
   * streaming is disabled. */
  void PCU_APPS_StreamProcess(void);

  /* Sub-dispatchers invoked from FEB_PCU_Commands.c when the user types
   * `PCU|apps|...` or `PCU|faults|...`. argv[0] is the entry (apps/faults). */
  void PCU_APPS_HandleAppsSubcommand(int argc, char *argv[]);
  void PCU_APPS_HandleFaultsSubcommand(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* FEB_PCU_APPS_COMMANDS_H */
