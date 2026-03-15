#ifndef FEB_RES_EBS_SAFETY_H
#define FEB_RES_EBS_SAFETY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  bool ts_activation;
  bool activation_seen_high;
  bool shutdown_open_fault;
  bool emergency_latched;
  bool relay_enable_allowed;
  uint32_t last_change_ms;
} RES_EBS_Safety_Status_t;

void RES_EBS_Safety_Init(void);
void RES_EBS_Safety_Tick(void);
void RES_EBS_Safety_GetStatus(RES_EBS_Safety_Status_t *status);
bool RES_EBS_Safety_RelayEnableAllowed(void);
bool RES_EBS_Safety_EmergencyLatched(void);
bool RES_EBS_Safety_ClearLatchedEmergency(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_RES_EBS_SAFETY_H */
