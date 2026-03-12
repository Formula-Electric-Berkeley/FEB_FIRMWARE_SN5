/**
 ******************************************************************************
 * @file           : feb_can.c
 * @brief          : Core initialization for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "feb_can_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ============================================================================
 * Global Context
 * ============================================================================ */

static FEB_CAN_Context_t feb_can_ctx = {0};

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
  /* Create queues */
  uint16_t tx_queue_size = config->tx_queue_size > 0 ? config->tx_queue_size : FEB_CAN_TX_QUEUE_SIZE;
  uint16_t rx_queue_size = config->rx_queue_size > 0 ? config->rx_queue_size : FEB_CAN_RX_QUEUE_SIZE;

  feb_can_ctx.tx_queue = FEB_CAN_QUEUE_CREATE(tx_queue_size, sizeof(FEB_CAN_Message_t));
  feb_can_ctx.rx_queue = FEB_CAN_QUEUE_CREATE(rx_queue_size, sizeof(FEB_CAN_Message_t));

  if (feb_can_ctx.tx_queue == NULL || feb_can_ctx.rx_queue == NULL)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_QUEUE;
  }

  /* Create mutexes */
  feb_can_ctx.tx_mutex = FEB_CAN_MUTEX_CREATE();
  feb_can_ctx.rx_mutex = FEB_CAN_MUTEX_CREATE();

  /* Create TX complete semaphore (3 mailboxes available) */
  feb_can_ctx.tx_sem = FEB_CAN_SEM_CREATE(3, 3);
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

  /* Activate RX notifications */
  if (HAL_CAN_ActivateNotification(hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_HAL;
  }

  /* Activate TX complete notifications for non-blocking flow control */
  if (HAL_CAN_ActivateNotification(hcan1, CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK)
  {
    FEB_CAN_DeInit();
    return FEB_CAN_ERROR_HAL;
  }

  if (hcan2 != NULL)
  {
    if (HAL_CAN_ActivateNotification(hcan2, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
    {
      FEB_CAN_DeInit();
      return FEB_CAN_ERROR_HAL;
    }

    if (HAL_CAN_ActivateNotification(hcan2, CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK)
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
                                   CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY);
    HAL_CAN_Stop(hcan1);
  }

  if (hcan2 != NULL)
  {
    HAL_CAN_DeactivateNotification(hcan2,
                                   CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY);
    HAL_CAN_Stop(hcan2);
  }

#if FEB_CAN_USE_FREERTOS
  if (feb_can_ctx.tx_queue != NULL)
  {
    FEB_CAN_QUEUE_DELETE(feb_can_ctx.tx_queue);
  }
  if (feb_can_ctx.rx_queue != NULL)
  {
    FEB_CAN_QUEUE_DELETE(feb_can_ctx.rx_queue);
  }
  if (feb_can_ctx.tx_mutex != NULL)
  {
    FEB_CAN_MUTEX_DELETE(feb_can_ctx.tx_mutex);
  }
  if (feb_can_ctx.rx_mutex != NULL)
  {
    FEB_CAN_MUTEX_DELETE(feb_can_ctx.rx_mutex);
  }
  if (feb_can_ctx.tx_sem != NULL)
  {
    FEB_CAN_SEM_DELETE(feb_can_ctx.tx_sem);
  }
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
  /* Signal that a mailbox is now free */
  if (feb_can_ctx.tx_pending_count > 0)
  {
    feb_can_ctx.tx_pending_count--;
  }
  FEB_CAN_SEM_GIVE_ISR(feb_can_ctx.tx_sem);
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
  (void)hcan;
  /* Can be extended to track error statistics */
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
  default:
    return "UNKNOWN";
  }
}
