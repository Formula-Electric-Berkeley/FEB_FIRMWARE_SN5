/**
 ******************************************************************************
 * @file           : feb_can.c
 * @brief          : Core initialization for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "feb_can_internal.h"
#include "feb_log.h"
#include "main.h"
#include <string.h>

/* ============================================================================
 * Global Context
 * ============================================================================ */

static FEB_CAN_Context_t feb_can_ctx = {0};

#define FEB_CAN_RX_NOTIFICATIONS (CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING)
#define FEB_CAN_TX_NOTIFICATIONS (CAN_IT_TX_MAILBOX_EMPTY)
/* Only enable real bus-state errors. CAN_IT_LAST_ERROR_CODE fires the SCE IRQ
 * on every transmitted/received frame (LEC is "last error code", written
 * unconditionally by the bxCAN core), which produces an ISR storm in loopback
 * and amplifies any per-ISR stack usage. CAN_IT_ERROR is the master gate for
 * the previous flags and is implied by enabling any of them, so omit it too. */
#define FEB_CAN_ERROR_NOTIFICATIONS (CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF)

FEB_CAN_Context_t *feb_can_get_context(void)
{
  return &feb_can_ctx;
}

/* ============================================================================
 * Default Timestamp Function
 * ============================================================================ */

static uint32_t feb_can_default_get_tick(void)
{
  return HAL_GetTick();
}

/* ============================================================================
 * Instance Lookup Helper
 * ============================================================================ */

static FEB_CAN_Instance_t feb_can_get_instance_from_handle(CAN_HandleTypeDef *hcan)
{
  if (hcan == (CAN_HandleTypeDef *)feb_can_ctx.hcan[FEB_CAN_INSTANCE_1])
  {
    return FEB_CAN_INSTANCE_1;
  }
  else if (hcan == (CAN_HandleTypeDef *)feb_can_ctx.hcan[FEB_CAN_INSTANCE_2])
  {
    return FEB_CAN_INSTANCE_2;
  }
  return FEB_CAN_INSTANCE_1; /* Default fallback */
}

/* ============================================================================
 * Initialization API
 * ============================================================================ */

FEB_CAN_Status_t FEB_CAN_Init(const FEB_CAN_Config_t *config)
{
  if (config == NULL)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (config->hcan1 == NULL)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  /* Already initialized? */
  if (feb_can_ctx.initialized)
  {
    return FEB_CAN_ERROR;
  }

  /* Clear context */
  memset(&feb_can_ctx, 0, sizeof(feb_can_ctx));

  /* Store HAL handles */
  feb_can_ctx.hcan[FEB_CAN_INSTANCE_1] = config->hcan1;
  feb_can_ctx.hcan[FEB_CAN_INSTANCE_2] = config->hcan2;

  /* Set timestamp function */
  feb_can_ctx.get_tick_ms = config->get_tick_ms ? config->get_tick_ms : feb_can_default_get_tick;

#if FEB_CAN_USE_FREERTOS
  /* Validate REQUIRED sync primitives */
  if (config->tx_queue == NULL || config->rx_queue == NULL)
  {
    return FEB_CAN_ERROR_QUEUE;
  }
  if (config->tx_mutex == NULL || config->rx_mutex == NULL)
  {
    return FEB_CAN_ERROR_NO_MUTEX;
  }
  if (config->tx_mailbox_sem == NULL)
  {
    return FEB_CAN_ERROR_NO_SEMAPHORE;
  }

  /* Store user-provided sync primitives (NOT created internally) */
  feb_can_ctx.tx_queue = config->tx_queue;
  feb_can_ctx.rx_queue = config->rx_queue;
  feb_can_ctx.tx_mutex = config->tx_mutex;
  feb_can_ctx.rx_mutex = config->rx_mutex;
  feb_can_ctx.tx_sem = config->tx_mailbox_sem;
#endif

  /* Cast handles for HAL functions */
  CAN_HandleTypeDef *hcan1 = (CAN_HandleTypeDef *)config->hcan1;
  CAN_HandleTypeDef *hcan2 = (CAN_HandleTypeDef *)config->hcan2;

  /* Start CAN peripherals */
  if (HAL_CAN_Start(hcan1) != HAL_OK)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_HAL;
  }

  if (hcan2 != NULL)
  {
    if (HAL_CAN_Start(hcan2) != HAL_OK)
    {
      HAL_CAN_Stop(hcan1);
      FEB_CAN_DeInit();
      return FEB_CAN_ERROR_HAL;
    }
  }

  /* Activate RX/TX/error notifications */
  if (HAL_CAN_ActivateNotification(hcan1, FEB_CAN_RX_NOTIFICATIONS) != HAL_OK)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_HAL;
  }

  if (HAL_CAN_ActivateNotification(hcan1, FEB_CAN_TX_NOTIFICATIONS) != HAL_OK)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_HAL;
  }

  if (HAL_CAN_ActivateNotification(hcan1, FEB_CAN_ERROR_NOTIFICATIONS) != HAL_OK)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_HAL;
  }

  if (hcan2 != NULL)
  {
    if (HAL_CAN_ActivateNotification(hcan2, FEB_CAN_RX_NOTIFICATIONS) != HAL_OK)
    {
      FEB_CAN_DeInit();
      return FEB_CAN_ERROR_HAL;
    }

    if (HAL_CAN_ActivateNotification(hcan2, FEB_CAN_TX_NOTIFICATIONS) != HAL_OK)
    {
      FEB_CAN_DeInit();
      return FEB_CAN_ERROR_HAL;
    }

    if (HAL_CAN_ActivateNotification(hcan2, FEB_CAN_ERROR_NOTIFICATIONS) != HAL_OK)
    {
      FEB_CAN_DeInit();
      return FEB_CAN_ERROR_HAL;
    }
  }

  feb_can_ctx.initialized = true;

  return FEB_CAN_OK;
}

