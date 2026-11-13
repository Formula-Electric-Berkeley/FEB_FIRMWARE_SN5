/**
 ******************************************************************************
 * @file           : feb_can_scheduler.hpp
 * @brief          : Periodic TX scheduling for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_SCHEDULER_HPP
#define FEB_CAN_SCHEDULER_HPP

#include <cstdint>

namespace feb::can
{
inline constexpr std::uint32_t kAutoOffset = 0xFFFFFFFFu;

struct Entry
{
  Entry *next = nullptr;
  std::uint32_t frame_id = 0;
  std::uint32_t period_ms = 0;
  std::uint32_t offset_ms = 0;
  std::uint32_t next_due_ms = 0;
  std::uint32_t requested_offset = kAutoOffset;
  std::uint32_t tx_count = 0;
  std::uint32_t skip_count = 0;
  std::uint32_t error_count = 0;
  bool linked = false;
  bool (*emit)(Entry *) = nullptr;
};

class Scheduler
{
public:
  static void add(Entry *e);
  static void remove(Entry *e);

  static void tick(std::uint32_t now_ms);

  static void restart(std::uint32_t now_ms);

  static void set_gate(bool (*gate)());

  static void for_each(void (*fn)(const Entry &, void *), void *ctx);
  static std::uint32_t count();

private:
  static void rebalance();
};

}  // namespace feb::can

#endif  /* FEB_CAN_SCHEDULER_HPP */
