/**
 * @file scenario.cpp
 * @brief Check helpers and the scenario registry.
 */
#include "scenario.hpp"

#include <cstdarg>

namespace sim
{

std::vector<Scenario> &scenarios()
{
  static std::vector<Scenario> registry;
  return registry;
}

void Check::expect(bool cond, const char *fmt, ...)
{
  if (cond)
  {
    return;
  }
  char msg[256];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  char line[320];
  std::snprintf(line, sizeof(line), "t=%u ms: %s", now_ms(), msg);
  failures_.emplace_back(line);
  std::printf("  !! FAIL %s\n", line);
}

void Check::expect_state(const Sim &s, BMS_State_t state)
{
  expect(s.state() == state, "expected state %s, got %s", state_name(state), state_name(s.state()));
}

bool Check::expect_reaches(Sim &s, BMS_State_t state, uint32_t timeout_ms)
{
  bool ok = s.run_until_state(state, timeout_ms);
  expect(ok, "did not reach %s within %u ms (stuck in %s)", state_name(state), timeout_ms, state_name(s.state()));
  return ok;
}

void Check::expect_holds(Sim &s, BMS_State_t state, uint32_t duration_ms)
{
  for (uint32_t i = 0; i < duration_ms; i++)
  {
    if (s.state() != state)
    {
      expect(false, "expected to hold %s for %u ms, left for %s after %u ms", state_name(state), duration_ms,
             state_name(s.state()), i);
      return;
    }
    s.step();
  }
  expect_state(s, state);
}

void Check::expect_visited(BMS_State_t state)
{
  for (const StateChange &sc : state_history())
  {
    if (sc.state == state)
    {
      return;
    }
  }
  expect(false, "state %s was never entered", state_name(state));
}

} // namespace sim
