/**
 ******************************************************************************
 * @file           : feb_can_scheduler.cpp
 * @brief          : Periodic TX scheduling for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_scheduler.hpp"

#include "feb_can_lib.h"

#if FEB_CAN_USE_FREERTOS
#include "cmsis_os2.h"
#endif

namespace feb::can
{
namespace
{
Entry *head = nullptr;
std::uint32_t entry_count = 0;
std::uint32_t origin_ms = 0;
bool (*gate_fn)() = nullptr;
bool gate_open = false;

class Lock
{
public:
#if FEB_CAN_USE_FREERTOS
  Lock() : held_(osKernelGetState() == osKernelRunning)
  {
    if (held_)
    {
      osKernelLock();
    }
  }
  ~Lock()
  {
    if (held_)
    {
      osKernelUnlock();
    }
  }
#else
  Lock() = default;
  ~Lock() = default;
#endif
  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;

#if FEB_CAN_USE_FREERTOS
private:
  bool held_;
#endif
};

bool orders_before(const Entry *a, const Entry *b)
{
  if (a->period_ms != b->period_ms)
  {
    return a->period_ms < b->period_ms;
  }
  return a->frame_id < b->frame_id;
}

std::uint32_t next_slot(const Entry *e, std::uint32_t now_ms)
{
  const std::uint32_t base = origin_ms + e->offset_ms;
  if (e->period_ms == 0 || (std::int32_t)(base - now_ms) >= 0)
  {
    return base;
  }
  const std::uint32_t late = now_ms - base;
  return base + ((late / e->period_ms) + 1u) * e->period_ms;
}
}  // namespace

void Scheduler::add(Entry *e)
{
  if (e == nullptr || e->linked)
  {
    return;
  }

  Lock guard;

  Entry **slot = &head;
  while (*slot != nullptr && orders_before(*slot, e))
  {
    slot = &(*slot)->next;
  }
  e->next = *slot;
  *slot = e;
  e->linked = true;
  entry_count++;

  rebalance();
}

void Scheduler::remove(Entry *e)
{
  if (e == nullptr || !e->linked)
  {
    return;
  }

  Lock guard;

  for (Entry **slot = &head; *slot != nullptr; slot = &(*slot)->next)
  {
    if (*slot == e)
    {
      *slot = e->next;
      e->next = nullptr;
      e->linked = false;
      entry_count--;
      break;
    }
  }

  rebalance();
}

void Scheduler::rebalance()
{
  const std::uint32_t now = FEB_CAN_Now();

  Entry *run = head;
  while (run != nullptr)
  {
    const std::uint32_t period = run->period_ms;

    std::uint32_t n = 0;
    for (Entry *e = run; e != nullptr && e->period_ms == period; e = e->next)
    {
      if (e->requested_offset == kAutoOffset)
      {
        n++;
      }
    }

    std::uint32_t i = 0;
    Entry *e = run;
    for (; e != nullptr && e->period_ms == period; e = e->next)
    {
      if (e->requested_offset != kAutoOffset)
      {
        e->offset_ms = period != 0 ? e->requested_offset % period : e->requested_offset;
      }
      else
      {
        e->offset_ms = (n != 0) ? (i * period) / n : 0;
        i++;
      }
      e->next_due_ms = next_slot(e, now);
    }
    run = e;
  }
}

void Scheduler::restart(std::uint32_t now_ms)
{
  Lock guard;

  origin_ms = now_ms;
  for (Entry *e = head; e != nullptr; e = e->next)
  {
    e->next_due_ms = now_ms + e->offset_ms;
  }
}

void Scheduler::tick(std::uint32_t now_ms)
{
  if (gate_fn != nullptr)
  {
    const bool open = gate_fn();
    if (!open)
    {
      gate_open = false;
      return;
    }

    if (!gate_open)
    {
      gate_open = true;
      restart(now_ms);
      return;
    }
  }

  for (Entry *e = head; e != nullptr; e = e->next)
  {
    if (e->period_ms == 0 || e->emit == nullptr)
    {
      continue;
    }

    if ((std::int32_t)(now_ms - e->next_due_ms) < 0)
    {
      continue;
    }

    e->emit(e);

    e->next_due_ms += e->period_ms;
    if ((std::int32_t)(now_ms - e->next_due_ms) >= 0)
    {
      e->next_due_ms = now_ms + e->period_ms;
    }
  }
}

void Scheduler::set_gate(bool (*gate)())
{
  gate_fn = gate;
}

void Scheduler::for_each(void (*fn)(const Entry &, void *), void *ctx)
{
  if (fn == nullptr)
  {
    return;
  }
  Lock guard;
  for (const Entry *e = head; e != nullptr; e = e->next)
  {
    fn(*e, ctx);
  }
}

std::uint32_t Scheduler::count()
{
  return entry_count;
}

}  // namespace feb::can
