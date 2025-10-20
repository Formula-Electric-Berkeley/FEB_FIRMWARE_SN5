#include "bms_tasks.h"
#include "main.h"
#include "can.h"          
#include "stm32f4xx_hal.h"

/* ===== SensorTask =====
   Later: trigger ADBMS read via DMA/IT. For now it’s a stub at 50 ms. */
void SensorTask(void *argument) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    // TODO: trigger ADBMS read (non-blocking), enqueue result
    vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
  }
}

/* Example heartbeat packer (adjust ID/DLC/fields to your DBC) */
static inline void pack_bms_heartbeat(can_msg_t *m, uint8_t ctr){
  m->id  = 0x182;     // example 11-bit ID
  m->dlc = 2;
  m->data[0] = 0;     // status bits placeholder
  m->data[1] = ctr;   // rolling counter
}

/* ===== CommTask =====
   10 Hz TX heartbeat + (later) drain RX queue */
void CommTask(void *argument) {
  TickType_t last = xTaskGetTickCount();
  uint8_t ctr = 0;
  for (;;) {
    // (later) while (xQueueReceive(qCanRx, &rcv, 0) == pdTRUE) { ... }

    if (xTaskGetTickCount() - last >= pdMS_TO_TICKS(100)) {
      last += pdMS_TO_TICKS(100);
      can_msg_t tx; pack_bms_heartbeat(&tx, ctr++);
      // (later) xQueueSend(qCanTx, &tx, 0);
      (void)tx; // silence unused warning until CAN TX is hooked up
    }
    taskYIELD();
  }
}

/* ===== ProtectionTask =====
   Event-driven; opens contactors immediately on FAULT */
void ProtectionTask(void *argument) {
  for (;;) {
    xEventGroupWaitBits(evBmsFlags, EV_FAULT, pdTRUE, pdFALSE, portMAX_DELAY);
    // TODO: open_contactors_immediately(); send fault frame
  }
}