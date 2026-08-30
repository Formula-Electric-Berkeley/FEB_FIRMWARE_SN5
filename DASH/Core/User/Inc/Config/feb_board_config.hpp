/**
 ******************************************************************************
 * @file           : feb_board_config.hpp
 * @brief          : DASH board contract for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_BOARD_HPP
#define FEB_CAN_BOARD_HPP

#include "feb_can_lib.h"
#include "feb_can_traits.hpp"

#include <cstddef>

namespace feb::can
{
inline constexpr Node kThisNode = Node::kDash;

inline constexpr FEB_CAN_Instance_t kVehicleBus = FEB_CAN_INSTANCE_2;

inline constexpr std::size_t kMaxPayload = 8;

} // namespace feb::can

#endif /* FEB_CAN_BOARD_HPP */
