/**
 ******************************************************************************
 * @file           : feb_can_tx.c
 * @brief          : TX handling for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "feb_can_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ============================================================================
 * Internal TX Function via HAL
 * ============================================================================ */

int feb_can_tx_hal_transmit(FEB_CAN_Instance_t instance, uint32_t can_id, uint8_t id_type, const uint8_t *data,
                            uint8_t length)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

  CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)ctx->hcan[instance];
  if (hcan == NULL)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

  /* Check for available mailbox */
  if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
  {
    return -FEB_CAN_ERROR_FULL;
  }

  /* Prepare TX header */
  CAN_TxHeaderTypeDef tx_header = {0};

  if (id_type == FEB_CAN_ID_STD)
  {
    tx_header.StdId = can_id;
    tx_header.IDE = CAN_ID_STD;
  }
  else
  {
    tx_header.ExtId = can_id;
    tx_header.IDE = CAN_ID_EXT;
  }

  tx_header.RTR = CAN_RTR_DATA;
  tx_header.DLC = length;
  tx_header.TransmitGlobalTime = DISABLE;

  /* Copy data to local buffer for const correctness */
  uint8_t tx_data[8] = {0};
  if (length > 0 && data != NULL)
  {
    if (length > 8)
    {
      length = 8;
    }
    memcpy(tx_data, data, length);
  }

#if FEB_CAN_USE_FREERTOS
  /* Increment pending count BEFORE HAL call to avoid race condition with TX complete ISR.
   * If ISR fires after HAL_CAN_AddTxMessage but before we increment, the count would be wrong. */
  ctx->tx_pending_count++;
#endif

  /* Add to mailbox */
  uint32_t tx_mailbox;
  if (HAL_CAN_AddTxMessage(hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
  {
#if FEB_CAN_USE_FREERTOS
    /* Rollback the increment on failure */
    if (ctx->tx_pending_count > 0)
    {
      ctx->tx_pending_count--;
    }
#endif
    ctx->hal_error_count++;
    return -FEB_CAN_ERROR_HAL;
  }

  return FEB_CAN_OK;
}

/* ============================================================================
 * TX Registration API
 * ============================================================================ */

int32_t FEB_CAN_TX_Register(const FEB_CAN_TX_Params_t *params)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return -FEB_CAN_ERROR_NOT_INIT;
  }

  if (params == NULL)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (params->instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_LOCK(ctx->tx_mutex);
#endif

  /* Find free slot */
  int32_t free_slot = -1;
  for (uint32_t i = 0; i < FEB_CAN_MAX_TX_HANDLES; i++)
  {
    if (!ctx->tx_handles[i].is_active)
    {
      free_slot = (int32_t)i;
      break;
    }
  }

  if (free_slot < 0)
  {
#if FEB_CAN_USE_FREERTOS
    FEB_CAN_MUTEX_UNLOCK(ctx->tx_mutex);
#endif
    return -FEB_CAN_ERROR_FULL;
  }

  /* Fill handle */
  FEB_CAN_TX_Handle_Internal_t *handle = &ctx->tx_handles[free_slot];
  handle->instance = params->instance;
  handle->can_id = params->can_id;
  handle->id_type = params->id_type;
  handle->data_ptr = params->data_ptr;
  handle->data_size = params->data_size;
  handle->period_ms = params->period_ms;
  handle->pack_func = params->pack_func;
  handle->last_tx_time = 0;
  handle->is_active = true;

  ctx->tx_handle_count++;

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_UNLOCK(ctx->tx_mutex);
#endif

  return free_slot;
}

FEB_CAN_Status_t FEB_CAN_TX_Unregister(int32_t handle)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (handle < 0 || handle >= FEB_CAN_MAX_TX_HANDLES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_LOCK(ctx->tx_mutex);
#endif

  FEB_CAN_TX_Handle_Internal_t *h = &ctx->tx_handles[handle];
  if (!h->is_active)
  {
#if FEB_CAN_USE_FREERTOS
    FEB_CAN_MUTEX_UNLOCK(ctx->tx_mutex);
#endif
    return FEB_CAN_ERROR_NOT_FOUND;
  }

  /* Clear handle */
  memset(h, 0, sizeof(FEB_CAN_TX_Handle_Internal_t));
  ctx->tx_handle_count--;

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_UNLOCK(ctx->tx_mutex);
#endif

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_TX_SetPeriod(int32_t handle, uint32_t period_ms)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (handle < 0 || handle >= FEB_CAN_MAX_TX_HANDLES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  FEB_CAN_TX_Handle_Internal_t *h = &ctx->tx_handles[handle];
  if (!h->is_active)
  {
    return FEB_CAN_ERROR_NOT_FOUND;
  }

  h->period_ms = period_ms;

  return FEB_CAN_OK;
}

uint32_t FEB_CAN_TX_GetRegisteredCount(void)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();
  return ctx->tx_handle_count;
}

/* ============================================================================
 * TX Transmit API
 * ============================================================================ */

FEB_CAN_Status_t FEB_CAN_TX_Send(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (length > 8)
  {
    length = 8;
  }

#if FEB_CAN_USE_FREERTOS
  /* Queue the message for transmission */
  FEB_CAN_Message_t msg;
  msg.can_id = can_id;
  msg.id_type = id_type;
  msg.instance = instance;
  msg.length = length;
  msg.timestamp = ctx->get_tick_ms();

  if (data != NULL && length > 0)
  {
    memcpy(msg.data, data, length);
  }
  else
  {
    memset(msg.data, 0, 8);
  }

  if (!FEB_CAN_QUEUE_SEND(ctx->tx_queue, &msg, FEB_CAN_TX_QUEUE_TIMEOUT_MS))
  {
    ctx->tx_queue_overflow_count++;
    return FEB_CAN_ERROR_QUEUE;
  }

  return FEB_CAN_OK;
#else
  /* Direct transmit in bare-metal mode */
  int result = feb_can_tx_hal_transmit(instance, can_id, id_type, data, length);
  return (result >= 0) ? FEB_CAN_OK : (FEB_CAN_Status_t)(-result);
#endif
}