void FEB_CAN_DeInit(void)
{
  CAN_HandleTypeDef *hcan1 = (CAN_HandleTypeDef *)feb_can_ctx.hcan[FEB_CAN_INSTANCE_1];
  CAN_HandleTypeDef *hcan2 = (CAN_HandleTypeDef *)feb_can_ctx.hcan[FEB_CAN_INSTANCE_2];

  if (hcan1 != NULL)
  {
    HAL_CAN_DeactivateNotification(hcan1,
                                   FEB_CAN_RX_NOTIFICATIONS | FEB_CAN_TX_NOTIFICATIONS | FEB_CAN_ERROR_NOTIFICATIONS);
    HAL_CAN_Stop(hcan1);
  }

  if (hcan2 != NULL)
  {
    HAL_CAN_DeactivateNotification(hcan2,
                                   FEB_CAN_RX_NOTIFICATIONS | FEB_CAN_TX_NOTIFICATIONS | FEB_CAN_ERROR_NOTIFICATIONS);
    HAL_CAN_Stop(hcan2);
  }

  /*
   * NOTE: We do NOT delete user-provided sync primitives (queues, mutexes, semaphores).
   * The user created them in CubeMX/.ioc and owns their lifecycle.
   * We only clear our references.
   */
#if FEB_CAN_USE_FREERTOS
  feb_can_ctx.tx_queue = NULL;
  feb_can_ctx.rx_queue = NULL;
  feb_can_ctx.tx_mutex = NULL;
  feb_can_ctx.rx_mutex = NULL;
  feb_can_ctx.tx_sem = NULL;
#endif

  memset(&feb_can_ctx, 0, sizeof(feb_can_ctx));
}

bool FEB_CAN_IsInitialized(void)
{
  return feb_can_ctx.initialized;
}

/* ============================================================================
 * HAL Callback Routing - RX
 * ============================================================================ */

static void feb_can_rx_fifo_callback(FEB_CAN_Handle_t hcan_ptr, uint32_t fifo)
{
  if (!feb_can_ctx.initialized)
  {
    return;
  }

  CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)hcan_ptr;
  CAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];

  /* Do NOT call LOG_*() here. This is ISR context — FEB_UART_Write returns
   * -1 from ISR (so nothing prints), but FEB_Log_Output still allocates a
   * 128-byte isr_buffer + ~150 bytes of vsnprintf scratch on the MSP. With
   * the Cortex-M4F lazy-FPU frame (128 B) this can overflow the 1 KiB MSP
   * (_Min_Stack_Size = 0x400) when CAN frames cascade in loopback mode. Drain
   * to the FreeRTOS RX queue and let FEB_CAN_RX_Process log from task ctx. */

  while (HAL_CAN_GetRxFifoFillLevel(hcan, fifo) > 0)
  {
    if (HAL_CAN_GetRxMessage(hcan, fifo, &rx_header, rx_data) != HAL_OK)
    {
      break;
    }

    FEB_CAN_Instance_t instance = feb_can_get_instance_from_handle(hcan);
    uint32_t can_id = (rx_header.IDE == CAN_ID_STD) ? rx_header.StdId : rx_header.ExtId;
    uint8_t id_type = (rx_header.IDE == CAN_ID_STD) ? FEB_CAN_ID_STD : FEB_CAN_ID_EXT;
    uint32_t timestamp = feb_can_ctx.get_tick_ms();

#if FEB_CAN_USE_FREERTOS
    /* Queue message for deferred processing */
    FEB_CAN_Message_t msg;
    msg.can_id = can_id;
    msg.id_type = id_type;
    msg.instance = instance;
    msg.length = rx_header.DLC;
    msg.timestamp = timestamp;
    memcpy(msg.data, rx_data, rx_header.DLC);

    if (!FEB_CAN_QUEUE_SEND_ISR(feb_can_ctx.rx_queue, &msg))
    {
      /* Queue full - message dropped */
      feb_can_ctx.rx_queue_overflow_count++;
    }
#else
    /* Direct dispatch in bare-metal mode */
    feb_can_rx_dispatch(instance, can_id, id_type, rx_data, rx_header.DLC, timestamp);
#endif
  }
}

