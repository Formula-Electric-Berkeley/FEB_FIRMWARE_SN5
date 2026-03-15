#include "FEB_RES_EBS_Safety.h"

#include "FEB_RES_EBS_Board.h"
#include "feb_uart_log.h"
#include "stm32f4xx_hal.h"

typedef struct
{
  bool ts_activation;
  bool activation_seen_high;
  bool shutdown_open_fault;
  bool emergency_latched;
  uint32_t last_change_ms;
} RES_EBS_Safety_Runtime_t;

static RES_EBS_Safety_Runtime_t safety_runtime;

static void safety_latch_shutdown_open_fault(void)
{
  if (!safety_runtime.shutdown_open_fault)
  {
    LOG_E(TAG_GPIO, "Shutdown permissive lost on %s; opening %s", RES_EBS_TSActivation_GetName(), RES_EBS_Relay_GetName());
  }

  safety_runtime.shutdown_open_fault = true;
  safety_runtime.emergency_latched = true;
}

void RES_EBS_Safety_Init(void)
{
  safety_runtime.ts_activation = RES_EBS_TSActivation_Get();
  safety_runtime.activation_seen_high = safety_runtime.ts_activation;
  safety_runtime.shutdown_open_fault = false;
  safety_runtime.emergency_latched = false;
  safety_runtime.last_change_ms = HAL_GetTick();
}

void RES_EBS_Safety_Tick(void)
{
  bool ts_activation = RES_EBS_TSActivation_Get();

  if (ts_activation != safety_runtime.ts_activation)
  {
    safety_runtime.ts_activation = ts_activation;
    safety_runtime.last_change_ms = HAL_GetTick();
    LOG_I(TAG_GPIO, "%s=%u", RES_EBS_TSActivation_GetName(), (unsigned int)ts_activation);
  }

  if (ts_activation)
  {
    safety_runtime.activation_seen_high = true;
  }

  if (!ts_activation && (safety_runtime.activation_seen_high || RES_EBS_Relay_Get()))
  {
    safety_latch_shutdown_open_fault();
  }

  if (safety_runtime.emergency_latched && RES_EBS_Relay_Get())
  {
    RES_EBS_Relay_Set(false);
    LOG_W(TAG_GPIO, "Forced %s OFF due to shutdown/open safe state", RES_EBS_Relay_GetName());
  }
}

void RES_EBS_Safety_GetStatus(RES_EBS_Safety_Status_t *status)
{
  if (status == NULL)
  {
    return;
  }

  status->ts_activation = safety_runtime.ts_activation;
  status->activation_seen_high = safety_runtime.activation_seen_high;
  status->shutdown_open_fault = safety_runtime.shutdown_open_fault;
  status->emergency_latched = safety_runtime.emergency_latched;
  status->relay_enable_allowed = (safety_runtime.ts_activation && !safety_runtime.emergency_latched);
  status->last_change_ms = safety_runtime.last_change_ms;
}

bool RES_EBS_Safety_RelayEnableAllowed(void)
{
  return (safety_runtime.ts_activation && !safety_runtime.emergency_latched);
}

bool RES_EBS_Safety_EmergencyLatched(void)
{
  return safety_runtime.emergency_latched;
}

bool RES_EBS_Safety_ClearLatchedEmergency(void)
{
  if (!RES_EBS_TSActivation_Get())
  {
    return false;
  }

  safety_runtime.ts_activation = true;
  safety_runtime.activation_seen_high = true;
  safety_runtime.shutdown_open_fault = false;
  safety_runtime.emergency_latched = false;
  safety_runtime.last_change_ms = HAL_GetTick();
  LOG_I(TAG_GPIO, "Cleared shutdown/open emergency latch");

  return true;
}
