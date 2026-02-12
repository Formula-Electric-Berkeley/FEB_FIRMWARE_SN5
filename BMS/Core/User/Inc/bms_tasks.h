#include "FreeRTOS.h"
#ifndef BMS_TASKS_H
#define BMS_TASKS_H

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include <stdint.h>
#include "main.h"

typedef struct
{
  uint32_t id;
  uint8_t dlc;
  uint8_t data[8];
  uint32_t ts;
} can_msg_t;

extern osMutexId_t ADBMSMutexHandle;

enum
{
  EV_FAULT = (1u << 0),
  EV_PRECHARGE_OK = (1u << 1),
};

void StartADBMSTask(void *arg);
void StartTPSTask(void *arg);

#endif
