/**
 * @file scenarios.cpp
 * @brief BMS state machine scenarios. Each one boots a fresh firmware image
 *        against the plant, scripts the outside world, and checks the
 *        firmware's response.
 *
 * Scenarios marked CHARACTERIZATION pin down current behaviour that may be
 * worth a design review; they are not a claim that the behaviour is right.
 */
#include "scenario.hpp"

#include "FEB_SM.h"

using sim::Check;
using sim::Sim;

namespace
{

/* LV on, IMD reset by the operator, DASH/PCU on CAN, TSMS still off. */
void power_up_car(Sim &s)
{
  s.boot();
  s.plant().imd_reset();
  s.run_ms(200);
}

/* Close the shutdown loop and let the firmware precharge. */
bool to_energized(Sim &s, Check &c)
{
  s.plant().external_loop_closed = true;
  return c.expect_reaches(s, BMS_STATE_ENERGIZED, 15000);
}

bool to_drive(Sim &s, Check &c)
{
  if (!to_energized(s, c))
  {
    return false;
  }
  s.plant().r2d = true;
  return c.expect_reaches(s, BMS_STATE_DRIVE, 500);
}

/* Car-side nodes off, charger plugged in: the BMS should go BATTERY_FREE. */
bool power_up_on_charger(Sim &s, Check &c, double soc)
{
  for (double &x : s.plant().soc)
  {
    x = soc;
  }
  s.plant().others_on_can = false;
  s.plant().dash_alive = false;
  s.plant().charger.present = true;
  s.boot();
  s.plant().imd_reset();
  return c.expect_reaches(s, BMS_STATE_BATTERY_FREE, 2000);
}

} // namespace

/* ============================================================================
 * Drive path
 * ============================================================================ */

SIM_SCENARIO(boot_to_lv_power, "Boot settles in LV_POWER with HV off and TSSI green")
{
  power_up_car(s);
  c.expect_state(s, BMS_STATE_LV_POWER);
  c.expect_holds(s, BMS_STATE_LV_POWER, 5000);
  c.expect(s.plant().out.bms_shutdown_relay, "BMS shutdown relay should be closed");
  c.expect(s.plant().out.tssi_green, "TSSI should be green");
  c.expect(!s.plant().out.air_plus_cmd && !s.plant().out.precharge_cmd, "AIR+/precharge should be commanded open");
}

SIM_SCENARIO(nominal_drive_cycle, "TSMS on -> precharge -> ENERGIZED -> R2D -> DRIVE -> R2D off -> ENERGIZED")
{
  power_up_car(s);
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_PRECHARGE, 100);
  uint32_t t_pc = sim::now_ms();
  c.expect_reaches(s, BMS_STATE_ENERGIZED, 15000);
  uint32_t precharge_ms = sim::now_ms() - t_pc;
  c.expect(precharge_ms >= 2000 && precharge_ms <= 10000, "precharge took %u ms, outside 2-10 s window", precharge_ms);
  s.run_ms(50); /* precharge relay drop-out */
  c.expect(s.plant().air_plus.closed() && !s.plant().precharge.closed(), "AIR+ closed, precharge open after energize");
  c.expect(s.plant().bus_v() > 0.95 * s.plant().pack_terminal_v(), "bus should be at pack voltage");

  s.plant().r2d = true;
  c.expect_reaches(s, BMS_STATE_DRIVE, 500);
  s.plant().load_current_a = 120.0;
  c.expect_holds(s, BMS_STATE_DRIVE, 10000);
  s.plant().load_current_a = 0.0;

  s.plant().r2d = false;
  c.expect_reaches(s, BMS_STATE_ENERGIZED, 1000);
}

SIM_SCENARIO(tsms_off_while_energized_faults_then_recovers,
             "Shutdown loop opens while ENERGIZED -> FAULT_BMS; loop closes again -> LV_POWER")
{
  power_up_car(s);
  if (!to_energized(s, c))
  {
    return;
  }
  s.plant().external_loop_closed = false;
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 10);
  s.run_ms(600);
  c.expect(!s.plant().air_plus.closed() && !s.plant().air_minus.closed(), "contactors open after fault settle");
  c.expect(s.plant().out.buzzer, "buzzer on while latched");

  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_LV_POWER, 200);
  c.expect(!s.plant().out.buzzer, "buzzer off after recovery");
}

