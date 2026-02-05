/**
 ******************************************************************************
 * @file           : FEB_LVPDB_Commands.c
 * @brief          : LVPDB Custom Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_LVPDB_Commands.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_Main.h"
#include "TPS2482.h"
#include "feb_console.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * External Variables from FEB_Main.c
 * ============================================================================ */

extern I2C_HandleTypeDef hi2c1;
extern uint8_t tps2482_i2c_addresses[NUM_TPS2482];
extern GPIO_TypeDef *tps2482_en_ports[NUM_TPS2482 - 1];
extern uint16_t tps2482_en_pins[NUM_TPS2482 - 1];
extern GPIO_TypeDef *tps2482_pg_ports[NUM_TPS2482];
extern uint16_t tps2482_pg_pins[NUM_TPS2482];
extern uint16_t tps2482_bus_voltage[NUM_TPS2482];
extern int16_t tps2482_current[NUM_TPS2482];

/* ============================================================================
 * Chip Name/Index Mapping
 * ============================================================================ */

static const char *chip_names[NUM_TPS2482] = {"LV", "SH", "LT", "BM_L", "SM", "AF1_AF2", "CP_RF"};

/**
 * @brief Case-insensitive string comparison
 */
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

/**
 * @brief Get chip index from name or numeric string
 * @return Chip index (0-6) or -1 if invalid
 */
static int get_chip_index(const char *name)
{
  // Try numeric first
  if (isdigit((unsigned char)name[0]))
  {
    int idx = atoi(name);
    if (idx >= 0 && idx < NUM_TPS2482)
      return idx;
    return -1;
  }

  // Name lookup (case-insensitive)
  for (int i = 0; i < NUM_TPS2482; i++)
  {
    if (strcasecmp_local(name, chip_names[i]) == 0)
      return i;
  }
  return -1;
}

/* ============================================================================
 * Register Name/Address Mapping
 * ============================================================================ */

typedef struct
{
  const char *name;
  uint8_t addr;
  bool writable;
} RegInfo_t;

static const RegInfo_t registers[] = {
    {"config", TPS2482_CONFIG, true}, {"shunt", TPS2482_SHUNT_VOLT, false}, {"bus", TPS2482_BUS_VOLT, false},
    {"power", TPS2482_POWER, false},  {"current", TPS2482_CURRENT, false},  {"cal", TPS2482_CAL, true},
    {"mask", TPS2482_MASK, true},     {"alert", TPS2482_ALERT_LIM, true},   {"id", TPS2482_ID, false},
};

#define NUM_REGISTERS (sizeof(registers) / sizeof(registers[0]))

/**
 * @brief Get register info from name
 * @return Pointer to RegInfo_t or NULL if not found
 */
static const RegInfo_t *get_register_info(const char *name)
{
  for (size_t i = 0; i < NUM_REGISTERS; i++)
  {
    if (strcasecmp_local(name, registers[i].name) == 0)
      return &registers[i];
  }
  return NULL;
}

/* ============================================================================
 * Subcommand Handlers
 * ============================================================================ */

