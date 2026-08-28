/**
 ******************************************************************************
 * @file           : feb_can_rx_registry.cpp
 * @brief          : Subscriber registry for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_rx_registry.hpp"

#include "feb_can_lib.h"

#if FEB_CAN_USE_FREERTOS
#include "cmsis_os2.h"
#endif

namespace feb::can
{
namespace
{
RxNode *head = nullptr;
std::uint32_t node_count = 0;
}  // namespace

namespace
{
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
}  // namespace

void RxRegistry::link(RxNode *n)
{
  if (n == nullptr || n->linked)
  {
    return;
  }
  Lock guard;
  n->next = head;
  head = n;
  n->linked = true;
  node_count++;
}

void RxRegistry::unlink(RxNode *n)
{
  if (n == nullptr || !n->linked)
  {
    return;
  }
  Lock guard;
  for (RxNode **slot = &head; *slot != nullptr; slot = &(*slot)->next)
  {
    if (*slot == n)
    {
      *slot = n->next;
      n->next = nullptr;
      n->linked = false;
      node_count--;
      break;
    }
  }
  if (n->handle >= 0)
  {
    FEB_CAN_RX_Unregister(n->handle);
    n->handle = -1;
  }
}

void RxRegistry::attach_all()
{
  for (RxNode *n = head; n != nullptr; n = n->next)
  {
    if (n->attach != nullptr)
    {
      n->attach(n);
    }
  }
}

void RxRegistry::for_each(void (*fn)(const RxNode &, void *), void *ctx)
{
  if (fn == nullptr)
  {
    return;
  }
  Lock guard;
  for (const RxNode *n = head; n != nullptr; n = n->next)
  {
    fn(*n, ctx);
  }
}

std::uint32_t RxRegistry::count()
{
  return node_count;
}

}  // namespace feb::can