FEB_CAN_Status_t FEB_CAN_TX_SendSlot(int32_t handle)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (handle < 0 || handle >= FEB_CAN_MAX_TX_HANDLES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  FEB_CAN_TX_Handle_Internal_t *h = &ctx->tx_handles[handle];
  if (!h->is_active)
  {
    return FEB_CAN_ERROR_NOT_FOUND;
  }

  /* Pack data if pack function is provided */
  uint8_t tx_data[8] = {0};
  uint8_t length = 0;

  if (h->pack_func != NULL && h->data_ptr != NULL)
  {
    int pack_result = h->pack_func(tx_data, h->data_ptr, sizeof(tx_data));
    if (pack_result > 0)
    {
      length = (uint8_t)pack_result;
    }
    else
    {
      /* Pack function returned error or size */
      length = (h->data_size <= 8) ? (uint8_t)h->data_size : 8;
    }
  }
  else if (h->data_ptr != NULL)
  {
    /* Direct copy without pack function */
    length = (h->data_size <= 8) ? (uint8_t)h->data_size : 8;
    memcpy(tx_data, h->data_ptr, length);
  }

  /* Update last TX time */
  h->last_tx_time = ctx->get_tick_ms();

  return FEB_CAN_TX_Send((FEB_CAN_Instance_t)h->instance, h->can_id, (FEB_CAN_ID_Type_t)h->id_type, tx_data, length);
}

FEB_CAN_Status_t FEB_CAN_TX_SendSlotData(int32_t handle, const uint8_t *data, uint8_t length)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (handle < 0 || handle >= FEB_CAN_MAX_TX_HANDLES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  FEB_CAN_TX_Handle_Internal_t *h = &ctx->tx_handles[handle];
  if (!h->is_active)
  {
    return FEB_CAN_ERROR_NOT_FOUND;
  }

  /* Update last TX time */
  h->last_tx_time = ctx->get_tick_ms();

  return FEB_CAN_TX_Send((FEB_CAN_Instance_t)h->instance, h->can_id, (FEB_CAN_ID_Type_t)h->id_type, data, length);
}

FEB_CAN_Status_t FEB_CAN_TX_SendFromISR(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                        const uint8_t *data, uint8_t length)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (length > 8)
  {
    length = 8;
  }

#if FEB_CAN_USE_FREERTOS
  /* Queue the message (ISR-safe version) */
  FEB_CAN_Message_t msg;
  msg.can_id = can_id;
  msg.id_type = id_type;
  msg.instance = instance;
  msg.length = length;
  msg.timestamp = ctx->get_tick_ms();

  if (data != NULL && length > 0)
  {
    memcpy(msg.data, data, length);
  }
  else
  {
    memset(msg.data, 0, 8);
  }

  if (!FEB_CAN_QUEUE_SEND_ISR(ctx->tx_queue, &msg))
  {
    ctx->tx_queue_overflow_count++;
    return FEB_CAN_ERROR_QUEUE;
  }

  return FEB_CAN_OK;
#else
  /* Direct transmit in bare-metal mode */
  int result = feb_can_tx_hal_transmit(instance, can_id, id_type, data, length);
  return (result >= 0) ? FEB_CAN_OK : (FEB_CAN_Status_t)(-result);
#endif
}

/* ============================================================================
 * TX Process Functions
 * ============================================================================ */

void FEB_CAN_TX_Process(void)
{
#if FEB_CAN_USE_FREERTOS
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized || ctx->tx_queue == NULL)
  {
    return;
  }

  FEB_CAN_Message_t msg;

  /* Process pending messages */
  while (FEB_CAN_QUEUE_RECEIVE(ctx->tx_queue, &msg, 0))
  {
    /* Wait for available mailbox using semaphore */
    if (FEB_CAN_SEM_TAKE(ctx->tx_sem, FEB_CAN_TX_TIMEOUT_MS))
    {
      int result =
          feb_can_tx_hal_transmit((FEB_CAN_Instance_t)msg.instance, msg.can_id, msg.id_type, msg.data, msg.length);

      if (result < 0)
      {
        /* TX failed, give back semaphore */
        FEB_CAN_SEM_GIVE(ctx->tx_sem);
      }
      /* If successful, semaphore will be given back in TX complete callback */
    }
    else
    {
      /* Timeout waiting for mailbox - message dropped */
      ctx->tx_timeout_count++;
    }
  }
#else
  /* In bare-metal mode, TX is immediate */
#endif
}

void FEB_CAN_TX_ProcessPeriodic(void)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return;
  }

  uint32_t current_time = ctx->get_tick_ms();

  for (uint32_t i = 0; i < FEB_CAN_MAX_TX_HANDLES; i++)
  {
    FEB_CAN_TX_Handle_Internal_t *h = &ctx->tx_handles[i];

    if (!h->is_active || h->period_ms == 0)
    {
      continue;
    }

    /* Check if it's time to transmit */
    uint32_t elapsed = current_time - h->last_tx_time;
    if (elapsed >= h->period_ms)
    {
      FEB_CAN_TX_SendSlot((int32_t)i);
    }
  }
}