/* ============================================================================
 * Precharge
 * ============================================================================ */

SIM_SCENARIO(precharge_too_fast, "Shorted precharge resistor completes <2 s -> FAULT_BMS (inrush detection)")
{
  power_up_car(s);
  s.plant().precharge_resistor_shorted = true;
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 3000);
}

SIM_SCENARIO(precharge_timeout, "Open precharge resistor never charges the bus -> FAULT_BMS at 10 s")
{
  power_up_car(s);
  s.plant().precharge_resistor_open = true;
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_PRECHARGE, 100);
  uint32_t t0 = sim::now_ms();
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 11000);
  uint32_t dt = sim::now_ms() - t0;
  c.expect(dt >= 9900 && dt <= 10100, "timeout fired after %u ms, expected ~10000", dt);
}

SIM_SCENARIO(precharge_relay_welded, "Precharge relay welds after energize -> contactor mismatch -> FAULT_BMS")
{
  power_up_car(s);
  if (!to_energized(s, c))
  {
    return;
  }
  s.plant().precharge.fault = sim::Contactor::Fault::WeldedClosed;
  uint32_t t0 = sim::now_ms();
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 1000);
  uint32_t dt = sim::now_ms() - t0;
  c.expect(dt >= FEB_CONTACTOR_FEEDBACK_TIMEOUT_MS, "faulted after %u ms, before the %u ms debounce", dt,
           FEB_CONTACTOR_FEEDBACK_TIMEOUT_MS);
}

SIM_SCENARIO(air_plus_stuck_open, "AIR+ never closes at end of precharge -> FAULT_BMS")
{
  power_up_car(s);
  s.plant().air_plus.fault = sim::Contactor::Fault::StuckOpen;
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 15000);
}

SIM_SCENARIO(welded_precharge_blocks_health_check,
             "CHARACTERIZATION: precharge welded before TSMS -> waits in BUS_HEALTH_CHECK with no fault")
{
  power_up_car(s);
  s.plant().precharge.fault = sim::Contactor::Fault::WeldedClosed;
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_BUS_HEALTH_CHECK, 100);
  c.expect_holds(s, BMS_STATE_BUS_HEALTH_CHECK, 30000);
}

/* ============================================================================
 * Shutdown loop in DRIVE
 * ============================================================================ */

SIM_SCENARIO(shutdown_glitch_rejected_in_drive, "200 us noise pulse on SHS_IN in DRIVE is rejected by majority vote")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  /* Starts exactly on the next SM tick so the first sample sees OPEN. */
  uint64_t t = (sim::now_us() / 1000 + 1) * 1000;
  s.plant().shutdown_sense_glitch(t, t + 200);
  c.expect_holds(s, BMS_STATE_DRIVE, 1000);
}

SIM_SCENARIO(shutdown_long_glitch_trips_in_drive, "900 us OPEN on SHS_IN in DRIVE is confirmed by majority -> FAULT_BMS")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  uint64_t t = (sim::now_us() / 1000 + 1) * 1000;
  s.plant().shutdown_sense_glitch(t, t + 900);
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 5);
}

SIM_SCENARIO(estop_in_drive_then_recover,
             "E-stop in DRIVE -> FAULT_BMS; released after the bus bleeds down -> LV_POWER -> re-energize")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  s.plant().external_loop_closed = false;
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 5);
  s.run_ms(10000);
  c.expect(s.plant().bus_v() < 60.0, "bus should have bled below 60 V (%.1f V)", s.plant().bus_v());
  s.plant().r2d = false;
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_LV_POWER, 200);
  c.expect_reaches(s, BMS_STATE_ENERGIZED, 15000);
}

