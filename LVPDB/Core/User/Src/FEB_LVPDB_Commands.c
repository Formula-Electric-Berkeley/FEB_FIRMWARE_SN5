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
#include "feb_console.h"
#include "feb_string_utils.h"
#include "feb_tps.h"
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
 * Resolve a TPS2482 chip index from a name or a numeric string.
 *
 * @param name Chip identifier as either a numeric string ("0".."6") or a case-insensitive chip name.
 * @returns Index in the range 0..NUM_TPS2482-1 if the identifier is valid, `-1` otherwise.
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
    if (FEB_strcasecmp(name, chip_names[i]) == 0)
      return i;
  }
  return -1;
}

/* ============================================================================
 * Register Name/Address Mapping (using FEB_TPS_Library constants)
 * ============================================================================ */

typedef struct
{
  const char *name;
  uint8_t addr;
  bool writable;
} RegInfo_t;

static const RegInfo_t registers[] = {
    {"config", FEB_TPS_REG_CONFIG, true},    {"shunt", FEB_TPS_REG_SHUNT_VOLT, false},
    {"bus", FEB_TPS_REG_BUS_VOLT, false},    {"power", FEB_TPS_REG_POWER, false},
    {"current", FEB_TPS_REG_CURRENT, false}, {"cal", FEB_TPS_REG_CAL, true},
    {"mask", FEB_TPS_REG_MASK, true},        {"alert", FEB_TPS_REG_ALERT_LIM, true},
    {"id", FEB_TPS_REG_ID, false},
};

#define NUM_REGISTERS (sizeof(registers) / sizeof(registers[0]))

/**
 * Find register metadata by name.
 *
 * Performs a case-insensitive lookup of the register table for the given name.
 * @param name Register name to look up (case-insensitive).
 * @returns Pointer to the matching RegInfo_t, or NULL if no register with that name exists.
 */
static const RegInfo_t *get_register_info(const char *name)
{
  for (size_t i = 0; i < NUM_REGISTERS; i++)
  {
    if (FEB_strcasecmp(name, registers[i].name) == 0)
      return &registers[i];
  }
  return NULL;
}

/* ============================================================================
 * I2C Helper Functions (for direct register access in debug commands)
 * ============================================================================ */

/**
 * Read a 16-bit TPS2482 register over I2C and store its MSB-first value.
 *
 * Reads two bytes from the device at `i2c_addr` starting at register `reg` and,
 * on success, writes the assembled 16-bit value (MSB first) into `*value`.
 *
 * @param i2c_addr 7-bit I2C address of the TPS2482 device.
 * @param reg      8-bit register address to read.
 * @param value    Pointer to storage for the 16-bit register value; updated only on success.
 * @returns HAL_OK if the read succeeded and `*value` was updated, otherwise the HAL error status.
 */
static HAL_StatusTypeDef tps_read_reg(uint8_t i2c_addr, uint8_t reg, uint16_t *value)
{
  uint8_t buf[2];
  HAL_StatusTypeDef status =
      HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(i2c_addr << 1), reg, I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
  if (status == HAL_OK)
  {
    *value = ((uint16_t)buf[0] << 8) | buf[1]; // TPS2482 sends MSB first
  }
  return status;
}

/**
 * Write a 16-bit value to a TPS2482 register over I2C.
 * @param i2c_addr 7-bit I2C address of the TPS2482 device.
 * @param reg 8-bit TPS2482 register address.
 * @param value 16-bit value to write to the register.
 * @returns HAL_StatusTypeDef HAL status: `HAL_OK` on success, otherwise an error code.
 */
