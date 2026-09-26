#ifndef SN_CONFIG_MESSAGES_HPP
#define SN_CONFIG_MESSAGES_HPP

#include "FEB_SN_Config.h"
#include "feb_can_traits.hpp"

namespace feb::sn::msg
{
namespace fm = feb::can::msg;

#if FEB_SN_IS_FRONT()
using ImuAccel = fm::ImuAccelerationData;
using ImuGyro = fm::ImuGyroData;
using Mag = fm::MagnetometerData;
using Wss = fm::WssFrontData;
using Linpot = fm::LinearPotentiometerFront;
#else
using ImuAccel = fm::ImuAccelerationDataRear;
using ImuGyro = fm::ImuGyroDataRear;
using Mag = fm::MagnetometerDataRear;
using Wss = fm::WssRearData;
using Linpot = fm::LinearPotentiometerRear;
#endif

using SensorTemps = fm::SensorTempsData;
} // namespace feb::sn::msg

#endif /* SN_CONFIG_MESSAGES_HPP */