SIM_SCENARIO(quick_estop_release_trips_precharge_too_fast,
             "CHARACTERIZATION: e-stop released 1 s after a DRIVE trip -> residual bus voltage makes the "
             "re-precharge 'too fast' -> latched FAULT_BMS (depends on bus bleed time, here tau=2 s)")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  s.plant().external_loop_closed = false;
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 5);
  s.run_ms(1000);
  s.plant().r2d = false;
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_PRECHARGE, 300);
  double bus_at_start = s.plant().bus_v();
  c.expect(bus_at_start > 0.5 * s.plant().pack_terminal_v(), "bus should still be charged (%.0f V)", bus_at_start);
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 3000);
  c.expect_holds(s, BMS_STATE_FAULT_BMS, 5000); /* not the recoverable shutdown kind */
}

/* ============================================================================
 * IMD
 * ============================================================================ */

SIM_SCENARIO(imd_not_reset_blocks_hv, "IMD latch never reset: loop stays open, no fault, HV stays off")
{
  s.boot();
  s.plant().external_loop_closed = true;
  c.expect_holds(s, BMS_STATE_LV_POWER, 5000);
  c.expect(!FEB_SM_IMD_Armed(), "IMD monitoring should not be armed before the first reset");
}

SIM_SCENARIO(imd_trip_while_energized, "Isolation fault while ENERGIZED latches a fault and drops HV")
{
  power_up_car(s);
  if (!to_energized(s, c))
  {
    return;
  }
  s.plant().isolation_fault = true;
  s.run_ms(1000);
  c.expect(FEB_SM_Is_Faulted(), "expected a fault state, got %s", sim::state_name(s.state()));
  c.expect(!s.plant().air_plus.closed() && !s.plant().air_minus.closed(), "contactors open");
}

SIM_SCENARIO(imd_trip_outside_loop_reports_imd_fault,
             "IMD wired outside the loop: isolation fault while ENERGIZED -> FAULT_IMD after 100 ms")
{
  s.plant().params.imd_in_loop = false;
  power_up_car(s);
  if (!to_energized(s, c))
  {
    return;
  }
  s.plant().isolation_fault = true;
  uint32_t t0 = sim::now_ms();
  c.expect_reaches(s, BMS_STATE_FAULT_IMD, 200);
  uint32_t dt = sim::now_ms() - t0;
  c.expect(dt >= FEB_IMD_FAULT_CONFIRM_MS, "faulted after %u ms, before the IMD debounce", dt);
}

/* ============================================================================
 * Current / sensors
 * ============================================================================ */

SIM_SCENARIO(overcurrent_short_pulse_tolerated, "400 A for 30 ms (< 50 ms confirm) does not fault")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  s.plant().load_current_a = 400.0;
  s.run_ms(30);
  s.plant().load_current_a = 50.0;
  c.expect_holds(s, BMS_STATE_DRIVE, 500);
}

SIM_SCENARIO(overcurrent_sustained_faults, "400 A sustained -> FAULT_BMS after the 50 ms confirm window")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  s.plant().load_current_a = 400.0;
  uint32_t t0 = sim::now_ms();
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 200);
  uint32_t dt = sim::now_ms() - t0;
  c.expect(dt >= FEB_OVERCURRENT_CONFIRM_MS, "faulted after %u ms, before the confirm window", dt);
}

SIM_SCENARIO(ivt_loss_in_drive, "IVT stops transmitting in DRIVE -> FAULT_BMS after 1 s")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  s.plant().ivt_alive = false;
  c.expect_holds(s, BMS_STATE_DRIVE, 900);
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 200);
}

SIM_SCENARIO(cell_overvoltage, "One cell reads 4.25 V -> FAULT_BMS after 3 consecutive scans")
{
  power_up_car(s);
  s.plant().cell_v_offset[37] = 4.25 - s.plant().cell_terminal_v(37);
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 500);
}

SIM_SCENARIO(cell_monitor_stalls, "ADBMS task stops updating in DRIVE -> FAULT_BMS after 1 s")
{
  power_up_car(s);
  if (!to_drive(s, c))
  {
    return;
  }
  s.plant().afe.alive = false;
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 1200);
}

