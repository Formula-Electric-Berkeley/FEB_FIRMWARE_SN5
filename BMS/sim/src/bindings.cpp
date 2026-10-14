/**
 * @file bindings.cpp
 * @brief Host implementations of the C interfaces the simulated firmware
 *        modules call, backed by the plant model. These replace, at link
 *        time, FEB_HW_Relay.cpp, the FEB_CAN_* modules, the ADBMS6830 task
 *        and FEB_Time; the firmware source itself is compiled unmodified.
 *
 * Freshness semantics (last_rx == 0 means never, strict vs inclusive
 * comparisons) are copied from the real modules so timing edges match.
 */
#include "sim.hpp"

#include "FEB_ADBMS6830B.h"
#include "FEB_CAN_Charger.h"
#include "FEB_CAN_DASH.h"
#include "FEB_CAN_Heartbeat.h"
#include "FEB_CAN_IVT.h"
#include "FEB_CAN_State.h"
#include "FEB_HW_Relay.h"
#include "FEB_SM.h"
#include "feb_time.h"

using sim::plant;

namespace
{
FEB_Relay_State_t relay(bool closed)
{
  return closed ? FEB_RELAY_STATE_CLOSE : FEB_RELAY_STATE_OPEN;
}

bool fresh(uint32_t last_rx, uint32_t timeout_ms)
{
  return last_rx != 0 && (sim::now_ms() - last_rx) < timeout_ms;
}

BMS_State_t g_can_state = BMS_STATE_BOOT;
} // namespace

