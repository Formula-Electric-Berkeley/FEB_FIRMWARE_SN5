/**
 ******************************************************************************
 * @file           : FEB_Commands.cpp
 * @brief          : Console commands for the BMS (pack, state machine, ADBMS)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Commands.h"

#include "ADBMS6830B_Registers.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_CAN_Charger.h"
#include "FEB_CAN_IVT.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_State.h"
#include "FEB_Const.h"
#include "FEB_HW_Relay.h"
#include "FEB_SM.h"
#include "feb_commands_2.hpp"
#include "feb_console_2.hpp"
#include "feb_log.h"
#include "main.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace feb::console;

#define TAG_BMS "[BMS]"

extern "C"
{
  // Task handles from the CubeMX-generated freertos.c, for `bms tasks`.
  extern osThreadId_t uartRxTaskHandle;
  extern osThreadId_t ADBMSTaskHandle;
  extern osThreadId_t TPSTaskHandle;
  extern osThreadId_t BMSTaskRxHandle;
  extern osThreadId_t BMSTaskTxHandle;
}

namespace
{

const char *isospi_mode_name()
{
  switch (ISOSPI_MODE)
  {
  case ISOSPI_MODE_REDUNDANT:
    return "REDUNDANT";
  case ISOSPI_MODE_SPI1_ONLY:
    return "SPI1_ONLY";
  case ISOSPI_MODE_SPI2_ONLY:
    return "SPI2_ONLY";
  default:
    return "UNKNOWN";
  }
}

const char *state_name(BMS_State_t state)
{
  return FEB_CAN_State_GetStateName(state);
}

// MARK: Pack data

struct CellExtremes
{
  float min_c = 999.0f;
  float max_c = 0.0f;
  float min_s = 999.0f;
  float max_s = 0.0f;
};

CellExtremes cell_extremes()
{
  CellExtremes e;
  for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (std::uint16_t cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      const float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
      const float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
      if (v_c > 0)
      {
        e.min_c = (v_c < e.min_c) ? v_c : e.min_c;
        e.max_c = (v_c > e.max_c) ? v_c : e.max_c;
      }
      if (v_s > 0)
      {
        e.min_s = (v_s < e.min_s) ? v_s : e.min_s;
        e.max_s = (v_s > e.max_s) ? v_s : e.max_s;
      }
    }
  }
  return e;
}

void cmd_bms_status(Interaction &io, std::span<char *const>)
{
  const CellExtremes e = cell_extremes();

  KVTable t(io, 16, 28, "BMS Status");

  t.row("State", "%s", state_name(FEB_SM_Get_Current_State()));
  t.row("Limits profile", "%s",
        FEB_ADBMS_Get_Validation_Profile() == FEB_VALIDATION_PROFILE_CHARGING ? "CHARGING" : "NORMAL");
  t.row("Pack voltage", "%.2f V", (double)FEB_ADBMS_GET_ACC_Total_Voltage());
  t.row("Min cell (C/S)", "%.3f / %.3f V", (double)e.min_c, (double)e.min_s);
  t.row("Max cell (C/S)", "%.3f / %.3f V", (double)e.max_c, (double)e.max_s);
  t.row("Temps min/max", "%.1f / %.1f C", (double)FEB_ADBMS_GET_ACC_MIN_Temp(), (double)FEB_ADBMS_GET_ACC_MAX_Temp());
  t.row("Temps avg", "%.1f C", (double)FEB_ADBMS_GET_ACC_AVG_Temp());
  t.row("Balancing", "%s", FEB_Cell_Balancing_Status() ? "ON" : "off");
  t.row("Cell delta", "%.0f mV", (double)FEB_ADBMS_GET_Cell_Voltage_Delta_mV());
  t.row("Balance done", "%s", FEB_Cell_Balance_Complete() ? "YES" : "no");
  t.row("Error type", "0x%02X", FEB_ADBMS_Get_Error_Type());
}

void cmd_bms_cells(Interaction &io, std::span<char *const>)
{
  {
    static constexpr Column kCols[] = {{"Cell", 8}, {"C (V)", 8}, {"S (V)", 8}, {"Bal", 4}};
    Table t(io, kCols, "Cell Voltages");

    for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
    {
      for (std::uint16_t cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
      {
        const float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
        const float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
        const std::uint8_t bal = FEB_ADBMS_GET_Cell_Discharging(bank, cell);

        t.cell("B%u C%02u", (unsigned)(bank + 1), (unsigned)(cell + 1));
        /* NaN in both means the register group has failed PEC since boot. */
        if (std::isnan(v_c) && std::isnan(v_s))
        {
          t.cell("--");
          t.cell("--");
        }
        else
        {
          t.cell("%.3f", (double)v_c);
          t.cell("%.3f", (double)v_s);
        }
        t.cell("%s", bal ? "*" : "");
        t.end_row();
      }
    }
  }

  io.println("Balancing: %u cells active | delta %.0f mV | done: %s", (unsigned)FEB_ADBMS_GET_Balancing_Cell_Count(),
             (double)FEB_ADBMS_GET_Cell_Voltage_Delta_mV(), FEB_Cell_Balance_Complete() ? "YES" : "no");
}