void FEB_CAN_RxFifo0Callback(FEB_CAN_Handle_t hcan)
{
  feb_can_rx_fifo_callback(hcan, CAN_RX_FIFO0);
}

void FEB_CAN_RxFifo1Callback(FEB_CAN_Handle_t hcan)
{
  feb_can_rx_fifo_callback(hcan, CAN_RX_FIFO1);
}

/* ============================================================================
 * HAL Callback Routing - TX Complete
 * ============================================================================ */

static void feb_can_tx_complete_callback(FEB_CAN_Handle_t hcan)
{
  (void)hcan;

  if (!feb_can_ctx.initialized)
  {
    return;
  }

#if FEB_CAN_USE_FREERTOS
  /* One ISR notification ↔ one successful HAL_CAN_AddTxMessage (tx_pending_count++).
   * Only release the counting semaphore when we had a matching pending TX.
   * Spurious TX-mailbox-empty edges during bring-up or errata can otherwise
   * inflate the sem above the 3 hardware mailboxes and corrupt TX flow. */
  if (feb_can_ctx.tx_pending_count > 0)
  {
    feb_can_ctx.tx_pending_count--;
    FEB_CAN_SEM_GIVE_ISR(feb_can_ctx.tx_sem);
  }
#endif
}

void FEB_CAN_TxMailbox0CompleteCallback(FEB_CAN_Handle_t hcan)
{
  feb_can_tx_complete_callback(hcan);
}

void FEB_CAN_TxMailbox1CompleteCallback(FEB_CAN_Handle_t hcan)
{
  feb_can_tx_complete_callback(hcan);
}

void FEB_CAN_TxMailbox2CompleteCallback(FEB_CAN_Handle_t hcan)
{
  feb_can_tx_complete_callback(hcan);
}

/* ============================================================================
 * HAL Callback Routing - Error
 * ============================================================================ */

void FEB_CAN_ErrorCallback(FEB_CAN_Handle_t hcan)
{
  CAN_HandleTypeDef *h = (CAN_HandleTypeDef *)hcan;
  if (h == NULL || h->Instance == NULL)
  {
    return;
  }

  /* Snapshot the ESR + HAL ErrorCode for later inspection. We deliberately
   * do not LOG_*() here — same MSP-stack reasoning as feb_can_rx_fifo_callback.
   * Use FEB_CAN_GetLastErrorSnapshot() / a periodic task-context log to view. */
  feb_can_ctx.last_error_esr = h->Instance->ESR;
  feb_can_ctx.last_error_code = h->ErrorCode;
  feb_can_ctx.error_callback_count++;
}

/* ============================================================================
 * HAL callback forwarders (must be strong symbols)
 *
 * STM32 HAL ships __weak empty defaults for these in stm32f4xx_hal_can.c.
 * This translation unit also used __weak, so the linker could pick either
 * definition — often the HAL stub — and TX-complete / RX IRQ paths would never
 * reach the library (mailbox semaphore stuck, RX queue never fed).
 *
 * Strong definitions here override HAL's weak stubs. To extend behavior,
 * wrap or replace these in the application and call the FEB_CAN_* entry points.
 * ============================================================================ */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_RxFifo0Callback(hcan);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_RxFifo1Callback(hcan);
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_TxMailbox0CompleteCallback(hcan);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_TxMailbox1CompleteCallback(hcan);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_TxMailbox2CompleteCallback(hcan);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_ErrorCallback(hcan);
}

/* ============================================================================
 * Status API
 * ============================================================================ */

