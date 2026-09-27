/**
 ******************************************************************************
 * @file           : feb_board_config.hpp
 * @brief          : Sensor Node contract for the FEB C++ CAN library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * This header is included by the C++ CAN library (feb_can_tasks.hpp,
 * feb_can_publisher.hpp, feb_can_subscriber.hpp) and must define three
 * symbols in the `feb::can` namespace:
 *
 *   kThisNode    — which node this firmware represents (filters TX static_asserts)
 *   kVehicleBus  — which CAN peripheral carries vehicle traffic
 *   kMaxPayload  — maximum DLC for compile-time buffer sizing
 *
 * The Sensor Node builds two variants from the same source tree (FRONT and
 * REAR); the variant is selected by FEB_SENSOR_NODE_VARIANT, set by CMake.
 */

#ifndef FEB_CAN_BOARD_HPP
#define FEB_CAN_BOARD_HPP

#include "FEB_SN_Config.h"    // FEB_SN_VARIANT_FRONT / FEB_SN_VARIANT_REAR / FEB_SENSOR_NODE_VARIANT
#include "feb_can_lib.h"      // FEB_CAN_Instance_t, FEB_CAN_INSTANCE_*
#include "feb_can_traits.hpp" // Node enum

#include <cstddef>

namespace feb::can
{

#if FEB_SENSOR_NODE_VARIANT == FEB_SN_VARIANT_FRONT
inline constexpr Node kThisNode = Node::kSnFront;
#elif FEB_SENSOR_NODE_VARIANT == FEB_SN_VARIANT_REAR
inline constexpr Node kThisNode = Node::kSnRear;
#else
#error "FEB_SENSOR_NODE_VARIANT must be FEB_SN_VARIANT_FRONT or FEB_SN_VARIANT_REAR"
#endif

inline constexpr FEB_CAN_Instance_t kVehicleBus = FEB_CAN_INSTANCE_1;
inline constexpr std::size_t kMaxPayload = 8;

} // namespace feb::can

#endif /* FEB_CAN_BOARD_HPP */
