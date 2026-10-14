/**
 * @file sim.hpp
 * @brief BMS ECU simulator harness: owns simulated time and runs the firmware
 *        tasks at their real cadence against the plant model.
 *
 * Task schedule (mirrors FEB_Main.cpp / FEB_Task_ADBMS.cpp):
 *  - SM task:    FEB_SM_Process() every 1 ms
 *  - ADBMS task: every 10 ms, voltage/temperature scan every 100 ms
 *
 * Simulated time is in microseconds. HAL_GetTick() is now_us / 1000. Each
 * FEB_Time_Us() call advances time by 1 us so the firmware's busy-wait loops
 * (shutdown_trip_confirmed) make progress and see time-varying inputs.
 */
#pragma once

#include "FEB_CAN_State.h"
#include "feb_log.h"
#include "plant.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

namespace sim
{

uint64_t now_us();
uint32_t now_ms();
void advance_us(uint64_t us);

void set_log_level(FEB_Log_Level_t level);

struct StateChange
{
  uint32_t t_ms;
  BMS_State_t state;
};

/** Called by the FEB_CAN_State_SetState binding on every state publish. */
void record_state(BMS_State_t state);
const std::vector<StateChange> &state_history();

const char *state_name(BMS_State_t state);

class Sim
{
public:
  explicit Sim(const PlantParams &params = {});

  /** FEB_SM_Init(), as the firmware does once before the scheduler starts. */
  void boot();

  /** Advance one 1 ms SM tick: plant physics, ADBMS task slot, SM process. */
  void step();

  void run_ms(uint32_t ms);

  /** Step until pred() holds or timeout_ms elapses. Returns pred(). */
  bool run_until(const std::function<bool()> &pred, uint32_t timeout_ms);
  bool run_until_state(BMS_State_t state, uint32_t timeout_ms);

  BMS_State_t state() const;
  Plant &plant() const;

  /** Write one CSV row every `period_ms` while stepping (nullptr = off). */
  void set_trace(std::FILE *csv, uint32_t period_ms = 10);

private:
  void trace_row();

  std::FILE *trace_ = nullptr;
  uint32_t trace_period_ms_ = 10;
};

} // namespace sim
