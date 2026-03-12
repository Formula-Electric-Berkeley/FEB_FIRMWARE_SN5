/**
 ******************************************************************************
 * @file           : feb_can_rx.c
 * @brief          : RX handling for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "feb_can_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ============================================================================
 * Internal RX Dispatch Function
 * ============================================================================ */

void feb_can_rx_dispatch(FEB_CAN_Instance_t instance, uint32_t can_id, uint8_t id_type, const uint8_t *data,
                         uint8_t length, uint32_t timestamp)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  for (uint32_t i = 0; i < FEB_CAN_MAX_RX_HANDLES; i++)
  {
    FEB_CAN_RX_Handle_Internal_t *handle = &ctx->rx_handles[i];

    if (!handle->is_active)
    {
      continue;
    }

    /* Check instance */
    if (handle->instance != instance)
    {
      continue;
    }

    /* Check ID type */
    if (handle->id_type != id_type)
    {
      continue;
    }

    /* Check filter match */
    bool match = false;
    switch (handle->filter_type)
    {
    case FEB_CAN_FILTER_EXACT:
      match = (can_id == handle->can_id);
      break;

    case FEB_CAN_FILTER_MASK:
      match = ((can_id & handle->mask) == (handle->can_id & handle->mask));
      break;

    case FEB_CAN_FILTER_WILDCARD:
      match = true;
      break;

    default:
      break;
    }

    if (!match)
    {
      continue;
    }

    /* Invoke callback */
    if (handle->is_extended_cb)
    {
      FEB_CAN_RX_Extended_Callback_t ext_cb = (FEB_CAN_RX_Extended_Callback_t)handle->callback;
      if (ext_cb != NULL)
      {
        ext_cb(instance, can_id, (FEB_CAN_ID_Type_t)id_type, data, length, timestamp, 0, handle->user_data);
      }
    }
    else
    {
      FEB_CAN_RX_Callback_t cb = (FEB_CAN_RX_Callback_t)handle->callback;
      if (cb != NULL)
      {
        cb(instance, can_id, (FEB_CAN_ID_Type_t)id_type, data, length, handle->user_data);
      }
    }
  }
}

/* ============================================================================
 * RX Registration API
 * ============================================================================ */

int32_t FEB_CAN_RX_Register(const FEB_CAN_RX_Params_t *params)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return -FEB_CAN_ERROR_NOT_INIT;
  }

  if (params == NULL || params->callback == NULL)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (params->instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_LOCK(ctx->rx_mutex);
#endif

  /* Check for duplicate registration (except for wildcard) */
  if (params->filter_type != FEB_CAN_FILTER_WILDCARD)
  {
    for (uint32_t i = 0; i < FEB_CAN_MAX_RX_HANDLES; i++)
    {
      FEB_CAN_RX_Handle_Internal_t *handle = &ctx->rx_handles[i];
      if (handle->is_active && handle->instance == params->instance && handle->can_id == params->can_id &&
          handle->id_type == params->id_type && handle->filter_type == params->filter_type)
      {
#if FEB_CAN_USE_FREERTOS
        FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif
        return -FEB_CAN_ERROR_ALREADY_EXISTS;
      }
    }
  }

  /* Find free slot */
  int32_t free_slot = -1;
  for (uint32_t i = 0; i < FEB_CAN_MAX_RX_HANDLES; i++)
  {
    if (!ctx->rx_handles[i].is_active)
    {
      free_slot = (int32_t)i;
      break;
    }
  }

  if (free_slot < 0)
  {
#if FEB_CAN_USE_FREERTOS
    FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif
    return -FEB_CAN_ERROR_FULL;
  }

  /* Fill handle */
  FEB_CAN_RX_Handle_Internal_t *handle = &ctx->rx_handles[free_slot];
  handle->instance = params->instance;
  handle->can_id = params->can_id;
  handle->id_type = params->id_type;
  handle->filter_type = params->filter_type;
  handle->mask = (params->filter_type == FEB_CAN_FILTER_MASK) ? params->mask : 0xFFFFFFFF;
  handle->fifo = params->fifo;
  handle->callback = (void *)params->callback;
  handle->user_data = params->user_data;
  handle->is_extended_cb = false;
  handle->is_active = true;

  ctx->rx_handle_count++;

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif

  /* Update hardware filters */
  FEB_CAN_Filter_UpdateFromRegistry(params->instance);

  return free_slot;
}

