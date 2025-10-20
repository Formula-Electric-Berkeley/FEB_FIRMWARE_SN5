#include "FreeRTOS.h"
#ifndef BMS_TASKS_H
#define BMS_TASKS_H

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include <stdint.h>

typedef struct {
  uint32_t id;
  uint8_t  dlc;
  uint8_t  data[8];
  uint32_t ts;  
} can_msg_t;

extern QueueHandle_t qCanRx;
extern QueueHandle_t qCanTx;
extern EventGroupHandle_t evBmsFlags;

enum {
  EV_FAULT = (1u<<0),
  EV_PRECHARGE_OK = (1u<<1),
};

void SensorTask(void *arg);
void CommTask(void *arg);
void ProtectionTask(void *arg);

#endif

