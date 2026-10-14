/**
 * @file plant.hpp
 * @brief Physical/electrical model of everything the BMS firmware senses and
 *        drives: accumulator cells, HV contactors, shutdown loop, precharge
 *        circuit, IVT, IMD latch, and the other nodes on CAN.
 *
 * The firmware never sees this class directly. bindings.cpp implements the
 * firmware's C interfaces (FEB_HW_*, FEB_CAN_*, FEB_ADBMS_*) on top of it.
 *
 * Wiring assumptions (verify against the SN5 schematic):
 *  - SHS_IN (FEB_HW_Shutdown_Sense) is the shutdown loop UPSTREAM of the BMS
 *    shutdown relay.
 *  - AIR-, AIR+ and precharge coils are powered by the loop DOWNSTREAM of the
 *    BMS relay, so opening the BMS relay drops all three.
 *  - The IMD latch sits in series with the loop (imd_in_loop).
 */
#pragma once

#include "FEB_ADBMS6830B.h"
#include "FEB_Const.h"

#include <cstdint>
#include <vector>

namespace sim
{

struct PlantParams
{
  int cells = FEB_NBANKS * FEB_NUM_CELLS_PER_BANK; /* series elements */
  double cell_capacity_ah = 13.0;
  double cell_resistance_ohm = 0.0012;
  double initial_soc = 0.60;
  double ambient_c = 25.0;
  double cell_thermal_mass_j_per_k = 400.0;
  double cell_cooling_w_per_k = 2.0;

  double precharge_tau_s = 1.0;  /* RC of precharge resistor + bus capacitance */
  double bus_bleed_tau_s = 2.0;  /* bus discharge when isolated */
  uint32_t contactor_close_ms = 15;
  uint32_t contactor_open_ms = 10;

  bool imd_in_loop = true;

  uint32_t ivt_period_ms = 10;
  uint32_t dash_period_ms = 100;
  uint32_t heartbeat_period_ms = 100;
  uint32_t charger_period_ms = 500;
  uint32_t afe_scan_period_ms = 100; /* voltage + temperature scan cadence */
};

class Contactor
{
public:
  enum class Fault
  {
    None,
    WeldedClosed,
    StuckOpen,
  };

  void update(bool coil_powered, uint32_t now_ms, const PlantParams &p);
  bool closed() const
  {
    return closed_;
  }

  Fault fault = Fault::None;

private:
  bool closed_ = false;
  bool coil_ = false;
  uint32_t coil_change_ms_ = 0;
};

/**
 * Stand-in for the ADBMS6830 task (FEB_Task_ADBMS + FEB_ADBMS6830B.cpp). It
 * honours the contract the SM relies on — lock-free snapshots, sticky fault
 * flags after FEB_*_ERROR_THRESH consecutive bad scans, last-update tick,
 * profile-dependent temperature limit — without modelling isoSPI traffic.
 */
struct AfeModel
{
  bool alive = true; /* false = task hung / chain dead: no more scans */
  uint32_t last_update_tick = 0;
  uint32_t fault_flags = 0;
  FEB_Validation_Profile_t profile = FEB_VALIDATION_PROFILE_NORMAL;
  float snapshot_total_v = 0.0f;
  float snapshot_max_cell_v = 0.0f;
  float snapshot_max_temp_c = 0.0f;
  float cell_delta_mv = -1.0f;
  int voltage_violation_scans = 0;
  int temp_violation_scans = 0;
  bool balancing = false;
  uint32_t stop_balance_requests = 0;
};

struct ChargerModel
{
  bool present = false;      /* charger on CAN */
  bool hw_failure = false;   /* charger reports hardware fault */
  bool commanded_on = false; /* BMS sent START */
  bool done = false;         /* mirrors tx_msg.done_charging: set on STOP, cleared on START or unplug */
  uint32_t last_rx_tick = 0;
  double current_a = FEB_CHARGE_CURRENT_dA / 10.0;
};

/* Everything the firmware drives. */
struct Outputs
{
  bool air_plus_cmd = false;
  bool precharge_cmd = false;
  bool bms_shutdown_relay = false;
  bool bms_indicator = false;
  bool fault_indicator = false;
  bool buzzer = false;
  bool tssi_green = false;
  bool tsms_indicator = false;
};

class Plant
{
public:
  explicit Plant(const PlantParams &params = {});

  /** Advance the physics by one 1 ms tick. */
  void tick(uint32_t now_ms);

  /** Run the AFE stand-in's scan if due (called from the ADBMS task slot). */
  void afe_task(uint32_t now_ms);

  /* ── Derived electrical state ─────────────────────────────────────── */
  bool loop_closed_upstream() const; /* what SHS_IN sees */
  bool loop_powers_contactors() const;
  double pack_ocv() const;
  double pack_terminal_v() const;
  double cell_terminal_v(int i) const;
  double max_cell_temp() const;
  double avg_soc() const;
  double hv_current_a() const
  {
    return current_a_;
  }
  double bus_v() const
  {
    return bus_v_;
  }

  /* ── Operator / scenario controls ─────────────────────────────────── */
  void imd_reset()
  {
    imd_latch_ = !isolation_fault;
  }
  bool imd_latch() const
  {
    return imd_latch_;
  }
  /** Force SHS_IN to read OPEN between [start_us, end_us) without opening
   *  the real loop: models electrical noise on the sense line. */
  void shutdown_sense_glitch(uint64_t start_us, uint64_t end_us)
  {
    glitch_start_us_ = start_us;
    glitch_end_us_ = end_us;
  }
  bool shutdown_sense_glitched(uint64_t now_us) const
  {
    return now_us >= glitch_start_us_ && now_us < glitch_end_us_;
  }

  PlantParams params;
  Outputs out;

  /* External world */
  bool external_loop_closed = false; /* TSMS, e-stops, inertia switch, ... */
  bool isolation_fault = false;      /* IMD sees low isolation: latch trips */
  bool reset_button = false;
  bool others_on_can = true; /* DASH/PCU heartbeats */
  bool dash_alive = true;
  bool r2d = false;
  bool ivt_alive = true;
  double load_current_a = 0.0; /* drive demand; flows only with HV connected */

  /* Fault injection on the precharge path */
  bool precharge_resistor_shorted = false;
  bool precharge_resistor_open = false;

  Contactor air_minus, air_plus, precharge;
  AfeModel afe;
  ChargerModel charger;

  std::vector<double> soc;    /* per cell, 0..1 */
  std::vector<double> temp_c; /* per cell */
  std::vector<double> cell_v_offset; /* per cell, added to terminal voltage */

  /* Timestamps of the last CAN frames the firmware "received" */
  uint32_t ivt_last_rx = 0;
  float ivt_voltage_v = 0.0f;
  float ivt_current_a = 0.0f;
  uint32_t dash_last_rx = 0;
  bool dash_r2d_rx = false;
  uint32_t heartbeat_last_rx = 0;

private:
  void step_cells(double dt_s);
  void step_bus(double dt_s);
  void step_can(uint32_t now_ms);
  static double ocv(double soc);

  bool imd_latch_ = false; /* S-R latch powers up LOW until reset */
  double current_a_ = 0.0; /* + = discharge */
  double bus_v_ = 0.0;
  uint64_t glitch_start_us_ = 0;
  uint64_t glitch_end_us_ = 0;
  uint32_t last_scan_ms_ = 0;
};

/** The single plant instance the firmware bindings talk to. */
Plant &plant();
void reset_plant(const PlantParams &params);

} // namespace sim
