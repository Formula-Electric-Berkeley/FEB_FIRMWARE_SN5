/**
 ******************************************************************************
 * @file           : LVPDB_CAN_Publish.cpp
 * @brief          : Everything LVPDB transmits
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "LVPDB_CAN.h"
#include "LVPDB_TPS.h"
#include "cmsis_os2.h"
#include "feb_can_publisher.hpp"
#include "feb_can_subscriber.hpp"

namespace fc = feb::can;
namespace fm = feb::can::msg;

extern osMutexId_t tpsDataMutexHandle;

namespace
{

bool fill_voltages(feb_can_lvpdb_lv_24v_bus_and_12v_bus_voltages_t &m)
{
  if (!LVPDB_TPS_IsInitialized())
  {
    return false;
  }

  osMutexAcquire(tpsDataMutexHandle, osWaitForever);
  m.lv_24v_voltage = tps2482_bus_voltage[0];
  m.lv_12v_voltage = tps2482_bus_voltage[3];
  osMutexRelease(tpsDataMutexHandle);
  return true;
}

bool fill_lv_sh_lt_bm_l_currents(feb_can_lvpdb_lv_sh_lt_bm_l_currents_t &m)
{
  if (!LVPDB_TPS_IsInitialized())
  {
    return false;
  }

  osMutexAcquire(tpsDataMutexHandle, osWaitForever);
  m.lv_current = (uint16_t)tps2482_current[0];
  m.sh_current = (uint16_t)tps2482_current[1];
  m.lt_current = (uint16_t)tps2482_current[2];
  m.bm_l_current = (uint16_t)tps2482_current[3];
  osMutexRelease(tpsDataMutexHandle);
  return true;
}

bool fill_sm_af1_af2_cp_rf_currents(feb_can_lvpdb_sm_af1_af2_cp_rf_currents_t &m)
{
  if (!LVPDB_TPS_IsInitialized())
  {
    return false;
  }

  osMutexAcquire(tpsDataMutexHandle, osWaitForever);
  m.sm_current = (uint16_t)tps2482_current[4];
  m.af1_af2_current = (uint16_t)tps2482_current[5];
  m.cp_rf_current = (uint16_t)tps2482_current[6];
  osMutexRelease(tpsDataMutexHandle);
  return true;
}

bool fill_heartbeat(feb_can_lvpdb_heartbeat_t &m)
{
  m.tps_init_failed = !LVPDB_TPS_IsInitialized();

  osMutexAcquire(tpsDataMutexHandle, osWaitForever);

  m.tps_lv_poll_failed = !LVPDB_TPS_PollOk(0);
  m.tps_sh_poll_failed = !LVPDB_TPS_PollOk(1);
  m.tps_lt_poll_failed = !LVPDB_TPS_PollOk(2);
  m.tps_bm_l_poll_failed = !LVPDB_TPS_PollOk(3);
  m.tps_sm_poll_failed = !LVPDB_TPS_PollOk(4);
  m.tps_af1_af2_poll_failed = !LVPDB_TPS_PollOk(5);
  m.tps_cp_rf_poll_failed = !LVPDB_TPS_PollOk(6);

  m.tps_lv_power_not_good = !LVPDB_TPS_PowerGood(0);
  m.tps_sh_power_not_good = !LVPDB_TPS_PowerGood(1);
  m.tps_lt_power_not_good = !LVPDB_TPS_PowerGood(2);
  m.tps_bm_l_power_not_good = !LVPDB_TPS_PowerGood(3);
  m.tps_sm_power_not_good = !LVPDB_TPS_PowerGood(4);
  m.tps_af1_af2_power_not_good = !LVPDB_TPS_PowerGood(5);
  m.tps_cp_rf_power_not_good = !LVPDB_TPS_PowerGood(6);

  osMutexRelease(tpsDataMutexHandle);

  m.dash_state_stale = fc::rx<fm::DashState>.age_ms() >= 250;
  return true;
}

fc::Publisher<fm::LvpdbLv24vBusAnd12vBusVoltages> voltages_tx{fill_voltages, 99};
fc::Publisher<fm::LvpdbLvShLtBmLCurrents> lv_sh_lt_bm_l_currents_tx{fill_lv_sh_lt_bm_l_currents, 99};
fc::Publisher<fm::LvpdbSmAf1Af2CpRfCurrents> sm_af1_af2_cp_rf_currents_tx{fill_sm_af1_af2_cp_rf_currents, 99};

// not 100ms to offset message from other two statuses (ran into issue where mailbox got full)
fc::Publisher<fm::LvpdbHeartbeat> heartbeat_tx{fill_heartbeat, 67};

} // namespace