extern "C"
{

/* ============================================================================
 * FEB_Time
 * ============================================================================ */

void FEB_Time_Init(void)
{
}

uint64_t FEB_Time_Us(void)
{
  /* Every read costs 1 us of simulated time, so busy-wait loops terminate. */
  sim::advance_us(1);
  return sim::now_us();
}

uint32_t FEB_Time_Us32(void)
{
  return static_cast<uint32_t>(FEB_Time_Us());
}

void FEB_Time_OnSysTick(void)
{
}

/* ============================================================================
 * FEB_HW_Relay
 * ============================================================================ */

void FEB_HW_AIR_Plus_Set(bool closed)
{
  plant().out.air_plus_cmd = closed;
}

void FEB_HW_Precharge_Set(bool closed)
{
  plant().out.precharge_cmd = closed;
}

void FEB_HW_BMS_Shutdown_Set(bool closed)
{
  plant().out.bms_shutdown_relay = closed;
}

bool FEB_HW_BMS_Shutdown_Get(void)
{
  return plant().out.bms_shutdown_relay;
}

void FEB_HW_BMS_Indicator_Set(bool on)
{
  plant().out.bms_indicator = on;
}

bool FEB_HW_BMS_Indicator_Get(void)
{
  return plant().out.bms_indicator;
}

void FEB_HW_Fault_Indicator_Set(bool on)
{
  plant().out.fault_indicator = on;
}

void FEB_HW_Buzzer_Set(bool on)
{
  plant().out.buzzer = on;
}

void FEB_HW_TSSI_Set(bool green)
{
  plant().out.tssi_green = green;
}

FEB_Relay_State_t FEB_HW_AIR_Minus_Sense(void)
{
  return relay(plant().air_minus.closed());
}

FEB_Relay_State_t FEB_HW_AIR_Plus_Sense(void)
{
  return relay(plant().air_plus.closed());
}

FEB_Relay_State_t FEB_HW_Precharge_Sense(void)
{
  return relay(plant().precharge.closed());
}

FEB_Relay_State_t FEB_HW_Shutdown_Sense(void)
{
  sim::Plant &p = plant();
  if (p.shutdown_sense_glitched(sim::now_us()))
  {
    return FEB_RELAY_STATE_OPEN;
  }
  return relay(p.loop_closed_upstream());
}

FEB_Relay_State_t FEB_HW_IMD_Sense(void)
{
  return relay(plant().imd_latch());
}

void FEB_HW_TSMS_Indicator_Set(bool on)
{
  plant().out.tsms_indicator = on;
}

bool FEB_HW_TSMS_Indicator_Get(void)
{
  return plant().out.tsms_indicator;
}

bool FEB_HW_Reset_Button_Pressed(void)
{
  return plant().reset_button;
}

void FEB_HW_Open_All_Relays(void)
{
  plant().out.air_plus_cmd = false;
  plant().out.precharge_cmd = false;
}

bool FEB_HW_Is_HV_Safe(void)
{
  return !plant().air_plus.closed() && !plant().air_minus.closed();
}

/* ============================================================================
 * FEB_CAN_State
 * ============================================================================ */

int FEB_CAN_State_SetState(BMS_State_t state)
{
  g_can_state = state;
  sim::record_state(state);
  return 0;
}

BMS_State_t FEB_CAN_State_GetState(void)
{
  return g_can_state;
}

const char *FEB_CAN_State_GetStateName(BMS_State_t state)
{
  return sim::state_name(state);
}

/* ============================================================================
 * FEB_CAN_IVT
 * ============================================================================ */

bool FEB_CAN_IVT_IsDataFresh(uint32_t timeout_ms)
{
  return fresh(plant().ivt_last_rx, timeout_ms);
}

float FEB_CAN_IVT_GetVoltage(void)
{
  if (!FEB_CAN_IVT_IsDataFresh(1000))
  {
    return 0.0f;
  }
  return plant().ivt_voltage_v;
}

float FEB_CAN_IVT_GetCurrent(void)
{
  return plant().ivt_current_a;
}

/* ============================================================================
 * FEB_CAN_DASH / FEB_CAN_Heartbeat
 * ============================================================================ */

bool FEB_CAN_DASH_IsReadyToDrive(uint32_t timeout_ms)
{
  const sim::Plant &p = plant();
  if (p.dash_last_rx == 0 || (sim::now_ms() - p.dash_last_rx) > timeout_ms)
  {
    return false;
  }
  return p.dash_r2d_rx;
}

bool FEB_CAN_Heartbeat_OthersPresent(uint32_t timeout_ms)
{
  return fresh(plant().heartbeat_last_rx, timeout_ms);
}

/* ============================================================================
 * FEB_CAN_Charger
 * ============================================================================ */

bool FEB_CAN_Charger_Received(void)
{
  return fresh(plant().charger.last_rx_tick, FEB_CHARGER_RX_TIMEOUT_MS);
}

int8_t FEB_CAN_Charging_Status(void)
{
  /* Same decision order as FEB_CAN_Charger.cpp, minus the charger-side
   * recoverable flags the model does not produce. */
  const sim::Plant &p = plant();
  if (p.charger.done)
  {
    return 1;
  }
  if (FEB_CAN_Charger_Received() && p.charger.hw_failure)
  {
    return -1;
  }
  if (p.afe.snapshot_total_v <= 0.0f)
  {
    return 1;
  }
  if (p.afe.snapshot_max_cell_v * 1000.0f >= FEB_CONFIG_CELL_SOFT_MAX_VOLTAGE_mV)
  {
    return 1;
  }
  if (p.afe.snapshot_max_temp_c * 10.0f >= FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC)
  {
    return 1;
  }
  return 0;
}

void FEB_CAN_Charger_Start_Charge(void)
{
  plant().charger.commanded_on = true;
  plant().charger.done = false;
}

void FEB_CAN_Charger_Stop_Charge(void)
{
  plant().charger.commanded_on = false;
  plant().charger.done = true;
}

void FEB_CAN_Charger_Process(void)
{
  /* Unplug ends the session; outside CHARGING the charger is held OFF. */
  if (!FEB_CAN_Charger_Received())
  {
    plant().charger.done = false;
  }
  if (FEB_SM_Get_Current_State() != BMS_STATE_CHARGING)
  {
    plant().charger.commanded_on = false;
  }
}

/* ============================================================================
 * ADBMS6830 (task stand-in, see AfeModel)
 * ============================================================================ */

uint32_t FEB_ADBMS_Get_Fault_Flags(void)
{
  return plant().afe.fault_flags;
}

uint32_t FEB_ADBMS_Get_Last_Update_Tick(void)
{
  return plant().afe.last_update_tick;
}

void FEB_ADBMS_Set_Validation_Profile(FEB_Validation_Profile_t profile)
{
  plant().afe.profile = profile;
}

FEB_Validation_Profile_t FEB_ADBMS_Get_Validation_Profile(void)
{
  return plant().afe.profile;
}

float FEB_ADBMS_Snapshot_Total_Voltage(void)
{
  return plant().afe.snapshot_total_v;
}

float FEB_ADBMS_Snapshot_Max_Cell_Voltage(void)
{
  return plant().afe.snapshot_max_cell_v;
}

float FEB_ADBMS_Snapshot_Max_Temp(void)
{
  return plant().afe.snapshot_max_temp_c;
}

void FEB_Stop_Balance(void)
{
  plant().afe.balancing = false;
  plant().afe.stop_balance_requests++;
}

bool FEB_Cell_Balance_Complete(void)
{
  float delta = plant().afe.cell_delta_mv;
  return delta >= 0.0f && delta < FEB_CELL_BALANCE_THRESHOLD_MV;
}

} // extern "C"
