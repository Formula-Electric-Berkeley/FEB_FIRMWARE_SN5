/**
 ******************************************************************************
 * @file           : feb_can_filter.c
 * @brief          : Filter management for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "feb_can_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ============================================================================
 * Filter Bank Helpers
 * ============================================================================ */

static uint8_t feb_can_get_filter_bank_start(FEB_CAN_Instance_t instance)
{
  if (instance == FEB_CAN_INSTANCE_1)
  {
    return FEB_CAN_CAN1_FILTER_BANK_START;
  }
  else
  {
    return FEB_CAN_CAN2_FILTER_BANK_START;
  }
}

static uint8_t feb_can_get_filter_bank_end(FEB_CAN_Instance_t instance)
{
  if (instance == FEB_CAN_INSTANCE_1)
  {
    return FEB_CAN_CAN2_FILTER_BANK_START; /* CAN1 ends where CAN2 starts */
  }
  else
  {
    return FEB_CAN_TOTAL_FILTER_BANKS;
  }
}

/* ============================================================================
 * Filter Configuration API
 * ============================================================================ */

FEB_CAN_Status_t FEB_CAN_Filter_Configure(FEB_CAN_Instance_t instance, uint8_t filter_bank, uint32_t id, uint32_t mask,
                                          FEB_CAN_ID_Type_t id_type, FEB_CAN_FIFO_t fifo)
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

  /* Validate filter bank range */
  uint8_t start = feb_can_get_filter_bank_start(instance);
  uint8_t end = feb_can_get_filter_bank_end(instance);

  if (filter_bank < start || filter_bank >= end)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)ctx->hcan[instance];
  if (hcan == NULL)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  /* Configure filter */
  CAN_FilterTypeDef filter_config = {0};
  filter_config.FilterBank = filter_bank;
  filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
  filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
  filter_config.FilterFIFOAssignment = (fifo == FEB_CAN_FIFO_0) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
  filter_config.FilterActivation = ENABLE;
  filter_config.SlaveStartFilterBank = FEB_CAN_CAN2_FILTER_BANK_START;

  if (id_type == FEB_CAN_ID_STD)
  {
    /* Standard ID format: ID in bits [31:21] of FilterIdHigh */
    filter_config.FilterIdHigh = (uint16_t)((id << 5) & 0xFFFF);
    filter_config.FilterIdLow = 0x0000;
    filter_config.FilterMaskIdHigh = (uint16_t)((mask << 5) & 0xFFFF);
    filter_config.FilterMaskIdLow = 0x0000;
  }
  else
  {
    /* Extended ID format: Full 29-bit ID */
    filter_config.FilterIdHigh = (uint16_t)((id >> 13) & 0xFFFF);
    filter_config.FilterIdLow = (uint16_t)(((id << 3) & 0xFFF8) | CAN_ID_EXT);
    filter_config.FilterMaskIdHigh = (uint16_t)((mask >> 13) & 0xFFFF);
    filter_config.FilterMaskIdLow = (uint16_t)(((mask << 3) & 0xFFF8) | CAN_ID_EXT);
  }

  /* Use CAN1 handle for filter configuration (required by HAL) */
  CAN_HandleTypeDef *filter_hcan = (CAN_HandleTypeDef *)ctx->hcan[FEB_CAN_INSTANCE_1];
  if (filter_hcan == NULL)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  if (HAL_CAN_ConfigFilter(filter_hcan, &filter_config) != HAL_OK)
  {
    return FEB_CAN_ERROR_HAL;
  }

  /* Track filter in context */
  ctx->filters[filter_bank].id = id;
  ctx->filters[filter_bank].mask = mask;
  ctx->filters[filter_bank].id_type = id_type;
  ctx->filters[filter_bank].fifo = fifo;
  ctx->filters[filter_bank].is_active = true;
  ctx->filters[filter_bank].mode = CAN_FILTERMODE_IDMASK;

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_Filter_AcceptAll(FEB_CAN_Instance_t instance, uint8_t filter_bank, FEB_CAN_FIFO_t fifo)
{
  /* Accept all by using mask = 0 (all bits match) */
  return FEB_CAN_Filter_Configure(instance, filter_bank, 0x00000000, 0x00000000, FEB_CAN_ID_STD, fifo);
}

/* ============================================================================
 * Disable Filter Helper
 * ============================================================================ */

