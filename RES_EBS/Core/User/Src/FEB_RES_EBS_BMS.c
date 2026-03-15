#include "FEB_RES_EBS_BMS.h"

#include "FEB_RES_EBS_Board.h"
#include "feb_can_lib.h"
#include "feb_uart_log.h"

#include <string.h>

#define RES_EBS_BMS_STATE_FRAME_LEN ((uint8_t)2U)
#define RES_EBS_BMS_FAULTS_FRAME_LEN ((uint8_t)1U)

typedef struct
{
  volatile bool bms_state_received;
  volatile bool accumulator_faults_received;
  volatile uint32_t last_bms_state_rx_ms;
  volatile uint32_t last_accumulator_faults_rx_ms;
  volatile uint8_t bms_state;
  volatile uint8_t ping_lv_nodes;
  volatile uint8_t relay_state;
  volatile uint8_t gpio_sense;
  volatile bool bms_fault;
  volatile bool imd_fault;
  volatile bool critical_from_state;
  volatile bool critical_from_fault_flags;
  volatile bool critical_shutdown_active;
  volatile bool state_changed;
  volatile bool faults_changed;
  bool last_logged_critical_shutdown_active;
  int32_t state_rx_handle;
  int32_t faults_rx_handle;
} RES_EBS_BMS_Runtime_t;

static RES_EBS_BMS_Runtime_t bms_runtime = {
    .state_rx_handle = -1,
    .faults_rx_handle = -1,
};

static bool bms_state_is_critical_fault(uint8_t state)
{
  switch ((RES_EBS_BMS_State_t)state)
  {
  case RES_EBS_BMS_STATE_FAULT_BMS:
  case RES_EBS_BMS_STATE_FAULT_BSPD:
  case RES_EBS_BMS_STATE_FAULT_IMD:
  case RES_EBS_BMS_STATE_FAULT_CHARGING:
    return true;

  default:
    return false;
  }
}

static void bms_update_critical_flags(void)
{
  bms_runtime.critical_from_state = bms_state_is_critical_fault(bms_runtime.bms_state);
  bms_runtime.critical_from_fault_flags = (bms_runtime.bms_fault || bms_runtime.imd_fault);
  bms_runtime.critical_shutdown_active = (bms_runtime.critical_from_state || bms_runtime.critical_from_fault_flags);
}

static void bms_state_rx_callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                  const uint8_t *data, uint8_t length, void *user_data)
{
  uint8_t new_bms_state;
  uint8_t new_ping_lv_nodes;
  uint8_t new_relay_state;
  uint8_t new_gpio_sense;

  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;

  if (data == NULL || length < RES_EBS_BMS_STATE_FRAME_LEN)
  {
    return;
  }

  new_bms_state = (uint8_t)(data[0] & 0x1FU);
  new_ping_lv_nodes = (uint8_t)((data[0] >> 5) & 0x07U);
  new_relay_state = (uint8_t)(data[1] & 0x07U);
  new_gpio_sense = (uint8_t)((data[1] >> 3) & 0x1FU);

  if (!bms_runtime.bms_state_received || new_bms_state != bms_runtime.bms_state ||
      new_ping_lv_nodes != bms_runtime.ping_lv_nodes || new_relay_state != bms_runtime.relay_state ||
      new_gpio_sense != bms_runtime.gpio_sense)
  {
    bms_runtime.state_changed = true;
  }

  bms_runtime.bms_state_received = true;
  bms_runtime.last_bms_state_rx_ms = HAL_GetTick();
  bms_runtime.bms_state = new_bms_state;
  bms_runtime.ping_lv_nodes = new_ping_lv_nodes;
  bms_runtime.relay_state = new_relay_state;
  bms_runtime.gpio_sense = new_gpio_sense;
  bms_update_critical_flags();
}

static void bms_faults_rx_callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                   const uint8_t *data, uint8_t length, void *user_data)
{
  bool new_bms_fault;
  bool new_imd_fault;

  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;

  if (data == NULL || length < RES_EBS_BMS_FAULTS_FRAME_LEN)
  {
    return;
  }

  new_bms_fault = (data[0] & 0x01U) != 0U;
  new_imd_fault = (data[0] & 0x02U) != 0U;

  if (!bms_runtime.accumulator_faults_received || new_bms_fault != bms_runtime.bms_fault ||
      new_imd_fault != bms_runtime.imd_fault)
  {
    bms_runtime.faults_changed = true;
  }

  bms_runtime.accumulator_faults_received = true;
  bms_runtime.last_accumulator_faults_rx_ms = HAL_GetTick();
  bms_runtime.bms_fault = new_bms_fault;
  bms_runtime.imd_fault = new_imd_fault;
  bms_update_critical_flags();
}

void RES_EBS_BMS_GetStatus(RES_EBS_BMS_Status_t *status)
{
  uint32_t primask;

  if (status == NULL)
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  status->bms_state_received = bms_runtime.bms_state_received;
  status->accumulator_faults_received = bms_runtime.accumulator_faults_received;
  status->last_bms_state_rx_ms = bms_runtime.last_bms_state_rx_ms;
  status->last_accumulator_faults_rx_ms = bms_runtime.last_accumulator_faults_rx_ms;
  status->bms_state = (RES_EBS_BMS_State_t)bms_runtime.bms_state;
  status->ping_lv_nodes = bms_runtime.ping_lv_nodes;
  status->relay_state = bms_runtime.relay_state;
  status->gpio_sense = bms_runtime.gpio_sense;
  status->bms_fault = bms_runtime.bms_fault;
  status->imd_fault = bms_runtime.imd_fault;
  status->critical_from_state = bms_runtime.critical_from_state;
  status->critical_from_fault_flags = bms_runtime.critical_from_fault_flags;
  status->critical_shutdown_active = bms_runtime.critical_shutdown_active;

  if (primask == 0U)
  {
    __enable_irq();
  }
}

