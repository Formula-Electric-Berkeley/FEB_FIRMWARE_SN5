#ifndef FEB_RES_EBS_BMS_H
#define FEB_RES_EBS_BMS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * Mirrors the state ordering used in PCU/Core/User/Inc/FEB_CAN_BMS.h so
 * RES_EBS interprets BMS fault states the same way PCU does.
 */
typedef enum
{
  RES_EBS_BMS_STATE_BOOT = 0,
  RES_EBS_BMS_STATE_LV,
  RES_EBS_BMS_STATE_HEALTH_CHECK,
  RES_EBS_BMS_STATE_PRECHARGE,
  RES_EBS_BMS_STATE_ENERGIZED,
  RES_EBS_BMS_STATE_DRIVE,
  RES_EBS_BMS_STATE_FREE,
  RES_EBS_BMS_STATE_CHARGER_PRECHARGE,
  RES_EBS_BMS_STATE_CHARGING,
  RES_EBS_BMS_STATE_BALANCE,
  RES_EBS_BMS_STATE_FAULT_BMS,
  RES_EBS_BMS_STATE_FAULT_BSPD,
  RES_EBS_BMS_STATE_FAULT_IMD,
  RES_EBS_BMS_STATE_FAULT_CHARGING,
  RES_EBS_BMS_STATE_DEFAULT
} RES_EBS_BMS_State_t;

typedef struct
{
  bool bms_state_received;
  bool accumulator_faults_received;
  uint32_t last_bms_state_rx_ms;
  uint32_t last_accumulator_faults_rx_ms;
  RES_EBS_BMS_State_t bms_state;
  uint8_t ping_lv_nodes;
  uint8_t relay_state;
  uint8_t gpio_sense;
  bool bms_fault;
  bool imd_fault;
  bool critical_from_state;
  bool critical_from_fault_flags;
  bool critical_shutdown_active;
} RES_EBS_BMS_Status_t;

int RES_EBS_BMS_Init(void);
void RES_EBS_BMS_Tick(void);
void RES_EBS_BMS_GetStatus(RES_EBS_BMS_Status_t *status);
bool RES_EBS_BMS_CriticalShutdownActive(void);
bool RES_EBS_BMS_RelayEnableAllowed(void);
const char *RES_EBS_BMS_StateToString(RES_EBS_BMS_State_t state);

#ifdef __cplusplus
}
#endif

#endif /* FEB_RES_EBS_BMS_H */