static FEB_CAN_Status_t feb_can_filter_disable(uint8_t filter_bank)
{
  FEB_CAN_Context_t *ctx = feb_can_get_context();

  if (!ctx->initialized)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  CAN_HandleTypeDef *hcan = (CAN_HandleTypeDef *)ctx->hcan[FEB_CAN_INSTANCE_1];
  if (hcan == NULL)
  {
    return FEB_CAN_ERROR_NOT_INIT;
  }

  CAN_FilterTypeDef filter_config = {0};
  filter_config.FilterBank = filter_bank;
  filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
  filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
  filter_config.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter_config.FilterActivation = DISABLE;
  filter_config.SlaveStartFilterBank = FEB_CAN_CAN2_FILTER_BANK_START;

  if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK)
  {
    return FEB_CAN_ERROR_HAL;
  }

  /* Clear filter tracking */
  memset(&ctx->filters[filter_bank], 0, sizeof(FEB_CAN_Filter_Entry_t));

  return FEB_CAN_OK;
}

/* ============================================================================
 * Update Filters From Registry
 * ============================================================================ */

FEB_CAN_Status_t FEB_CAN_Filter_UpdateFromRegistry(FEB_CAN_Instance_t instance)
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

  uint8_t filter_start = feb_can_get_filter_bank_start(instance);
  uint8_t filter_end = feb_can_get_filter_bank_end(instance);
  uint8_t current_filter = filter_start;

  /* Collect unique IDs from RX handles for this instance */
  bool has_wildcard = false;
  uint32_t unique_ids[FEB_CAN_MAX_RX_HANDLES];
  uint8_t unique_id_types[FEB_CAN_MAX_RX_HANDLES];
  uint32_t unique_masks[FEB_CAN_MAX_RX_HANDLES];
  uint8_t unique_fifos[FEB_CAN_MAX_RX_HANDLES];
  uint8_t unique_filter_types[FEB_CAN_MAX_RX_HANDLES];
  uint32_t unique_count = 0;

  for (uint32_t i = 0; i < FEB_CAN_MAX_RX_HANDLES; i++)
  {
    FEB_CAN_RX_Handle_Internal_t *handle = &ctx->rx_handles[i];

    if (!handle->is_active || handle->instance != instance)
    {
      continue;
    }

    if (handle->filter_type == FEB_CAN_FILTER_WILDCARD)
    {
      has_wildcard = true;
      continue;
    }

    /* Check if this ID/mask combination is already tracked */
    bool found = false;
    for (uint32_t j = 0; j < unique_count; j++)
    {
      if (unique_ids[j] == handle->can_id && unique_id_types[j] == handle->id_type && unique_masks[j] == handle->mask &&
          unique_filter_types[j] == handle->filter_type)
      {
        found = true;
        break;
      }
    }

    if (!found && unique_count < FEB_CAN_MAX_RX_HANDLES)
    {
      unique_ids[unique_count] = handle->can_id;
      unique_id_types[unique_count] = handle->id_type;
      unique_masks[unique_count] = handle->mask;
      unique_fifos[unique_count] = handle->fifo;
      unique_filter_types[unique_count] = handle->filter_type;
      unique_count++;
    }
  }

  /* If wildcard is present, configure one filter to accept all */
  if (has_wildcard)
  {
    if (current_filter < filter_end)
    {
      FEB_CAN_Filter_AcceptAll(instance, current_filter, FEB_CAN_FIFO_0);
      current_filter++;
    }
  }
  else if (unique_count == 0)
  {
    /* No handlers registered - configure reject-all filter */
    /* Use mask that matches nothing (ID = max, mask = max) */
    if (current_filter < filter_end)
    {
      FEB_CAN_Filter_Configure(instance, current_filter, 0x1FFFFFFF, 0x1FFFFFFF, FEB_CAN_ID_EXT, FEB_CAN_FIFO_0);
      current_filter++;
    }
  }
  else
  {
    /* Configure one filter per unique ID */
    for (uint32_t i = 0; i < unique_count && current_filter < filter_end; i++)
    {
      uint32_t mask;
      if (unique_filter_types[i] == FEB_CAN_FILTER_MASK)
      {
        mask = unique_masks[i];
      }
      else
      {
        /* Exact match */
        mask = (unique_id_types[i] == FEB_CAN_ID_STD) ? 0x7FF : 0x1FFFFFFF;
      }

      FEB_CAN_Filter_Configure(instance, current_filter, unique_ids[i], mask, (FEB_CAN_ID_Type_t)unique_id_types[i],
                               (FEB_CAN_FIFO_t)unique_fifos[i]);
      current_filter++;
    }
  }

  /* Disable remaining filter banks for this instance */
  while (current_filter < filter_end)
  {
    feb_can_filter_disable(current_filter);
    current_filter++;
  }

  return FEB_CAN_OK;
}