void cmd_bms_temps(Interaction &io, std::span<char *const>)
{
  for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    io.print("Bank %2u:", (unsigned)(bank + 1));
    for (std::uint16_t sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      io.print(" %.1f", (double)FEB_ADBMS_GET_Cell_Temperature(bank, sensor));
    }
    io.print("\r\n");
  }
  io.println("Pack: min %.1f C  max %.1f C  avg %.1f C", (double)FEB_ADBMS_GET_ACC_MIN_Temp(),
             (double)FEB_ADBMS_GET_ACC_MAX_Temp(), (double)FEB_ADBMS_GET_ACC_AVG_Temp());
}

void cmd_bms_therm_raw(Interaction &io, std::span<char *const>)
{
  io.println("Thermistor raw voltages (mV)");
  for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      io.println("Bank %u IC %d", (unsigned)(bank + 1), ic + 1);
      for (int mux = 0; mux < 6; mux++)
      {
        io.print("  MUX%d ch0..6:", mux + 1);
        for (int ch = 0; ch < 7; ch++)
        {
          const std::uint16_t sensor = (std::uint16_t)(ic * FEB_NUM_TEMP_SENSE_PER_IC + mux * 7 + ch);
          io.print(" %7.1f", (double)FEB_ADBMS_GET_Therm_Raw_mV(bank, sensor));
        }
        io.print("\r\n");
      }
    }
  }

  io.println("Thermistor raw codes (hex)");
  for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      io.println("Bank %u IC %d", (unsigned)(bank + 1), ic + 1);
      for (int mux = 0; mux < 6; mux++)
      {
        io.print("  MUX%d ch0..6:", mux + 1);
        for (int ch = 0; ch < 7; ch++)
        {
          const std::uint16_t sensor = (std::uint16_t)(ic * FEB_NUM_TEMP_SENSE_PER_IC + mux * 7 + ch);
          io.print(" 0x%04X", FEB_ADBMS_GET_Therm_Raw_Code(bank, sensor));
        }
        io.print("\r\n");
      }
    }
  }
  io.println("Note: 0xFFFF / NaN = PEC failure on that aux register");
}

void cmd_bms_cell(Interaction &io, std::span<char *const> args)
{
  long bank = 0;
  long cell = 0;
  if (!io.arg_int(args, 1, &bank, 1, FEB_NBANKS) || !io.arg_int(args, 2, &cell, 1, FEB_NUM_CELLS_PER_BANK))
  {
    return;
  }

  const std::uint8_t bank_idx = (std::uint8_t)(bank - 1);
  const std::uint16_t cell_idx = (std::uint16_t)(cell - 1);
  const float voltage_c = FEB_ADBMS_GET_Cell_Voltage(bank_idx, cell_idx);
  const float voltage_s = FEB_ADBMS_GET_Cell_Voltage_S(bank_idx, cell_idx);

  KVTable t(io, 16, 20, "Cell");

  t.row("Bank / cell", "%ld / %ld", bank, cell);
  t.row("Voltage (C)", "%.3f V", (double)voltage_c);
  t.row("Voltage (S)", "%.3f V", (double)voltage_s);
  t.row("Delta", "%.4f V", (double)(voltage_c - voltage_s));
  t.row("Temperature", "%.1f C", (double)FEB_ADBMS_GET_Cell_Temperature(bank_idx, cell_idx));
  t.row("Violations", "%u", (unsigned)FEB_ADBMS_GET_Cell_Violations(bank_idx, cell_idx));
  t.row("Balancing", "%s", FEB_ADBMS_GET_Cell_Discharging(bank_idx, cell_idx) ? "yes" : "no");
}

void cmd_bms_cell_stats(Interaction &io, std::span<char *const>)
{
  if (!io.is_csv())
  {
    io.error("error", "csv_only", "%s emits csv rows only — use cells and temps", io.path());
    return;
  }

  for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (std::uint16_t cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      io.emit("voltage", "%u,%u,%.3f,%.3f,%u", (unsigned)(bank + 1), (unsigned)(cell + 1),
              (double)FEB_ADBMS_GET_Cell_Voltage(bank, cell), (double)FEB_ADBMS_GET_Cell_Voltage_S(bank, cell),
              (unsigned)FEB_ADBMS_GET_Cell_Discharging(bank, cell));
    }
  }
  for (std::uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (std::uint16_t sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      io.emit("temp", "%u,%u,%.1f", (unsigned)(bank + 1), (unsigned)(sensor + 1),
              (double)FEB_ADBMS_GET_Cell_Temperature(bank, sensor));
    }
  }
}