SIM_SCENARIO(cell_monitor_never_starts, "No ADBMS scan ever completes -> FAULT_BMS after the 3 s boot grace")
{
  s.plant().afe.alive = false;
  s.boot();
  s.plant().imd_reset();
  c.expect_holds(s, BMS_STATE_LV_POWER, FEB_ADBMS_BOOT_GRACE_MS - 100);
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 200);
}

/* ============================================================================
 * Charging / balancing
 * ============================================================================ */

SIM_SCENARIO(charge_to_soft_limit, "Charger only on CAN -> FREE -> precharge -> CHARGING -> soft limit -> FREE")
{
  power_up_on_charger(s, c, 0.97);
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_CHARGER_PRECHARGE, 1000);
  c.expect_reaches(s, BMS_STATE_CHARGING, 15000);
  c.expect(s.plant().afe.profile == FEB_VALIDATION_PROFILE_CHARGING, "CHARGING validation profile active");
  s.run_ms(20);
  c.expect(s.plant().hv_current_a() < -1.0, "charge current should flow (I=%.1f A)", s.plant().hv_current_a());
  c.expect_reaches(s, BMS_STATE_BATTERY_FREE, 20 * 60 * 1000);
  c.expect(s.plant().afe.snapshot_max_cell_v * 1000.0f >= FEB_CONFIG_CELL_SOFT_MAX_VOLTAGE_mV,
           "stopped at max cell %.3f V", static_cast<double>(s.plant().afe.snapshot_max_cell_v));
  c.expect(s.plant().afe.profile == FEB_VALIDATION_PROFILE_NORMAL, "NORMAL profile restored");
  c.expect_holds(s, BMS_STATE_BATTERY_FREE, 5000);
}

SIM_SCENARIO(charging_hot_cell_soft_stops,
             "CHARACTERIZATION: cell steps to 50 C mid-charge -> soft stop to FREE before the 45 C hard fault")
{
  power_up_on_charger(s, c, 0.60);
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_CHARGING, 15000);
  s.plant().temp_c[12] = 50.0;
  c.expect_reaches(s, BMS_STATE_BATTERY_FREE, 500);
  c.expect_holds(s, BMS_STATE_BATTERY_FREE, 2000);
}

SIM_SCENARIO(charger_hw_failure, "Charger reports hardware failure while CHARGING -> FAULT_CHARGING")
{
  power_up_on_charger(s, c, 0.60);
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_CHARGING, 15000);
  s.plant().charger.hw_failure = true;
  c.expect_reaches(s, BMS_STATE_FAULT_CHARGING, 50);
  s.run_ms(600);
  c.expect(!s.plant().charger.commanded_on, "charger commanded off");
}

SIM_SCENARIO(car_reconnects_during_free, "DASH/PCU come back on CAN while BATTERY_FREE -> LV_POWER")
{
  power_up_on_charger(s, c, 0.60);
  s.plant().charger.present = false;
  s.plant().others_on_can = true;
  c.expect_reaches(s, BMS_STATE_LV_POWER, 1500);
}

SIM_SCENARIO(balance_completes, "Console BALANCE on a balanced pack -> back to BATTERY_FREE within ~1 s")
{
  power_up_on_charger(s, c, 0.60);
  FEB_SM_Transition(BMS_STATE_BALANCE); /* what `BMS|balance|on` does */
  c.expect_state(s, BMS_STATE_BALANCE);
  c.expect_reaches(s, BMS_STATE_BATTERY_FREE, 1500);
}

SIM_SCENARIO(balance_with_hv_on_faults, "Shutdown loop closed during BALANCE -> FAULT_CHARGING after 200 ms")
{
  power_up_on_charger(s, c, 0.60);
  s.plant().cell_v_offset[5] = 0.05; /* 50 mV imbalance keeps BALANCE running */
  c.expect_reaches(s, BMS_STATE_BATTERY_FREE, 2000);
  FEB_SM_Transition(BMS_STATE_BALANCE);
  c.expect_holds(s, BMS_STATE_BALANCE, 3000);
  s.plant().external_loop_closed = true;
  c.expect_reaches(s, BMS_STATE_FAULT_CHARGING, 400);
  c.expect(s.plant().afe.stop_balance_requests > 0, "balancing stop requested");
}
