/**
 ******************************************************************************
 * @file           : PCU_CAN_Publish.cpp
 * @brief          : Everything PCU transmits
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "PCU_CAN.h"
#include "PCU_ADC.hpp"
#include "cmsis_os2.h"
#include "feb_can_publisher.hpp"
#include "feb_can_subscriber.hpp"

namespace fc = feb::can;
namespace fm = feb::can::msg;

namespace
{
bool fill_positions_accel(feb_can_pcu_raw_acc_t &m)
{

  m.acc0 = apps1.get_percentage();
  m.acc1 = apps2.get_percentage();
  m.accel = (m.acc0 + m.acc1) / 2.0f;
  m.plausible = 0;
  m.open_circuit = 0;
  m.short_circuit = 0;
  return true;
}

bool fill_positions_brake(feb_can_brake_t &m)
{
  m.brake1_pct = brake1.get_percentage();
  m.brake2_pct = brake2.get_percentage();
  m.brake_position = (m.brake1_pct + m.brake2_pct) / 2.0f;
  m.plausible = 0;
  m.brake_pressed = 0;
  m.bots_active = 0;
  m.brake_switch = 0;
  return true;
}

bool fill_bspd(feb_can_bspd_state &m)
{
  m.bspd_state = 0;
  return true;
}

bool fill_heartbeat(feb_can_pcu_heartbeat_t &m)
{
  return true;
}

bool fill_pedal_voltages(feb_can_pcu_pedal_voltages_t &m)
{
  m.acc1_mv = apps1.getRaw();
  m.acc2_mv = apps2.getRaw();
  m.brake1_mv = brake1.getRaw();
  m.brake2_mv = brake2.getRaw();
  return true;
}

fc::Publisher<fm::PcuRawAcc> positions_accel_tx{fill_positions_accel};
fc::Publisher<fm::Brake> positions_brake_tx{fill_positions_brake};
fc::Publisher<fm::BspdState> bspd_tx{fill_bspd};
fc::Publisher<fm::PcuHeartbeat> heartbeat_tx{fill_heartbeat};
fc::Publisher<fm::PcuPedalVoltages> pedal_voltages_tx{fill_pedal_voltages};
} // namespace
