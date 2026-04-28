/**
 ******************************************************************************
 * @file    FEB_CAN.c
 * @brief   DASH CAN application layer (FreeRTOS)
 * @author  Formula Electric @ Berkeley
 ******************************************************************************
 *
 * This file:
 *  - Initializes the FEB CAN library
 *  - Registers RX callbacks
 *  - Implements FreeRTOS RX/TX tasks
 *  - Owns all DASH CAN behavior
 *
 * CubeMX FreeRTOS tasks are expected to call:
 *   - StartDASHTaskRx()
 *   - StartDASHTaskTx()
 *
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "feb_console.h"
#include "main.h"
#include "FEB_CAN_State.h"
#include "FEB_CAN_PingPong.h"

/* ========================== External HAL handles ========================== */
extern CAN_HandleTypeDef hcan1;

/* ========================== External FreeRTOS handles (from CubeMX) ========================== */
/* NOTE: These are created in CubeMX .ioc and defined in freertos.c */
/* Names in .ioc are without "Handle" suffix; CubeMX adds it automatically */
extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

/* ========================== Local Prototypes ========================== */
static void DASH_CAN_Init(void);
static void DASH_CAN_RxCallback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                const uint8_t *data, uint8_t length, void *user_data);

/* ========================== RX Callback ========================== */

static void DASH_CAN_RxCallback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)data;
  (void)user_data;
  (void)can_id;
  (void)length;
}

/* ========================== CAN Initialization ========================== */

static void DASH_CAN_Init(void)
{
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = NULL,
      .get_tick_ms = HAL_GetTick,
#if FEB_CAN_USE_FREERTOS
      .tx_queue = canTxQueueHandle,
      .rx_queue = canRxQueueHandle,
      .tx_mutex = canTxMutexHandle,
      .rx_mutex = canRxMutexHandle,
      .tx_mailbox_sem = canTxMailboxSemHandle,
#endif
  };

  if (FEB_CAN_Init(&cfg) != FEB_CAN_OK)
  {
    FEB_Console_Printf("CAN failed to init\r\n");
    /* CAN initialization failed - halt in debug */
    while (1)
    {
    }
  }

  FEB_Console_Printf("CAN successfully init\r\n");

  /* ---------------- RX Registration (wildcard to receive all) ----------------
   * Temporarily enabled while diagnosing loopback RX. With the wildcard handler
   * registered, FEB_CAN_Filter_UpdateFromRegistry installs an accept-all filter
   * (see feb_can_filter.c FEB_CAN_Filter_AcceptAll) so every received frame
   * fires DASH_CAN_RxCallback, independent of any per-channel ping/pong filters.
   */
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x00,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_WILDCARD,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = DASH_CAN_RxCallback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&rx_params);

  /* Ensure filters reflect registry */
  FEB_CAN_Filter_UpdateFromRegistry(FEB_CAN_INSTANCE_1);
}

/* ============================================================================
 * FreeRTOS Tasks (override CubeMX weak stubs)
 * ============================================================================
 *
 * Task split mirrors the BMS layout (BMSTaskRx / SMTask / BMSTaskTx) but folds
 * the periodic publishers (State_Tick, PingPong_Tick) into DASHTaskRx so we
 * don't need a third task slot in the .ioc:
 *
 *   - DASHTaskRx : RX_Process + State_Tick + PingPong_Tick (publishers only
 *                  enqueue into the TX queue; they never take the mailbox
 *                  semaphore, so they cannot block on a stuck CAN bus)
 *   - DASHTaskTx : FEB_CAN_TX_Process() only, paced by osDelay(1)
 *
 * The previous design ran all three publishers AND TX_Process in the single
 * DASHTaskTx, gated on a TIM6 task notification. When TX_Process started
 * timing out on the mailbox semaphore (FEB_CAN_TX_TIMEOUT_MS = 100 ms per
 * queued message), the whole task stalled, accumulated TIM6 notifications
 * collapsed to a single take (pdTRUE), and the heartbeat / state / pingpong
 * cadence quietly stretched out — observed externally as "DASH stops sending
 * CAN." Splitting publishers from TX_Process matches the BMS pattern, which
 * is why the BMS keeps publishing through the same kind of bus hiccup.
 */

/**
 * @brief DASH CAN RX task — also drives the periodic publishers.
 */
void StartDASHTaskRx(void *argument)
{
  (void)argument;

  static uint16_t pingpong_divider = 0;

  /* CAN init MUST occur after scheduler start */
  DASH_CAN_Init();
  FEB_CAN_PingPong_Init();
  // FEB_CAN_BMS_Init();
  // FEB_CAN_PCU_Init();
  // FEB_CAN_LVPDB_Init();

  /* Signal that CAN is ready for state publishing */
  FEB_CAN_State_SetReady();

  for (;;)
  {
    /* Dispatch RX queue and invoke callbacks */
    FEB_CAN_RX_Process();

    /* Publish DASH state. State_Tick has an internal /100 divider so it only
       emits a heartbeat every ~100 ms even though we call it every ~1 ms. */
    FEB_CAN_State_Tick();

    /* PingPong_Tick at ~100 ms cadence (same as the BMS SMTask divider). */
    if (++pingpong_divider >= 100)
    {
      pingpong_divider = 0;
      FEB_CAN_PingPong_Tick();
    }

    osDelay(1);
  }
}

/**
 * @brief DASH CAN TX task — drains the TX queue into the CAN mailboxes.
 *
 * Mirrors BMSTaskTx exactly. Kept on its own osDelay(1) loop so a stalled
 * mailbox (sem starved, bus-off, AutoRetransmission=DISABLE NACKs, etc.) only
 * delays this task — not the publishers in DASHTaskRx.
 */
void StartDASHTaskTx(void *argument)
{
  (void)argument;

  for (;;)
  {
    FEB_CAN_TX_Process();
    osDelay(1);
  }
}
