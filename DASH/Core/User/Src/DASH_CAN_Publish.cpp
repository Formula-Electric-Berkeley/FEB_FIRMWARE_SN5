/**
 ******************************************************************************
 * @file           : DASH_CAN_Publish.cpp
 * @brief          : Everything DASH transmits, and the task that drives it
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DASH_CAN.h"
#include "DASH_IO.h"
#include "DASH_RTD.h"
#include "feb_can_publisher.hpp"

namespace fc = feb::can;
namespace fm = feb::can::msg;

namespace
{
bool fill_dash_state(feb_can_dash_state_t &m)
{
  const IO_States_t io = FEB_IO_GetLastIOStates();
  m.button1 = io.button_rtd;
  m.button2 = io.button_2;
  m.button3 = io.button_3;
  m.button4 = io.button_4;
  m.switch1 = io.switch_accumulator_fans;
  m.switch2 = io.switch_coolant_pump_radiator_fan;
  m.switch3 = io.switch_logging;
  m.switch4 = io.switch_4;
  m.buzzer = io.buzzer_enabled;
  m.ready_to_drive = FEB_State_GetLastRTD();
  return true;
}

bool fill_dash_heartbeat(feb_can_dash_heartbeat_t &m)
{
  m.io_expander_error = !FEB_IO_StatusOk();
  return true;
}

fc::Publisher<fm::DashState> state_tx{fill_dash_state};
fc::Publisher<fm::DashHeartbeat> heartbeat_tx{fill_dash_heartbeat};

} // namespace