void cmd_bms_volts(Interaction &io, std::span<char *const>)
{
  ADBMS_SendCmd(0x0468);
  osDelay(pdMS_TO_TICKS(1)); // let the conversion settle

  static constexpr Column kCols[] = {{"IC", 4}, {"VREF2 (V)", 10}, {"VA (V)", 10}, {"VD (V)", 10}, {"ITMP (C)", 10}};
  Table t(io, kCols, "ADBMS6830 Supply Voltages");

  for (std::uint8_t ic = 0; ic < FEB_NUM_IC; ic++)
  {
    ADBMS_STATA_t a = {};
    ADBMS_STATB_t b = {};
    ADBMS_ReadReg(RDSTATA, ic, a.raw);
    ADBMS_ReadReg(RDSTATB, ic, b.raw);

    t.cell("%u", (unsigned)ic);
    t.cell("%.3f", (double)(ADBMS_CodeToVoltage_mV(a.values.VREF2) / 1000.0f));
    t.cell("%.3f", (double)(ADBMS_CodeToVoltage_mV(a.values.VA) / 1000.0f));
    t.cell("%.3f", (double)(ADBMS_CodeToVoltage_mV(b.bits.VD) / 1000.0f));
    t.cell("%.1f", (double)ADBMS_CodeToTemp_C(a.values.ITMP));
    t.end_row();
  }
}

// MARK: State Machine

struct StateEntry
{
  const char *name;
  const char *description;
  BMS_State_t state;
};

constexpr std::array<StateEntry, 14> kStateTable = {{
    {"boot", "Transition to BOOT", BMS_STATE_BOOT},
    {"lv_power", "Transition to LV_POWER", BMS_STATE_LV_POWER},
    {"bus_health", "Transition to BUS_HEALTH_CHECK", BMS_STATE_BUS_HEALTH_CHECK},
    {"precharge", "Transition to PRECHARGE", BMS_STATE_PRECHARGE},
    {"energized", "Transition to ENERGIZED", BMS_STATE_ENERGIZED},
    {"drive", "Transition to DRIVE", BMS_STATE_DRIVE},
    {"battery_free", "Transition to BATTERY_FREE", BMS_STATE_BATTERY_FREE},
    {"charger_precharge", "Transition to CHARGER_PRECHARGE", BMS_STATE_CHARGER_PRECHARGE},
    {"charging", "Transition to CHARGING", BMS_STATE_CHARGING},
    {"balance", "Transition to BALANCE", BMS_STATE_BALANCE},
    {"fault_bms", "Latch FAULT_BMS", BMS_STATE_FAULT_BMS},
    {"fault_bspd", "Latch FAULT_BSPD", BMS_STATE_FAULT_BSPD},
    {"fault_imd", "Latch FAULT_IMD", BMS_STATE_FAULT_IMD},
    {"fault_charging", "Latch FAULT_CHARGING", BMS_STATE_FAULT_CHARGING},
}};

/** @return nullptr when @p name is not a state. */
constexpr const StateEntry *state_entry(const char *name)
{
  for (const StateEntry &e : kStateTable)
  {
    if (iequal(e.name, name))
    {
      return &e;
    }
  }
  return nullptr;
}

bool is_manual_hv_step(BMS_State_t current, BMS_State_t target)
{
  return (current == BMS_STATE_LV_POWER && (target == BMS_STATE_BUS_HEALTH_CHECK || target == BMS_STATE_PRECHARGE)) ||
         (current == BMS_STATE_BUS_HEALTH_CHECK && target == BMS_STATE_PRECHARGE) ||
         (current == BMS_STATE_ENERGIZED && target == BMS_STATE_LV_POWER);
}

bool is_state_transition_allowed(BMS_State_t current, BMS_State_t target)
{
  if (target >= BMS_STATE_FAULT_BMS && target <= BMS_STATE_FAULT_CHARGING)
  {
    return true;
  }
  if ((current == BMS_STATE_ENERGIZED && target == BMS_STATE_DRIVE) ||
      (current == BMS_STATE_DRIVE && target == BMS_STATE_ENERGIZED))
  {
    return true;
  }
  if (is_manual_hv_step(current, target))
  {
    return true;
  }
  if (target == BMS_STATE_BATTERY_FREE)
  {
    return current == BMS_STATE_LV_POWER || current == BMS_STATE_BUS_HEALTH_CHECK;
  }
  return false;
}

void list_state_options(Interaction &io, BMS_State_t current)
{
  for (const StateEntry &e : kStateTable)
  {
    if (e.state == current || !is_state_transition_allowed(current, e.state))
    {
      continue;
    }
    char command[64];
    std::snprintf(command, sizeof(command), "%s %s", io.path(), e.name);
    io.option(e.name, command, e.description);
  }
}

void cmd_bms_state(Interaction &io, std::span<char *const> args)
{
  const BMS_State_t current = FEB_SM_Get_Current_State();

  if (args.size() < 2)
  {
    io.flags("read_only");
    io.println("BMS state: %s (%d)", state_name(current), (int)current);
    list_state_options(io, current);
    return;
  }

  const StateEntry *entry = state_entry(args[1]);
  if (entry == nullptr)
  {
    io.error("error", "unknown_state", "%s", args[1]);
    list_state_options(io, current);
    return;
  }

  if (!is_state_transition_allowed(current, entry->state))
  {
    LOG_W(TAG_BMS, "Console transition refused: %s -> %s", state_name(current), state_name(entry->state));
    io.error("error", "transition_not_allowed", "%s -> %s", state_name(current), state_name(entry->state));
    list_state_options(io, current);
    return;
  }

  if (is_manual_hv_step(current, entry->state))
  {
    LOG_W(TAG_BMS, "Manual transition override: %s -> %s", state_name(current), state_name(entry->state));
  }

  FEB_SM_Transition(entry->state);
  io.println("State transition requested: %s -> %s", state_name(current), state_name(entry->state));
}