int32_t FEB_CAN_RX_RegisterExtended(const FEB_CAN_RX_Params_t *params, FEB_CAN_RX_Extended_Callback_t ext_callback)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return -FEB_CAN_ERROR_NOT_INIT;
  }

  if (params == NULL || ext_callback == NULL)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (params->instance >= FEB_CAN_INSTANCE_COUNT)
  {
    return -FEB_CAN_ERROR_INVALID_PARAM;
  }

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_LOCK(ctx->rx_mutex);
#endif

  /* Find free slot */
  int32_t free_slot = -1;
  for (uint32_t i = 0; i < FEB_CAN_MAX_RX_HANDLES; i++)
  {
    if (!ctx->rx_handles[i].is_active)
    {
      free_slot = (int32_t)i;
      break;
    }
  }

  if (free_slot < 0)
  {
#if FEB_CAN_USE_FREERTOS
    FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif
    return -FEB_CAN_ERROR_FULL;
  }

  /* Fill handle */
  FEB_CAN_RX_Handle_Internal_t *handle = &ctx->rx_handles[free_slot];
  handle->instance = params->instance;
  handle->can_id = params->can_id;
  handle->id_type = params->id_type;
  handle->filter_type = params->filter_type;
  handle->mask = (params->filter_type == FEB_CAN_FILTER_MASK) ? params->mask : 0xFFFFFFFF;
  handle->fifo = params->fifo;
  handle->callback = (void *)ext_callback;
  handle->user_data = params->user_data;
  handle->is_extended_cb = true;
  handle->is_active = true;

  ctx->rx_handle_count++;

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif

  /* Update hardware filters */
  FEB_CAN_Filter_UpdateFromRegistry(params->instance);

  return free_slot;
}

FEB_CAN_Status_t FEB_CAN_RX_Unregister(int32_t handle)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (handle < 0 || handle >= FEB_CAN_MAX_RX_HANDLES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_LOCK(ctx->rx_mutex);
#endif

  FEB_CAN_RX_Handle_Internal_t *h = &ctx->rx_handles[handle];
  if (!h->is_active)
  {
#if FEB_CAN_USE_FREERTOS
    FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif
    return FEB_CAN_ERROR_NOT_FOUND;
  }

  FEB_CAN_Instance_t instance = (FEB_CAN_Instance_t)h->instance;

  /* Clear handle */
  memset(h, 0, sizeof(FEB_CAN_RX_Handle_Internal_t));
  ctx->rx_handle_count--;

#if FEB_CAN_USE_FREERTOS
  FEB_CAN_MUTEX_UNLOCK(ctx->rx_mutex);
#endif

  /* Update hardware filters */
  FEB_CAN_Filter_UpdateFromRegistry(instance);

  return FEB_CAN_OK;
}

bool FEB_CAN_RX_IsRegistered(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return false;
  }

  for (uint32_t i = 0; i < FEB_CAN_MAX_RX_HANDLES; i++)
  {
    FEB_CAN_RX_Handle_Internal_t *handle = &ctx->rx_handles[i];
    if (handle->is_active && handle->instance == instance && handle->can_id == can_id && handle->id_type == id_type)
    {
      return true;
    }
  }

  return false;
}

uint32_t FEB_CAN_RX_GetRegisteredCount(void)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();
  return ctx->rx_handle_count;
}

/* ============================================================================
 * RX Process Function (FreeRTOS mode)
 * ============================================================================ */

void FEB_CAN_RX_Process(void)
{
#if FEB_CAN_USE_FREERTOS
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized || ctx->rx_queue == NULL)
  {
    return;
  }

  FEB_CAN_Message_t msg;

  /* Process all pending messages */
  while (FEB_CAN_QUEUE_RECEIVE(ctx->rx_queue, &msg, 0))
  {
    feb_can_rx_dispatch((FEB_CAN_Instance_t)msg.instance, msg.can_id, msg.id_type, msg.data, msg.length, msg.timestamp);
  }
#else
  /* In bare-metal mode, callbacks are invoked directly in ISR */
#endif
}
