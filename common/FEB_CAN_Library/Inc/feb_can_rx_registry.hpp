/**
 ******************************************************************************
 * @file           : feb_can_rx_registry.hpp
 * @brief          : Subscriber registry for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_RX_REGISTRY_HPP
#define FEB_CAN_RX_REGISTRY_HPP

#include <cstdint>

namespace feb::can
{
struct RxNode
{
  RxNode *next = nullptr;
  std::uint32_t frame_id = 0;
  std::int32_t handle = -1;
  bool linked = false;
  bool extended = false;
  void (*attach)(RxNode *) = nullptr;
};

class RxRegistry
{
public:
  static void link(RxNode *n);
  static void unlink(RxNode *n);

  static void attach_all();

  static void for_each(void (*fn)(const RxNode &, void *), void *ctx);
  static std::uint32_t count();
};

}  // namespace feb::can

#endif  /* FEB_CAN_RX_REGISTRY_HPP */