bool RES_EBS_BMS_CriticalShutdownActive(void)
{
  return bms_runtime.critical_shutdown_active;
}

bool RES_EBS_BMS_RelayEnableAllowed(void)
{
  return !RES_EBS_BMS_CriticalShutdownActive();
}

const char *RES_EBS_BMS_StateToString(RES_EBS_BMS_State_t state)
{
  switch (state)
  {
  case RES_EBS_BMS_STATE_BOOT:
    return "BOOT";
  case RES_EBS_BMS_STATE_LV:
    return "LV";
  case RES_EBS_BMS_STATE_HEALTH_CHECK:
    return "HEALTH_CHECK";
  case RES_EBS_BMS_STATE_PRECHARGE:
    return "PRECHARGE";
  case RES_EBS_BMS_STATE_ENERGIZED:
    return "ENERGIZED";
  case RES_EBS_BMS_STATE_DRIVE:
    return "DRIVE";
  case RES_EBS_BMS_STATE_FREE:
    return "FREE";
  case RES_EBS_BMS_STATE_CHARGER_PRECHARGE:
    return "CHARGER_PRECHARGE";
  case RES_EBS_BMS_STATE_CHARGING:
    return "CHARGING";
  case RES_EBS_BMS_STATE_BALANCE:
    return "BALANCE";
  case RES_EBS_BMS_STATE_FAULT_BMS:
    return "FAULT_BMS";
  case RES_EBS_BMS_STATE_FAULT_BSPD:
    return "FAULT_BSPD";
  case RES_EBS_BMS_STATE_FAULT_IMD:
    return "FAULT_IMD";
  case RES_EBS_BMS_STATE_FAULT_CHARGING:
    return "FAULT_CHARGING";
  case RES_EBS_BMS_STATE_DEFAULT:
    return "DEFAULT";
  default:
    return "UNKNOWN";
  }
}

int RES_EBS_BMS_Init(void)
{
  FEB_CAN_RX_Params_t state_rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x00U,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FFU,
      .fifo = FEB_CAN_FIFO_0,
      .callback = bms_state_rx_callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Params_t faults_rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x04U,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FFU,
      .fifo = FEB_CAN_FIFO_0,
      .callback = bms_faults_rx_callback,
      .user_data = NULL,
  };

  memset(&bms_runtime, 0, sizeof(bms_runtime));
  bms_runtime.state_rx_handle = -1;
  bms_runtime.faults_rx_handle = -1;

  bms_runtime.state_rx_handle = FEB_CAN_RX_Register(&state_rx_params);
  if (bms_runtime.state_rx_handle < 0)
  {
    LOG_E(TAG_BMS, "Failed to register BMS state RX: %ld", (long)bms_runtime.state_rx_handle);
    return -1;
  }

  bms_runtime.faults_rx_handle = FEB_CAN_RX_Register(&faults_rx_params);
  if (bms_runtime.faults_rx_handle < 0)
  {
    LOG_E(TAG_BMS, "Failed to register accumulator faults RX: %ld", (long)bms_runtime.faults_rx_handle);
    FEB_CAN_RX_Unregister(bms_runtime.state_rx_handle);
    bms_runtime.state_rx_handle = -1;
    return -1;
  }

  LOG_I(TAG_BMS, "Monitoring BMS state on 0x000 and accumulator faults on 0x004");

  return 0;
}

void RES_EBS_BMS_Tick(void)
{
  RES_EBS_BMS_Status_t status = {0};
  uint32_t primask;
  bool state_changed;
  bool faults_changed;

  primask = __get_PRIMASK();
  __disable_irq();
  state_changed = bms_runtime.state_changed;
  faults_changed = bms_runtime.faults_changed;
  bms_runtime.state_changed = false;
  bms_runtime.faults_changed = false;
  if (primask == 0U)
  {
    __enable_irq();
  }

  RES_EBS_BMS_GetStatus(&status);

  if (state_changed)
  {
    LOG_I(TAG_BMS, "BMS state=%s(%u) ping=%u relay=%u gpio=%u", RES_EBS_BMS_StateToString(status.bms_state),
          (unsigned int)status.bms_state, (unsigned int)status.ping_lv_nodes, (unsigned int)status.relay_state,
          (unsigned int)status.gpio_sense);
  }

  if (faults_changed)
  {
    LOG_W(TAG_BMS, "Accumulator faults: bms_fault=%u imd_fault=%u", (unsigned int)status.bms_fault,
          (unsigned int)status.imd_fault);
  }

  if (status.critical_shutdown_active && !bms_runtime.last_logged_critical_shutdown_active)
  {
    LOG_E(TAG_BMS, "Critical shutdown active; opening %s (state=%s(%u), bms_fault=%u, imd_fault=%u)",
          RES_EBS_Relay_GetName(), RES_EBS_BMS_StateToString(status.bms_state), (unsigned int)status.bms_state,
          (unsigned int)status.bms_fault, (unsigned int)status.imd_fault);
  }
  else if (!status.critical_shutdown_active && bms_runtime.last_logged_critical_shutdown_active)
  {
    LOG_I(TAG_BMS, "Critical shutdown cleared; %s remains OFF until re-enabled", RES_EBS_Relay_GetName());
  }

  bms_runtime.last_logged_critical_shutdown_active = status.critical_shutdown_active;

  if (status.critical_shutdown_active && RES_EBS_Relay_Get())
  {
    RES_EBS_Relay_Set(false);
    LOG_W(TAG_BMS, "Forced %s OFF due to BMS critical shutdown", RES_EBS_Relay_GetName());
  }
}
