/**
 * @file scenario.hpp
 * @brief Scenario registry and assertion helpers.
 *
 * A scenario is a scripted drive/charge session against a fresh firmware
 * image. The firmware keeps its state in file-scope statics, so each scenario
 * runs in its own process (main.cpp forks one per scenario).
 *
 *   SIM_SCENARIO(precharge_too_fast, "Shorted precharge resistor -> FAULT_BMS")
 *   {
 *     s.plant().precharge_resistor_shorted = true;
 *     ...
 *     c.expect_reaches(s, BMS_STATE_FAULT_BMS, 3000);
 *   }
 */
#pragma once

#include "sim.hpp"

#include <string>
#include <vector>

namespace sim
{

class Check
{
public:
  void expect(bool cond, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

  /** Current state must be `state` right now. */
  void expect_state(const Sim &s, BMS_State_t state);

  /** Step until `state` is reached; fail if it is not within timeout_ms. */
  bool expect_reaches(Sim &s, BMS_State_t state, uint32_t timeout_ms);

  /** Step for duration_ms; fail if the state ever differs from `state`. */
  void expect_holds(Sim &s, BMS_State_t state, uint32_t duration_ms);

  /** The state history must contain `state` at some point. */
  void expect_visited(BMS_State_t state);

  bool passed() const
  {
    return failures_.empty();
  }
  const std::vector<std::string> &failures() const
  {
    return failures_;
  }

private:
  std::vector<std::string> failures_;
};

using ScenarioFn = void (*)(Sim &, Check &);

struct Scenario
{
  const char *name;
  const char *summary;
  ScenarioFn run;
};

std::vector<Scenario> &scenarios();

struct Registrar
{
  Registrar(const char *name, const char *summary, ScenarioFn fn)
  {
    scenarios().push_back({name, summary, fn});
  }
};

} // namespace sim

#define SIM_SCENARIO(id, summary)                                                                                      \
  static void id(sim::Sim &s, sim::Check &c);                                                                          \
  static const sim::Registrar id##_registrar{#id, summary, id};                                                        \
  static void id([[maybe_unused]] sim::Sim &s, [[maybe_unused]] sim::Check &c)