static void print_lvpdb_help(void)
{
  FEB_Console_Printf("LVPDB Commands:\r\n");
  FEB_Console_Printf("  LVPDB|status              - Show all TPS chip status\r\n");
  FEB_Console_Printf("  LVPDB|enable|<chip>       - Enable chip (SH,LT,BM_L,SM,AF1_AF2,CP_RF or 1-6)\r\n");
  FEB_Console_Printf("  LVPDB|disable|<chip>      - Disable chip\r\n");
  FEB_Console_Printf("  LVPDB|read|<chip>|<reg>   - Read register\r\n");
  FEB_Console_Printf("  LVPDB|write|<chip>|<reg>|<val> - Write register\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CAN Ping/Pong:\r\n");
  FEB_Console_Printf("  LVPDB|ping|<ch>           - Start ping mode (TX every 100ms) on channel 1-4\r\n");
  FEB_Console_Printf("  LVPDB|pong|<ch>           - Start pong mode (respond to pings) on channel 1-4\r\n");
  FEB_Console_Printf("  LVPDB|stop|<ch|all>       - Stop channel (1-4) or all\r\n");
  FEB_Console_Printf("  LVPDB|canstatus           - Show CAN ping/pong status\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("Chips: LV(0), SH(1), LT(2), BM_L(3), SM(4), AF1_AF2(5), CP_RF(6)\r\n");
  FEB_Console_Printf("  Note: LV cannot be enabled/disabled (always on)\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("Registers: config, shunt, bus, power, current, cal, mask, alert, id\r\n");
  FEB_Console_Printf("CAN Channels: 1 (0xE0), 2 (0xE1), 3 (0xE2), 4 (0xE3)\r\n");
}

static void cmd_status(void)
{
  GPIO_PinState pg_states[NUM_TPS2482];
  GPIO_PinState en_states[NUM_TPS2482 - 1];

  // Read power-good pins
  TPS2482_GPIO_Read(tps2482_pg_ports, tps2482_pg_pins, pg_states, NUM_TPS2482);

  // Read enable pins (for chips 1-6, EN pin array is 0-5)
  TPS2482_GPIO_Read(tps2482_en_ports, tps2482_en_pins, en_states, NUM_TPS2482 - 1);

  FEB_Console_Printf("TPS2482 Status:\r\n");
  FEB_Console_Printf("%-3s %-8s %-4s %-3s %8s %8s\r\n", "ID", "Name", "EN", "PG", "Vbus(mV)", "I(mA)");
  FEB_Console_Printf("--- -------- ---- --- -------- --------\r\n");

  for (int i = 0; i < NUM_TPS2482; i++)
  {
    const char *en_str;
    if (i == 0)
    {
      en_str = "ON"; // LV is always on
    }
    else
    {
      en_str = en_states[i - 1] ? "ON" : "OFF";
    }

    FEB_Console_Printf("%-3d %-8s %-4s %-3s %8u %8d\r\n", i, chip_names[i], en_str, pg_states[i] ? "OK" : "--",
                       tps2482_bus_voltage[i], tps2482_current[i]);
  }
}

static void cmd_enable(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: LVPDB|enable|<chip>\r\n");
    return;
  }

  int idx = get_chip_index(argv[1]);
  if (idx < 0)
  {
    FEB_Console_Printf("Error: Unknown chip '%s'\r\n", argv[1]);
    return;
  }

  if (idx == 0)
  {
    FEB_Console_Printf("Error: LV cannot be controlled (always on)\r\n");
    return;
  }

  // Enable pin index is chip index - 1
  int en_idx = idx - 1;
  uint8_t en_state = 1;
  bool result;

  TPS2482_Enable(&tps2482_en_ports[en_idx], &tps2482_en_pins[en_idx], &en_state, &result, 1);

  if (result)
  {
    FEB_Console_Printf("%s enabled\r\n", chip_names[idx]);
  }
  else
  {
    FEB_Console_Printf("Warning: %s enable command sent, but readback failed\r\n", chip_names[idx]);
  }
}

static void cmd_disable(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: LVPDB|disable|<chip>\r\n");
    return;
  }

  int idx = get_chip_index(argv[1]);
  if (idx < 0)
  {
    FEB_Console_Printf("Error: Unknown chip '%s'\r\n", argv[1]);
    return;
  }

  if (idx == 0)
  {
    FEB_Console_Printf("Error: LV cannot be controlled (always on)\r\n");
    return;
  }

  // Enable pin index is chip index - 1
  int en_idx = idx - 1;
  uint8_t en_state = 0;
  bool result;

  TPS2482_Enable(&tps2482_en_ports[en_idx], &tps2482_en_pins[en_idx], &en_state, &result, 1);

  if (!result)
  {
    FEB_Console_Printf("%s disabled\r\n", chip_names[idx]);
  }
  else
  {
    FEB_Console_Printf("Warning: %s disable command sent, but readback shows still enabled\r\n", chip_names[idx]);
  }
}

static void cmd_read(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: LVPDB|read|<chip>|<reg>\r\n");
    FEB_Console_Printf("Registers: config, shunt, bus, power, current, cal, mask, alert, id\r\n");
    return;
  }

  int idx = get_chip_index(argv[1]);
  if (idx < 0)
  {
    FEB_Console_Printf("Error: Unknown chip '%s'\r\n", argv[1]);
    return;
  }

  const RegInfo_t *reg = get_register_info(argv[2]);
  if (reg == NULL)
  {
    FEB_Console_Printf("Error: Unknown register '%s'\r\n", argv[2]);
    return;
  }

  uint16_t value;
  TPS2482_Get_Register(&hi2c1, &tps2482_i2c_addresses[idx], reg->addr, &value, 1);

  FEB_Console_Printf("%s %s = 0x%04X (%u)\r\n", chip_names[idx], reg->name, value, value);
}

