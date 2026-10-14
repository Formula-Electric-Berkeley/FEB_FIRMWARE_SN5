/**
 * @file sim.cpp
 * @brief Simulated clock, logger and task scheduler.
 */
#include "sim.hpp"

#include "FEB_CAN_Charger.h"
#include "FEB_SM.h"

#include <cstdarg>

namespace sim
{

namespace
{
/* Start at 1 ms: tick 0 doubles as "never" in several firmware timestamps. */
uint64_t g_now_us = 1000;
FEB_Log_Level_t g_log_level = FEB_LOG_INFO;
std::vector<StateChange> g_history;

constexpr const char *kStateNames[BMS_STATE_COUNT] = {
    "BOOT",         "LV_POWER",          "BUS_HEALTH_CHECK", "PRECHARGE",       "ENERGIZED",
    "DRIVE",        "BATTERY_FREE",      "CHARGER_PRECHARGE", "CHARGING",       "BALANCE",
    "FAULT_BMS",    "FAULT_BSPD",        "FAULT_IMD",        "FAULT_CHARGING",
};
} // namespace

uint64_t now_us()
{
  return g_now_us;
}

uint32_t now_ms()
{
  return static_cast<uint32_t>(g_now_us / 1000);
}

void advance_us(uint64_t us)
{
  g_now_us += us;
}

void set_log_level(FEB_Log_Level_t level)
{
  g_log_level = level;
}

void record_state(BMS_State_t state)
{
  if (g_history.empty() || g_history.back().state != state)
  {
    g_history.push_back({now_ms(), state});
  }
}

const std::vector<StateChange> &state_history()
{
  return g_history;
}

const char *state_name(BMS_State_t state)
{
  return state < BMS_STATE_COUNT ? kStateNames[state] : "UNKNOWN";
}

/* ============================================================================
 * Sim
 * ============================================================================ */

Sim::Sim(const PlantParams &params)
{
  reset_plant(params);
}

void Sim::boot()
{
  FEB_SM_Init();
}

void Sim::step()
{
  /* Move to the next 1 ms boundary. A busy-wait inside the previous
   * FEB_SM_Process() may already have consumed part of this millisecond. */
  g_now_us = (g_now_us / 1000 + 1) * 1000;
  uint32_t t = now_ms();

  Plant &p = plant();
  p.tick(t);

  if (t % 10 == 0)
  {
    p.afe_task(t);
  }

  FEB_SM_Process();

  /* StartSMTask's 100 ms divider */
  if (t % 100 == 0)
  {
    FEB_CAN_Charger_Process();
  }

  if (trace_ && t % trace_period_ms_ == 0)
  {
    trace_row();
  }
}

void Sim::run_ms(uint32_t ms)
{
  for (uint32_t i = 0; i < ms; i++)
  {
    step();
  }
}

bool Sim::run_until(const std::function<bool()> &pred, uint32_t timeout_ms)
{
  for (uint32_t i = 0; i < timeout_ms; i++)
  {
    if (pred())
    {
      return true;
    }
    step();
  }
  return pred();
}

bool Sim::run_until_state(BMS_State_t state, uint32_t timeout_ms)
{
  return run_until([state] { return FEB_SM_Get_Current_State() == state; }, timeout_ms);
}

BMS_State_t Sim::state() const
{
  return FEB_SM_Get_Current_State();
}

Plant &Sim::plant() const
{
  return sim::plant();
}

void Sim::set_trace(std::FILE *csv, uint32_t period_ms)
{
  trace_ = csv;
  trace_period_ms_ = period_ms ? period_ms : 1;
  if (trace_)
  {
    std::fprintf(trace_, "t_ms,state,pack_v,bus_v,current_a,avg_soc,max_temp_c,"
                         "loop,air_minus,air_plus,precharge,bms_relay,imd_latch\n");
  }
}

void Sim::trace_row()
{
  Plant &p = plant();
  std::fprintf(trace_, "%u,%s,%.2f,%.2f,%.2f,%.4f,%.2f,%d,%d,%d,%d,%d,%d\n", now_ms(), state_name(state()),
               p.pack_terminal_v(), p.bus_v(), p.hv_current_a(), p.avg_soc(), p.max_cell_temp(),
               p.loop_closed_upstream(), p.air_minus.closed(), p.air_plus.closed(), p.precharge.closed(),
               p.out.bms_shutdown_relay, p.imd_latch());
}

} // namespace sim

/* ============================================================================
 * Logger + clock C entry points used by the shims
 * ============================================================================ */

extern "C" void Sim_Log(FEB_Log_Level_t level, const char *tag, const char *format, ...)
{
  if (level > sim::g_log_level)
  {
    return;
  }
  static const char kLevel[] = "-EWIDT";
  std::printf("[%9.3f s] %c %s ", static_cast<double>(sim::g_now_us) / 1e6, kLevel[level], tag);
  va_list args;
  va_start(args, format);
  std::vprintf(format, args);
  va_end(args);
  std::printf("\n");
}

extern "C" uint32_t HAL_GetTick(void)
{
  return sim::now_ms();
}
