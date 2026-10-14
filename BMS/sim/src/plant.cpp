/**
 * @file plant.cpp
 * @brief Plant model implementation. Deliberately simple, first-order models:
 *        enough fidelity to exercise every timing gate in the state machine.
 */
#include "plant.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace sim
{

namespace
{
std::unique_ptr<Plant> g_plant;

/* NMC-ish open-circuit voltage curve (SOC, volts). */
constexpr double kOcvTable[][2] = {
    {0.00, 3.00}, {0.05, 3.30}, {0.10, 3.45}, {0.20, 3.55}, {0.35, 3.62}, {0.50, 3.70},
    {0.65, 3.80}, {0.80, 3.95}, {0.90, 4.05}, {1.00, 4.20},
};
} // namespace

Plant &plant()
{
  if (!g_plant)
  {
    g_plant = std::make_unique<Plant>();
  }
  return *g_plant;
}

void reset_plant(const PlantParams &params)
{
  g_plant = std::make_unique<Plant>(params);
}

/* ============================================================================
 * Contactor
 * ============================================================================ */

void Contactor::update(bool coil_powered, uint32_t now_ms, const PlantParams &p)
{
  if (coil_powered != coil_)
  {
    coil_ = coil_powered;
    coil_change_ms_ = now_ms;
  }

  switch (fault)
  {
  case Fault::WeldedClosed:
    closed_ = true;
    return;
  case Fault::StuckOpen:
    closed_ = false;
    return;
  case Fault::None:
    break;
  }

  uint32_t delay = coil_ ? p.contactor_close_ms : p.contactor_open_ms;
  if (closed_ != coil_ && (now_ms - coil_change_ms_) >= delay)
  {
    closed_ = coil_;
  }
}

/* ============================================================================
 * Plant
 * ============================================================================ */

Plant::Plant(const PlantParams &p)
    : params(p), soc(p.cells, p.initial_soc), temp_c(p.cells, p.ambient_c), cell_v_offset(p.cells, 0.0)
{
}

double Plant::ocv(double s)
{
  s = std::clamp(s, 0.0, 1.0);
  constexpr size_t n = sizeof(kOcvTable) / sizeof(kOcvTable[0]);
  for (size_t i = 1; i < n; i++)
  {
    if (s <= kOcvTable[i][0])
    {
      double f = (s - kOcvTable[i - 1][0]) / (kOcvTable[i][0] - kOcvTable[i - 1][0]);
      return kOcvTable[i - 1][1] + f * (kOcvTable[i][1] - kOcvTable[i - 1][1]);
    }
  }
  return kOcvTable[n - 1][1];
}

bool Plant::loop_closed_upstream() const
{
  return external_loop_closed && (!params.imd_in_loop || imd_latch_);
}

bool Plant::loop_powers_contactors() const
{
  return loop_closed_upstream() && out.bms_shutdown_relay;
}

double Plant::cell_terminal_v(int i) const
{
  return ocv(soc[i]) - current_a_ * params.cell_resistance_ohm + cell_v_offset[i];
}

double Plant::pack_ocv() const
{
  double v = 0.0;
  for (int i = 0; i < params.cells; i++)
  {
    v += ocv(soc[i]);
  }
  return v;
}

double Plant::pack_terminal_v() const
{
  double v = 0.0;
  for (int i = 0; i < params.cells; i++)
  {
    v += cell_terminal_v(i);
  }
  return v;
}

double Plant::max_cell_temp() const
{
  return *std::max_element(temp_c.begin(), temp_c.end());
}

double Plant::avg_soc() const
{
  double s = 0.0;
  for (double x : soc)
  {
    s += x;
  }
  return s / static_cast<double>(soc.size());
}

void Plant::tick(uint32_t now_ms)
{
  constexpr double dt = 0.001;

  /* IMD latch trips immediately on an isolation fault; only a manual reset
   * (imd_reset) sets it again. */
  if (isolation_fault)
  {
    imd_latch_ = false;
  }

  bool loop_power = loop_powers_contactors();
  air_minus.update(loop_power, now_ms, params);
  air_plus.update(loop_power && out.air_plus_cmd, now_ms, params);
  precharge.update(loop_power && out.precharge_cmd, now_ms, params);

  bool hv_connected = air_minus.closed() && air_plus.closed();
  double i = 0.0;
  if (hv_connected)
  {
    i = load_current_a;
    if (charger.present && charger.commanded_on && !charger.hw_failure)
    {
      i -= charger.current_a;
    }
  }
  current_a_ = i;

  step_cells(dt);
  step_bus(dt);
  step_can(now_ms);
}

void Plant::step_cells(double dt)
{
  const double dsoc = current_a_ * dt / (params.cell_capacity_ah * 3600.0);
  const double heat_w = current_a_ * current_a_ * params.cell_resistance_ohm;
  for (int c = 0; c < params.cells; c++)
  {
    soc[c] = std::clamp(soc[c] - dsoc, 0.0, 1.0);
    double cooling_w = params.cell_cooling_w_per_k * (temp_c[c] - params.ambient_c);
    temp_c[c] += (heat_w - cooling_w) * dt / params.cell_thermal_mass_j_per_k;
  }
}

void Plant::step_bus(double dt)
{
  double pack_v = pack_terminal_v();
  if (air_minus.closed() && air_plus.closed())
  {
    bus_v_ = pack_v;
  }
  else if (air_minus.closed() && precharge.closed() && !precharge_resistor_open)
  {
    double tau = precharge_resistor_shorted ? 0.02 : params.precharge_tau_s;
    bus_v_ += (pack_v - bus_v_) * (1.0 - std::exp(-dt / tau));
  }
  else
  {
    bus_v_ *= std::exp(-dt / params.bus_bleed_tau_s);
  }
}

void Plant::step_can(uint32_t now_ms)
{
  if (ivt_alive && now_ms % params.ivt_period_ms == 0)
  {
    ivt_last_rx = now_ms;
    ivt_voltage_v = static_cast<float>(bus_v_);
    ivt_current_a = static_cast<float>(current_a_);
  }
  if (dash_alive && now_ms % params.dash_period_ms == 0)
  {
    dash_last_rx = now_ms;
    dash_r2d_rx = r2d;
  }
  if (others_on_can && now_ms % params.heartbeat_period_ms == 0)
  {
    heartbeat_last_rx = now_ms;
  }
  if (charger.present && now_ms % params.charger_period_ms == 0)
  {
    charger.last_rx_tick = now_ms;
  }
}

void Plant::afe_task(uint32_t now_ms)
{
  if (!afe.alive || (now_ms - last_scan_ms_) < params.afe_scan_period_ms)
  {
    return;
  }
  last_scan_ms_ = now_ms;

  double total = 0.0;
  double vmax = -1e9;
  double vmin = 1e9;
  for (int c = 0; c < params.cells; c++)
  {
    double v = cell_terminal_v(c);
    total += v;
    vmax = std::max(vmax, v);
    vmin = std::min(vmin, v);
  }
  double tmax = max_cell_temp();

  afe.snapshot_total_v = static_cast<float>(total);
  afe.snapshot_max_cell_v = static_cast<float>(vmax);
  afe.snapshot_max_temp_c = static_cast<float>(tmax);
  afe.cell_delta_mv = static_cast<float>((vmax - vmin) * 1000.0);

  /* Validation, mirroring the firmware's consecutive-scan debounce. */
  bool v_bad = vmax * 1000.0 > FEB_CELL_MAX_VOLTAGE_MV || vmin * 1000.0 < FEB_CELL_MIN_VOLTAGE_MV;
  afe.voltage_violation_scans = v_bad ? afe.voltage_violation_scans + 1 : 0;
  if (afe.voltage_violation_scans >= FEB_VOLTAGE_ERROR_THRESH)
  {
    afe.fault_flags |= ADBMS_FAULT_FLAG_VOLTAGE;
  }

  double t_max_limit = (afe.profile == FEB_VALIDATION_PROFILE_CHARGING) ? FEB_CONFIG_CELL_HARD_MAX_TEMP_dC / 10.0
                                                                        : FEB_CELL_MAX_TEMP_DC / 10.0;
  bool t_bad = tmax > t_max_limit;
  afe.temp_violation_scans = t_bad ? afe.temp_violation_scans + 1 : 0;
  if (afe.temp_violation_scans >= FEB_TEMP_ERROR_THRESH)
  {
    afe.fault_flags |= ADBMS_FAULT_FLAG_TEMP;
  }

  afe.last_update_tick = now_ms;
}

} // namespace sim