static void cmd_write(int argc, char *argv[])
{
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: LVPDB|write|<chip>|<reg>|<value>\r\n");
    FEB_Console_Printf("Writable registers: config, cal, mask, alert\r\n");
    return;
  }

  int idx = get_chip_index(argv[1]);
  if (idx < 0)
  {
    FEB_Console_Printf("Error: Unknown chip '%s'\r\n", argv[1]);
    return;
  }

  const RegInfo_t *reg = get_register_info(argv[2]);
  if (reg == NULL)
  {
    FEB_Console_Printf("Error: Unknown register '%s'\r\n", argv[2]);
    return;
  }

  if (!reg->writable)
  {
    FEB_Console_Printf("Error: Register '%s' is read-only\r\n", reg->name);
    return;
  }

  // Parse value (supports hex with 0x prefix)
  char *endptr;
  uint16_t value = (uint16_t)strtoul(argv[3], &endptr, 0);
  if (*endptr != '\0')
  {
    FEB_Console_Printf("Error: Invalid value '%s'\r\n", argv[3]);
    return;
  }

  TPS2482_Write_Register(&hi2c1, &tps2482_i2c_addresses[idx], reg->addr, &value, 1);

  // Read back to verify
  uint16_t readback;
  TPS2482_Get_Register(&hi2c1, &tps2482_i2c_addresses[idx], reg->addr, &readback, 1);

  FEB_Console_Printf("%s %s written: 0x%04X, readback: 0x%04X\r\n", chip_names[idx], reg->name, value, readback);
}

/* ============================================================================
 * CAN Ping/Pong Command Handlers
 * ============================================================================ */

static const char *mode_names[] = {"OFF", "PING", "PONG"};
static const uint32_t pingpong_frame_ids[] = {0xE0, 0xE1, 0xE2, 0xE3};

static void cmd_ping(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: LVPDB|ping|<channel>\r\n");
    FEB_Console_Printf("Channels: 1-4 (Frame IDs 0xE0-0xE3)\r\n");
    return;
  }

  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_Printf("Error: Channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_PING);
  FEB_Console_Printf("Channel %d (0x%02X): PING mode started\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1]);
}

static void cmd_pong(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: LVPDB|pong|<channel>\r\n");
    FEB_Console_Printf("Channels: 1-4 (Frame IDs 0xE0-0xE3)\r\n");
    return;
  }

  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_Printf("Error: Channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_PONG);
  FEB_Console_Printf("Channel %d (0x%02X): PONG mode started\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1]);
}

static void cmd_stop(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: LVPDB|stop|<channel|all>\r\n");
    return;
  }

  if (strcasecmp_local(argv[1], "all") == 0)
  {
    FEB_CAN_PingPong_Reset();
    FEB_Console_Printf("All channels stopped\r\n");
    return;
  }

  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_Printf("Error: Channel must be 1-4 or 'all'\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_OFF);
  FEB_Console_Printf("Channel %d stopped\r\n", ch);
}

static void cmd_canstatus(void)
{
  FEB_Console_Printf("CAN Ping/Pong Status:\r\n");
  FEB_Console_Printf("%-3s %-6s %-5s %10s %10s %12s\r\n", "Ch", "FrameID", "Mode", "TX Count", "RX Count", "Last RX");
  FEB_Console_Printf("--- ------ ----- ---------- ---------- ------------\r\n");

  for (int ch = 1; ch <= 4; ch++)
  {
    FEB_PingPong_Mode_t mode = FEB_CAN_PingPong_GetMode((uint8_t)ch);
    uint32_t tx_count = FEB_CAN_PingPong_GetTxCount((uint8_t)ch);
    uint32_t rx_count = FEB_CAN_PingPong_GetRxCount((uint8_t)ch);
    int32_t last_rx = FEB_CAN_PingPong_GetLastCounter((uint8_t)ch);

    FEB_Console_Printf("%-3d 0x%02X   %-5s %10u %10u %12d\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1],
                       mode_names[mode], (unsigned int)tx_count, (unsigned int)rx_count, (int)last_rx);
  }
}

/* ============================================================================
 * Main Command Handler
 * ============================================================================ */

static void cmd_lvpdb(int argc, char *argv[])
{
  if (argc < 2)
  {
    // No subcommand = show LVPDB help
    print_lvpdb_help();
    return;
  }

  const char *subcmd = argv[1];

  if (strcasecmp_local(subcmd, "status") == 0)
  {
    cmd_status();
  }
  else if (strcasecmp_local(subcmd, "enable") == 0)
  {
    cmd_enable(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "disable") == 0)
  {
    cmd_disable(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "read") == 0)
  {
    cmd_read(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "write") == 0)
  {
    cmd_write(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "ping") == 0)
  {
    cmd_ping(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "pong") == 0)
  {
    cmd_pong(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "stop") == 0)
  {
    cmd_stop(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "canstatus") == 0)
  {
    cmd_canstatus();
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_lvpdb_help();
  }
}

/* ============================================================================
 * Command Descriptor
 * ============================================================================ */

const FEB_Console_Cmd_t lvpdb_cmd = {
    .name = "LVPDB",
    .help = "LVPDB board commands (LVPDB|status, LVPDB|enable, etc.)",
    .handler = cmd_lvpdb,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void LVPDB_RegisterCommands(void)
{
  FEB_Console_Register(&lvpdb_cmd);
}