// MARK: Balancing

// Balancing is only safe when the car is not in motion and not energized for
// driving: BATTERY_FREE (accumulator isolated) or the explicit BALANCE state.
bool is_balancing_allowed()
{
  const BMS_State_t state = FEB_SM_Get_Current_State();
  return (state == BMS_STATE_BATTERY_FREE || state == BMS_STATE_BALANCE);
}

void cmd_bms_balance(Interaction &io, std::span<char *const>)
{
  io.println("Balancing: %s", FEB_Cell_Balancing_Status() ? "ON" : "off");
  io.println("State: %s (balancing needs BATTERY_FREE or BALANCE)", state_name(FEB_SM_Get_Current_State()));
}

void cmd_bms_balance_on(Interaction &io, std::span<char *const>)
{
  if (!is_balancing_allowed())
  {
    io.error("error", "not_allowed", "%s", state_name(FEB_SM_Get_Current_State()));
    return;
  }

  /* Enter BALANCE first so no balance pass ever runs in BATTERY_FREE. */
  if (FEB_SM_Get_Current_State() == BMS_STATE_BATTERY_FREE)
  {
    FEB_SM_Transition(BMS_STATE_BALANCE); /* 6->9 begin_balance */
  }
  if (FEB_SM_Get_Current_State() != BMS_STATE_BALANCE)
  {
    io.error("error", "enter_balance_failed", "%s", state_name(FEB_SM_Get_Current_State()));
    return;
  }

  FEB_Cell_Balance_Start();
  io.println("Balancing started");
}

void cmd_bms_balance_off(Interaction &io, std::span<char *const>)
{
  FEB_Stop_Balance();
  if (FEB_SM_Get_Current_State() == BMS_STATE_BALANCE)
  {
    FEB_SM_Transition(BMS_STATE_BATTERY_FREE); /* 8->6 stop_balance */
  }
  io.println("Balancing stopped");
}

constexpr std::array<Command, 2> kBalanceSubcommands = {{
    {.name = "on", .description = "Start balancing", .handler = cmd_bms_balance_on},
    {.name = "off", .description = "Stop balancing", .handler = cmd_bms_balance_off},
}};

// MARK: Hardware and peripherals

