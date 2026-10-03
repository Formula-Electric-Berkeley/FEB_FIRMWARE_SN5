#pragma once

#include "FEB_SN_Config.h"
#include "feb_can_traits.hpp"

namespace feb::sn::msg
{
namespace fm = feb::can::msg;

#if FEB_SN_IS_FRONT()
using ImuAccel = fm::ImuAccelFront;
using ImuGyro = fm::ImuGyroFront;
using Mag = fm::MagFront;
using Wss = fm::WssFront;
using Linpot = fm::LinpotFront;
using Thermistor = fm::ThermistorFront;
using GpsPos = fm::GpsPosFront;
using GpsAltitude = fm::GpsAltitudeFront;
using GpsMotion = fm::GpsMotionFront;
using GpsTime = fm::GpsTimeFront;
using GpsDate = fm::GpsDateFront;
using GpsStatus = fm::GpsStatusFront;
using FusionQuat = fm::FusionQuatFront;
using FusionEuler = fm::FusionEulerFront;
using FusionLinAccel = fm::FusionLinAccelFront;
using FusionEarthAccel = fm::FusionEarthAccelFront;
using FusionStatus = fm::FusionStatusFront;
using StrainGauge = fm::StrainGaugeFront;
#else
using ImuAccel = fm::ImuAccelRear;
using ImuGyro = fm::ImuGyroRear;
using Mag = fm::MagRear;
using Wss = fm::WssRear;
using Linpot = fm::LinpotRear;
using Thermistor = fm::ThermistorRear;
using GpsPos = fm::GpsPosRear;
using GpsAltitude = fm::GpsAltitudeRear;
using GpsMotion = fm::GpsMotionRear;
using GpsTime = fm::GpsTimeRear;
using GpsDate = fm::GpsDateRear;
using GpsStatus = fm::GpsStatusRear;
using FusionQuat = fm::FusionQuatRear;
using FusionEuler = fm::FusionEulerRear;
using FusionLinAccel = fm::FusionLinAccelRear;
using FusionEarthAccel = fm::FusionEarthAccelRear;
using FusionStatus = fm::FusionStatusRear;
using StrainGauge = fm::StrainGaugeRear;
#endif
} // namespace feb::sn::msg