static HAL_StatusTypeDef tps_write_reg(uint8_t i2c_addr, uint8_t reg, uint16_t value)
{
  uint8_t buf[2];
  buf[0] = (uint8_t)(value >> 8); // MSB first
  buf[1] = (uint8_t)(value & 0xFF);
  return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(i2c_addr << 1), reg, I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
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

/**
 * Print TPS2482 chips status to the console.
 *
 * Reads each chip's power-good and enable states and prints a table showing
 * ID, Name, EN, PG, Vbus(mV), and I(mA). The LV chip (ID 0) is always reported as ON.
 */
static void cmd_status(void)
{
  GPIO_PinState pg_states[NUM_TPS2482];
  GPIO_PinState en_states[NUM_TPS2482 - 1];

  // Read power-good pins
  for (int i = 0; i < NUM_TPS2482; i++)
  {
    pg_states[i] = HAL_GPIO_ReadPin(tps2482_pg_ports[i], tps2482_pg_pins[i]);
  }

  // Read enable pins (for chips 1-6, EN pin array is 0-5)
  for (int i = 0; i < NUM_TPS2482 - 1; i++)
  {
    en_states[i] = HAL_GPIO_ReadPin(tps2482_en_ports[i], tps2482_en_pins[i]);
  }

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

/**
 * Enable a specified TPS2482 chip by name or numeric index and report the outcome.
 *
 * Expects a chip identifier in argv[1]; if missing prints usage. Resolves the chip index,
 * rejects attempts to control the LV (index 0) and prints an error for unknown chips.
 * For controllable chips, asserts the corresponding enable GPIO and reads it back:
 * on successful readback prints "<Name> enabled", otherwise prints a warning that
 * the enable command was sent but readback failed.
 *
 * @param argc Number of arguments (expects at least 2).
 * @param argv Argument vector where argv[1] is the chip name or numeric index.
 */
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

  HAL_GPIO_WritePin(tps2482_en_ports[en_idx], tps2482_en_pins[en_idx], GPIO_PIN_SET);

  // Read back to verify
  GPIO_PinState state = HAL_GPIO_ReadPin(tps2482_en_ports[en_idx], tps2482_en_pins[en_idx]);
  if (state == GPIO_PIN_SET)
  {
    FEB_Console_Printf("%s enabled\r\n", chip_names[idx]);
  }
  else
  {
    FEB_Console_Printf("Warning: %s enable command sent, but readback failed\r\n", chip_names[idx]);
  }
}

/**
 * Disable the specified TPS2482 chip (except LV) and report the result.
 *
 * Writes the chip's enable GPIO low, verifies the pin state by readback,
 * and prints a confirmation message on success or a warning if the readback
 * still shows the chip enabled. If the chip identifier is invalid or refers
 * to the LV rail (index 0), an error message is printed instead.
 *
 * @param argc Number of arguments; expects at least 2 (subcommand + chip).
 * @param argv Argument vector where argv[1] is the chip name or numeric index.
 */
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

  HAL_GPIO_WritePin(tps2482_en_ports[en_idx], tps2482_en_pins[en_idx], GPIO_PIN_RESET);

  // Read back to verify
  GPIO_PinState state = HAL_GPIO_ReadPin(tps2482_en_ports[en_idx], tps2482_en_pins[en_idx]);
  if (state == GPIO_PIN_RESET)
  {
    FEB_Console_Printf("%s disabled\r\n", chip_names[idx]);
  }
  else
  {
    FEB_Console_Printf("Warning: %s disable command sent, but readback shows still enabled\r\n", chip_names[idx]);
  }
}

/**
 * Read a TPS register for a specified chip and print the result to the console.
 *
 * Expects two arguments after the command: a chip identifier (index or name) and a
 * register name. On success prints "<Chip> <Register> = 0xXXXX (decimal)". On error
 * prints usage when arguments are missing, an "Unknown chip" or "Unknown register"
 * message for invalid inputs, or an "I2C read failed" message if the device read fails.
 *
 * @param argc Number of arguments (including subcommand). Must be at least 3.
 * @param argv Argument vector where argv[1] is the chip (index or name) and argv[2] is
 *             the register name (one of: config, shunt, bus, power, current, cal,
 *             mask, alert, id).
 */
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
  HAL_StatusTypeDef status = tps_read_reg(tps2482_i2c_addresses[idx], reg->addr, &value);

  if (status == HAL_OK)
  {
    FEB_Console_Printf("%s %s = 0x%04X (%u)\r\n", chip_names[idx], reg->name, value, value);
  }
  else
  {
    FEB_Console_Printf("Error: I2C read failed (status=%d)\r\n", status);
  }
}

/**
 * Write a 16-bit value to a TPS2482 register for a specified chip and verify by readback.
 *
 * Parses arguments as: argv[1]=chip (name or index), argv[2]=register name, argv[3]=value (supports 0x hex).
 * Validates the chip and register (rejects read-only registers), performs an I2C write of the 16-bit value,
 * then attempts an I2C readback and prints success or error messages to the console.
 *
 * @param argc Number of arguments; must be at least 4 (command, chip, register, value).
 * @param argv Argument vector where argv[1] is the chip identifier, argv[2] is the register name,
 *             and argv[3] is the value to write.
 */
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

  HAL_StatusTypeDef status = tps_write_reg(tps2482_i2c_addresses[idx], reg->addr, value);
  if (status != HAL_OK)
  {
    FEB_Console_Printf("Error: I2C write failed (status=%d)\r\n", status);
    return;
  }

  // Read back to verify
  uint16_t readback;
  status = tps_read_reg(tps2482_i2c_addresses[idx], reg->addr, &readback);
  if (status == HAL_OK)
  {
    FEB_Console_Printf("%s %s written: 0x%04X, readback: 0x%04X\r\n", chip_names[idx], reg->name, value, readback);
  }
  else
  {
    FEB_Console_Printf("%s %s written: 0x%04X (readback failed)\r\n", chip_names[idx], reg->name, value);
  }
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

  if (FEB_strcasecmp(argv[1], "all") == 0)
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

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    cmd_status();
  }
  else if (FEB_strcasecmp(subcmd, "enable") == 0)
  {
    cmd_enable(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "disable") == 0)
  {
    cmd_disable(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "read") == 0)
  {
    cmd_read(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "write") == 0)
  {
    cmd_write(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "ping") == 0)
  {
    cmd_ping(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "pong") == 0)
  {
    cmd_pong(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "stop") == 0)
  {
    cmd_stop(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "canstatus") == 0)
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