bool FEB_CAN_TX_IsReady(FEB_CAN_Instance_t instance)
{
  if (!feb_can_ctx.initialized || instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return false;
  }

  CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)feb_can_ctx.hcan[instance];
  if (hcan == NULL)
  {
    return false;
  }

  return HAL_CAN_GetTxMailboxesFreeLevel(hcan) > 0;
}

uint32_t FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_Instance_t instance)
{
  if (!feb_can_ctx.initialized || instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return 0;
  }

  CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)feb_can_ctx.hcan[instance];
  if (hcan == NULL)
  {
    return 0;
  }

  return HAL_CAN_GetTxMailboxesFreeLevel(hcan);
}

uint32_t FEB_CAN_TX_GetQueuePending(void)
{
#if FEB_CAN_USE_FREERTOS
  if (feb_can_ctx.tx_queue != NULL)
  {
    return FEB_CAN_QUEUE_COUNT(feb_can_ctx.tx_queue);
  }
#endif
  return 0;
}

uint32_t FEB_CAN_RX_GetQueuePending(void)
{
#if FEB_CAN_USE_FREERTOS
  if (feb_can_ctx.rx_queue != NULL)
  {
    return FEB_CAN_QUEUE_COUNT(feb_can_ctx.rx_queue);
  }
#endif
  return 0;
}

/* ============================================================================
 * Error Counter API
 * ============================================================================ */

uint32_t FEB_CAN_GetRxQueueOverflowCount(void)
{
  return feb_can_ctx.rx_queue_overflow_count;
}

uint32_t FEB_CAN_GetTxQueueOverflowCount(void)
{
  return feb_can_ctx.tx_queue_overflow_count;
}

uint32_t FEB_CAN_GetTxTimeoutCount(void)
{
  return feb_can_ctx.tx_timeout_count;
}

uint32_t FEB_CAN_GetHalErrorCount(void)
{
  return feb_can_ctx.hal_error_count;
}

void FEB_CAN_ResetErrorCounters(void)
{
  feb_can_ctx.rx_queue_overflow_count = 0;
  feb_can_ctx.tx_queue_overflow_count = 0;
  feb_can_ctx.tx_timeout_count = 0;
  feb_can_ctx.hal_error_count = 0;
}

const char *FEB_CAN_StatusToString(FEB_CAN_Status_t status)
{
  switch (status)
  {
  case FEB_CAN_OK:
    return "OK";
  case FEB_CAN_ERROR:
    return "ERROR";
  case FEB_CAN_ERROR_INVALID_PARAM:
    return "INVALID_PARAM";
  case FEB_CAN_ERROR_FULL:
    return "QUEUE_FULL";
  case FEB_CAN_ERROR_NOT_FOUND:
    return "NOT_FOUND";
  case FEB_CAN_ERROR_ALREADY_EXISTS:
    return "ALREADY_EXISTS";
  case FEB_CAN_ERROR_TIMEOUT:
    return "TIMEOUT";
  case FEB_CAN_ERROR_HAL:
    return "HAL_ERROR";
  case FEB_CAN_ERROR_NOT_INIT:
    return "NOT_INIT";
  case FEB_CAN_ERROR_QUEUE:
    return "QUEUE_ERROR";
  case FEB_CAN_ERROR_NO_MUTEX:
    return "NO_MUTEX";
  case FEB_CAN_ERROR_NO_SEMAPHORE:
    return "NO_SEMAPHORE";
  default:
    return "UNKNOWN";
  }
}

/* ============================================================================
 * Weak Task Function Implementations (FreeRTOS Mode)
 * ============================================================================ */

#if FEB_CAN_USE_FREERTOS

/**
 * @brief Weak default TX processing task
 *
 * Processes TX queue. Override to customize behavior.
 *
 * @param argument Not used (pass NULL)
 */
__attribute__((weak)) void FEB_CAN_TxTaskFunc(void *argument)
{
  (void)argument;

  for (;;)
  {
    FEB_CAN_TX_Process();
    osDelay(1); /* Yield to other tasks */
  }
}

/**
 * @brief Weak default RX processing task
 *
 * Processes RX queue and invokes callbacks. Override to customize.
 *
 * @param argument Not used (pass NULL)
 */
__attribute__((weak)) void FEB_CAN_RxTaskFunc(void *argument)
{
  (void)argument;

  for (;;)
  {
    FEB_CAN_RX_Process();
    osDelay(1); /* Yield to other tasks */
  }
}

#endif /* FEB_CAN_USE_FREERTOS */