void cmd_bms_gpio(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 18, 18, "GPIO");

  t.row("AIR+ sense", "%s", FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  t.row("AIR- sense", "%s", FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  t.row("Precharge sense", "%s", FEB_HW_Precharge_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  t.row("Shutdown loop", "%s", FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  t.row("IMD status", "%s",
        FEB_HW_IMD_Sense() == FEB_RELAY_STATE_CLOSE ? "OK (armed)"
                                                    : (FEB_SM_IMD_Armed() ? "FAULT" : "LOW (not armed)"));
  t.row("Reset button", "%s", FEB_HW_Reset_Button_Pressed() ? "PRESSED" : "NOT_PRESSED");
  t.row("BMS SHDN (PC1)", "%s", FEB_HW_BMS_Shutdown_Get() ? "CLOSED" : "OPEN");
  t.row("BMS IND (PC0)", "%s", FEB_HW_BMS_Indicator_Get() ? "ON" : "off");
  t.row("TSMS indicator", "%s", FEB_HW_TSMS_Indicator_Get() ? "ON" : "off");
  t.row("HV safe", "%s", FEB_HW_Is_HV_Safe() ? "YES" : "no");
}

void cmd_bms_charger(Interaction &io, std::span<char *const>)
{
  FEB_Charger_Snapshot_t s;
  FEB_CAN_Charger_GetSnapshot(&s);

  {
    /* Charger -> BMS (Charger_Status, 0x18FF50E5). */
    KVTable t(io, 16, 26, "Charger -> BMS");

    if (!s.ever_seen)
    {
      t.row("Link", "no charger frames on bus");
    }
    else
    {
      t.row("Link", "%s (age %lu ms, rx %lu)", s.present ? "PRESENT" : "TIMEOUT", (unsigned long)s.age_ms,
            (unsigned long)s.rx_count);
      t.row("Output V", "%.1f V", (double)(s.op_voltage_dV / 10.0f));
      t.row("Output I", "%.1f A", (double)(s.op_current_dA / 10.0f));
      t.row("HW status", "%s", s.hw_status ? "FAIL" : "OK");
      t.row("Temperature", "%s", s.temperature ? "FAULT" : "OK");
      t.row("Input voltage", "%s", s.input_voltage ? "FAULT" : "OK");
      t.row("Charger state", "%s", s.state ? "OFF" : "CHARGING");
      t.row("Comm state", "%s", s.communication_state ? "TIMEOUT" : "OK");
    }
  }

  /* BMS -> charger (Charger_Limits, 0x1806E5F4). */
  KVTable t(io, 16, 26, "BMS -> Charger");

  t.row("SM state", "%s", state_name(FEB_SM_Get_Current_State()));
  t.row("Target V", "%.1f V", (double)(s.cmd_voltage_dV / 10.0f));
  t.row("Max I", "%.1f A", (double)(s.cmd_current_dA / 10.0f));
  t.row("Control", "%s", s.control ? "STOP" : "START");
  t.row("Trickle", "%s%s", s.trickle_active ? "ACTIVE" : "off",
        s.trickle_active ? (s.trickle_on ? " (on)" : " (rest)") : "");
  t.row("Done charging", "%s", s.done_charging ? "YES" : "no");
}

void cmd_bms_ivt(Interaction &io, std::span<char *const>)
{
  const FEB_CAN_IVT_Data_t *ivt = FEB_CAN_IVT_GetData();
  const std::uint32_t age = HAL_GetTick() - ivt->last_rx_tick;

  KVTable t(io, 16, 22, "IVT Sensor");

  t.row("Pack current", "%.2f A", (double)(ivt->current_mA / 1000.0f));
  t.row("Voltage 1", "%.2f V", (double)(ivt->voltage_1_mV / 1000.0f));
  t.row("Voltage 2", "%.2f V", (double)(ivt->voltage_2_mV / 1000.0f));
  t.row("Voltage 3", "%.2f V", (double)(ivt->voltage_3_mV / 1000.0f));
  t.row("Pack voltage", "%.2f V (U%d)", (double)FEB_CAN_IVT_GetVoltage(), FEB_IVT_PACK_VOLTAGE_CHANNEL);
  t.row("Temperature", "%.1f C", (double)ivt->temperature_C);
  t.row("Data age", "%lu ms (%s)", (unsigned long)age, FEB_CAN_IVT_IsDataFresh(1000) ? "FRESH" : "STALE");
}

void cmd_bms_spi(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 18, 18, "isoSPI");

  t.row("Mode", "%s", isospi_mode_name());
  t.row("Primary channel", "SPI%d", ISOSPI_PRIMARY_CHANNEL);
  t.row("Failover thresh", "%d PEC errors", ISOSPI_FAILOVER_PEC_THRESHOLD);
}

const char *err_type_name(std::uint8_t err)
{
  switch (err)
  {
  case 0x00:
    return "None";
  case ERROR_TYPE_TEMP_VIOLATION:
    return "Temperature Violation";
  case ERROR_TYPE_LOW_TEMP_READS:
    return "Low Temp Reads";
  case ERROR_TYPE_VOLTAGE_VIOLATION:
    return "Voltage Violation";
  case ERROR_TYPE_INIT_FAILURE:
    return "Init Failure";
  default:
    return "Unknown";
  }
}

void cmd_bms_errors(Interaction &io, std::span<char *const>)
{
  const std::uint8_t err = FEB_ADBMS_Get_Error_Type();

  KVTable t(io, 14, 26, "Errors");

  t.row("Error type", "0x%02X (%s)", err, err_type_name(err));
  t.row("State", "%s", state_name(FEB_SM_Get_Current_State()));
  t.row("Faulted", "%s", FEB_SM_Is_Faulted() ? "YES" : "no");
  t.row("HV active", "%s", FEB_SM_Is_HV_Active() ? "YES" : "no");
}

void cmd_bms_config(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 18, 24, "Configuration");

  t.row("Banks", "%d", FEB_NBANKS);
  t.row("ICs per bank", "%d", FEB_NUM_ICPBANK);
  t.row("Cells per bank", "%d", FEB_NUM_CELLS_PER_BANK);
  t.row("Temp sensors", "%d", FEB_NUM_TEMP_SENSORS);
  t.row("Total cells", "%d", FEB_NBANKS * FEB_NUM_CELLS_PER_BANK);
  t.row("isoSPI mode", "%s", isospi_mode_name());
  t.row("Max cell V", "%.3f V", (double)(FEB_CELL_MAX_VOLTAGE_MV / 1000.0f));
  t.row("Min cell V", "%.3f V", (double)(FEB_CELL_MIN_VOLTAGE_MV / 1000.0f));
  t.row("Max cell temp", "%.1f C", (double)(FEB_CELL_MAX_TEMP_DC / 10.0f));
  t.row("Min cell temp", "%.1f C", (double)(FEB_CELL_MIN_TEMP_DC / 10.0f));
#if FEB_BMS_DISABLE_TEMP_CHECKS
  t.row("Temp checks", "DISABLED (BENCH MODE)");
#else
  t.row("Temp checks", "enabled");
#endif
#if FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
  t.row("Volt checks (C)", "DISABLED (BENCH MODE)");
#else
  t.row("Volt checks (C)", "enabled");
#endif
#if FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
  t.row("Volt checks (S)", "DISABLED (BENCH MODE)");
#else
  t.row("Volt checks (S)", "enabled");
#endif
}

void cmd_bms_tasks(Interaction &io, std::span<char *const>)
{
  const struct
  {
    const char *name;
    osThreadId_t handle;
  } tasks[] = {
      {"uartRxTask", uartRxTaskHandle}, {"ADBMSTask", ADBMSTaskHandle}, {"TPSTask", TPSTaskHandle},
      {"BMSTaskRx", BMSTaskRxHandle},   {"BMSTaskTx", BMSTaskTxHandle},
  };

  static constexpr Column kCols[] = {{"Task", 12}, {"Free (words)", 13}, {"Status", 7}};
  Table t(io, kCols, "Task Stacks");

  for (const auto &task : tasks)
  {
    if (task.handle == nullptr)
    {
      continue;
    }
    const UBaseType_t hwm = uxTaskGetStackHighWaterMark((TaskHandle_t)task.handle);
    t.cell("%s", task.name);
    t.cell("%u", (unsigned)hwm);
    t.cell("%s", (hwm < 50) ? "LOW!" : "OK");
    t.end_row();
  }
}

void cmd_bms_mem(Interaction &io, std::span<char *const>)
{
  const std::size_t total = configTOTAL_HEAP_SIZE;
  const std::size_t free_heap = xPortGetFreeHeapSize();
  const std::size_t used = total - free_heap;

  KVTable t(io, 16, 20, "Heap");

  t.row("Total", "%u bytes", (unsigned)total);
  t.row("Free", "%u bytes", (unsigned)free_heap);
  t.row("Min free ever", "%u bytes", (unsigned)xPortGetMinimumEverFreeHeapSize());
  t.row("Used", "%u bytes (%u%%)", (unsigned)used, (unsigned)((used * 100) / (total > 0 ? total : 1)));
}

constexpr const char *kPingPongModeNames[] = {"OFF", "PING", "PONG"};
constexpr std::uint32_t kPingPongFrameIds[] = {0xE0, 0xE1, 0xE2, 0xE3};

void cmd_bms_can_status(Interaction &io, std::span<char *const>)
{
  static constexpr Column kCols[] = {{"Ch", 4}, {"Frame", 6}, {"Mode", 5}, {"TX", 10}, {"RX", 10}, {"Last RX", 10}};
  Table t(io, kCols, "CAN Ping/Pong");

  for (std::uint8_t ch = 1; ch <= 4; ch++)
  {
    t.cell("%u", (unsigned)ch);
    t.cell("0x%02X", (unsigned)kPingPongFrameIds[ch - 1]);
    t.cell("%s", kPingPongModeNames[FEB_CAN_PingPong_GetMode(ch)]);
    t.cell("%lu", (unsigned long)FEB_CAN_PingPong_GetTxCount(ch));
    t.cell("%lu", (unsigned long)FEB_CAN_PingPong_GetRxCount(ch));
    t.cell("%ld", (long)FEB_CAN_PingPong_GetLastCounter(ch));
    t.end_row();
  }
}

void set_pingpong_mode(Interaction &io, std::span<char *const> args, FEB_PingPong_Mode_t mode)
{
  long ch = 0;
  if (!io.arg_int(args, 1, &ch, 1, 4))
  {
    return;
  }
  FEB_CAN_PingPong_SetMode((std::uint8_t)ch, mode);
  io.println("Channel %ld (0x%02X): %s mode started", ch, (unsigned)kPingPongFrameIds[ch - 1],
             kPingPongModeNames[mode]);
}

void cmd_bms_can_ping(Interaction &io, std::span<char *const> args)
{
  set_pingpong_mode(io, args, PINGPONG_MODE_PING);
}

void cmd_bms_can_pong(Interaction &io, std::span<char *const> args)
{
  set_pingpong_mode(io, args, PINGPONG_MODE_PONG);
}

void cmd_bms_can_stop(Interaction &io, std::span<char *const> args)
{
  const char *target = io.arg_str(args, 1);
  if (target == nullptr)
  {
    return;
  }
  if (iequal(target, "all"))
  {
    FEB_CAN_PingPong_Reset();
    io.println("All channels stopped");
    return;
  }

  long ch = 0;
  if (!io.arg_int(args, 1, &ch, 1, 4))
  {
    return;
  }
  FEB_CAN_PingPong_SetMode((std::uint8_t)ch, PINGPONG_MODE_OFF);
  io.println("Channel %ld stopped", ch);
}

constexpr std::array<Command, 4> kCanSubcommands = {{
    {.name = "status",
     .description = "Per-channel mode and counters",
     .handler = cmd_bms_can_status,
     .read_only = true},
    {.name = "ping",
     .description = "Start ping mode on a channel",
     .handler = cmd_bms_can_ping,
     .args = "$channel:int"},
    {.name = "pong",
     .description = "Start pong mode on a channel",
     .handler = cmd_bms_can_pong,
     .args = "$channel:int"},
    {.name = "stop", .description = "Stop one channel or all", .handler = cmd_bms_can_stop, .args = "$channel"},
}};

// MARK: ADBMS6830B register access

constexpr const char *kRegTypeNames[] = {"WR", "RD", "ACT", "POLL"};

void cmd_bms_reg_list(Interaction &io, std::span<char *const>)
{
  static constexpr Column kCols[] = {{"Name", 10}, {"Code", 6}, {"Type", 4}, {"Description", 30}};
  Table t(io, kCols, "ADBMS6830B Commands");

  for (const ADBMS_CmdInfo_t *cmd = ADBMS_CmdTable(); cmd->name != nullptr; cmd++)
  {
    t.cell("%s", cmd->name);
    t.cell("0x%04X", cmd->code);
    t.cell("%s", kRegTypeNames[cmd->type]);
    t.cell("%s", cmd->desc);
    t.end_row();
  }
}

/** Emits the `error` row itself when @p name is not a command of @p type. */
const ADBMS_CmdInfo_t *find_reg_cmd(Interaction &io, std::span<char *const> args, ADBMS_CmdType_t type)
{
  const char *name = io.arg_str(args, 1);
  if (name == nullptr)
  {
    return nullptr;
  }

  const ADBMS_CmdInfo_t *cmd = ADBMS_FindCmdByName(name);
  if (cmd == nullptr)
  {
    io.error("error", "unknown_command", "%s", name);
    return nullptr;
  }
  /* Poll commands go on the wire the same way action commands do. */
  const bool ok = (cmd->type == type) || (type == ADBMS_CMD_ACTION && cmd->type == ADBMS_CMD_POLL);
  if (!ok)
  {
    io.error("error", "wrong_command_type", "%s is %s", cmd->name, kRegTypeNames[cmd->type]);
    return nullptr;
  }
  return cmd;
}

void cmd_bms_reg_read(Interaction &io, std::span<char *const> args)
{
  const ADBMS_CmdInfo_t *cmd = find_reg_cmd(io, args, ADBMS_CMD_READ);
  if (cmd == nullptr)
  {
    return;
  }

  static constexpr Column kCols[] = {{"IC", 4}, {"Bytes", 14}};
  Table t(io, kCols, cmd->name);

  for (std::uint8_t ic = 0; ic < FEB_NUM_IC; ic++)
  {
    std::uint8_t data[6];
    const int err = ADBMS_ReadReg(cmd->code, ic, data);

    t.cell("%u", (unsigned)ic);
    if (err < 0)
    {
      t.cell("error");
    }
    else
    {
      t.cell("%02X%02X%02X%02X%02X%02X", data[0], data[1], data[2], data[3], data[4], data[5]);
    }
    t.end_row();
  }
}

void cmd_bms_reg_write(Interaction &io, std::span<char *const> args)
{
  const ADBMS_CmdInfo_t *cmd = find_reg_cmd(io, args, ADBMS_CMD_WRITE);
  if (cmd == nullptr)
  {
    return;
  }

  const char *hex = io.arg_str(args, 2);
  if (hex == nullptr)
  {
    return;
  }
  if (std::strlen(hex) != 12)
  {
    io.error("error", "bad_data_len", "need 12 hex chars (6 bytes)");
    return;
  }

  std::uint8_t data[6];
  for (int i = 0; i < 6; i++)
  {
    const char byte_str[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
    data[i] = (std::uint8_t)std::strtoul(byte_str, nullptr, 16);
  }

  const int err = ADBMS_WriteReg(cmd->code, 0, data);
  if (err < 0)
  {
    io.error("error", "write_failed", "%s", cmd->name);
    return;
  }
  io.println("%s <- %02X%02X%02X%02X%02X%02X", cmd->name, data[0], data[1], data[2], data[3], data[4], data[5]);
}

void cmd_bms_reg_cmd(Interaction &io, std::span<char *const> args)
{
  const ADBMS_CmdInfo_t *cmd = find_reg_cmd(io, args, ADBMS_CMD_ACTION);
  if (cmd == nullptr)
  {
    return;
  }

  const int err = ADBMS_SendCmd(cmd->code);
  if (err < 0)
  {
    io.error("error", "send_failed", "%s", cmd->name);
    return;
  }
  io.println("Sent %s (0x%04X)", cmd->name, cmd->code);
}

void cmd_bms_reg_dump(Interaction &io, std::span<char *const> args)
{
  long ic = 0;
  if (!io.arg_int(args, 1, &ic, 0, FEB_NUM_IC - 1))
  {
    return;
  }

  static constexpr std::uint16_t kDumpCmds[] = {RDCFGA, RDCFGB, RDSTATA, RDSTATB, RDPWMA, RDAUXA, RDAUXB, RDAUXC,
                                                RDAUXD, RDCVA,  RDCVB,   RDCVC,   RDCVD,  RDCVE,  RDCVF,  RDSID};

  static constexpr Column kCols[] = {{"Register", 10}, {"Bytes", 14}};
  Table t(io, kCols, "Register Dump");

  for (const std::uint16_t code : kDumpCmds)
  {
    const ADBMS_CmdInfo_t *cmd = ADBMS_FindCmdByCode(code);
    if (cmd == nullptr)
    {
      continue;
    }
    std::uint8_t data[6];
    ADBMS_ReadReg(cmd->code, (std::uint8_t)ic, data);

    t.cell("%s", cmd->name);
    t.cell("%02X%02X%02X%02X%02X%02X", data[0], data[1], data[2], data[3], data[4], data[5]);
    t.end_row();
  }
}

void cmd_bms_reg_status(Interaction &io, std::span<char *const>)
{
  ADBMS_STATA_t stata = {};
  ADBMS_ReadReg(RDSTATA, 0, stata.raw);

  ADBMS_STATB_t statb = {};
  ADBMS_ReadReg(RDSTATB, 0, statb.raw);

  std::uint8_t sid[6];
  ADBMS_ReadReg(RDSID, 0, sid);

  KVTable t(io, 8, 20, "ADBMS6830B Status (IC 0)");

  t.row("VREF2", "%.3f V", (double)(ADBMS_CodeToVoltage_mV(stata.values.VREF2) / 1000.0f));
  t.row("ITMP", "%.1f C", (double)ADBMS_CodeToTemp_C(stata.values.ITMP));
  t.row("VA", "%.3f V", (double)(ADBMS_CodeToVoltage_mV(stata.values.VA) / 1000.0f));
  t.row("VD", "%.3f V", (double)(ADBMS_CodeToVoltage_mV(statb.bits.VD) / 1000.0f));
  t.row("UV", "0x%04X", (unsigned)(statb.bits.C_UV_LO | ((std::uint16_t)statb.bits.C_UV_HI << 8)));
  t.row("OV", "0x%04X", (unsigned)(statb.bits.C_OV_LO | ((std::uint16_t)statb.bits.C_OV_HI << 8)));
  t.row("SID", "%02X%02X%02X%02X%02X%02X", sid[5], sid[4], sid[3], sid[2], sid[1], sid[0]);
}

constexpr std::array<Command, 6> kRegSubcommands = {{
    {.name = "list", .description = "Every datasheet command", .handler = cmd_bms_reg_list, .read_only = true},
    {.name = "read",
     .description = "Read a register group from every IC",
     .handler = cmd_bms_reg_read,
     .args = "$name",
     .read_only = true},
    {.name = "write",
     .description = "Write a register group to IC 0",
     .handler = cmd_bms_reg_write,
     .args = "$name $hex12"},
    {.name = "cmd", .description = "Send an action or poll command", .handler = cmd_bms_reg_cmd, .args = "$name"},
    {.name = "dump",
     .description = "Key registers of one IC",
     .handler = cmd_bms_reg_dump,
     .args = "$ic:int",
     .read_only = true},
    {.name = "status",
     .description = "STATA/STATB summary and serial id",
     .handler = cmd_bms_reg_status,
     .read_only = true},
}};

// MARK: Commands Table

constexpr std::array<Command, 19> kBmsSubcommands = {{
    {.name = "status",
     .description = "Pack summary: state, voltages, temps",
     .handler = cmd_bms_status,
     .read_only = true},
    {.name = "cells",
     .description = "Per-cell voltages (C/S) and balance flags",
     .handler = cmd_bms_cells,
     .read_only = true},
    {.name = "temps", .description = "Temperatures, one line per bank", .handler = cmd_bms_temps, .read_only = true},
    {.name = "cell",
     .description = "One cell in detail",
     .handler = cmd_bms_cell,
     .args = "$bank:int $cell:int",
     .read_only = true},
    {.name = "cell-stats",
     .description = "Voltage and temp rows for every cell and sensor",
     .handler = cmd_bms_cell_stats,
     .text_hidden = true,
     .read_only = true},
    {.name = "therm-raw",
     .description = "Raw thermistor voltages and ADC codes",
     .handler = cmd_bms_therm_raw,
     .read_only = true},
    {.name = "volts",
     .description = "ADBMS supply/reference voltages per IC",
     .handler = cmd_bms_volts,
     .read_only = true},
    /* Not read_only: `state <name>` transitions. The bare read flags itself. */
    {.name = "state", .description = "Show the state, or transition to another", .handler = cmd_bms_state},
    {.name = "balance",
     .description = "Cell balancing",
     .handler = cmd_bms_balance,
     .subs = kBalanceSubcommands,
     .read_only = true},
    {.name = "charger",
     .description = "Charger telemetry and the command we send",
     .handler = cmd_bms_charger,
     .read_only = true},
    {.name = "ivt", .description = "IVT current/voltage sensor", .handler = cmd_bms_ivt, .read_only = true},
    {.name = "gpio",
     .description = "Shutdown loop, relay sense, indicators",
     .handler = cmd_bms_gpio,
     .read_only = true},
    {.name = "errors", .description = "Error type and fault summary", .handler = cmd_bms_errors, .read_only = true},
    {.name = "config", .description = "Compile-time pack configuration", .handler = cmd_bms_config, .read_only = true},
    {.name = "spi", .description = "isoSPI mode and failover", .handler = cmd_bms_spi, .read_only = true},
    {.name = "tasks", .description = "FreeRTOS stack high-water marks", .handler = cmd_bms_tasks, .read_only = true},
    {.name = "mem", .description = "Heap usage", .handler = cmd_bms_mem, .read_only = true},
    {.name = "can", .description = "CAN ping/pong test channels", .subs = kCanSubcommands},
    {.name = "reg", .description = "ADBMS6830B register access", .subs = kRegSubcommands},
}};

constexpr std::array<Command, 1> kBmsCommands = {{
    group("bms", "BMS board commands", kBmsSubcommands),
}};

} // namespace

namespace bms
{
inline constexpr auto kAll = concat(kSystemCommands, kBmsCommands);
static_assert(!has_duplicate_names(kAll), "duplicate console command name");

inline constexpr Console kConsole{FEB_UART_INSTANCE_1, kAll};
} // namespace bms

extern "C" void BMS_Console_ProcessLine(const char *line, size_t len)
{
  bms::kConsole.process_line(line, len);
}
