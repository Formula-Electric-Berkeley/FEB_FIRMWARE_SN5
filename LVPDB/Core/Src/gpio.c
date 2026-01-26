/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    gpio.c
 * @brief   This file provides code for the configuration
 *          of all used GPIO pins.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
 * Analog
 * Input
 * Output
 * EVENT_OUT
 * EXTI
 */
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, CP_RF_EN_Pin | AF1_AF2_EN_Pin | SM_EN_Pin | BM_L_EN_Pin | A0_Pin | A1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BL_Switch_Pin | LT_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(A2_GPIO_Port, A2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SH_EN_GPIO_Port, SH_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SH_PG_Pin SH_Alert_Pin CP_RF_PG_Pin CP_RF_Alert_Pin
                           AF1_AF2_PG_Pin SM_PG_Pin SM_Alert_Pin */
  GPIO_InitStruct.Pin =
      SH_PG_Pin | SH_Alert_Pin | CP_RF_PG_Pin | CP_RF_Alert_Pin | AF1_AF2_PG_Pin | SM_PG_Pin | SM_Alert_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : CP_RF_EN_Pin AF1_AF2_EN_Pin SM_EN_Pin BM_L_EN_Pin
                           A0_Pin A1_Pin */
  GPIO_InitStruct.Pin = CP_RF_EN_Pin | AF1_AF2_EN_Pin | SM_EN_Pin | BM_L_EN_Pin | A0_Pin | A1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BL_Switch_Pin LT_EN_Pin */
  GPIO_InitStruct.Pin = BL_Switch_Pin | LT_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LT_Alert_Pin LT_PG_Pin AF1_AF2_Alert_Pin BM_L_Alert_Pin
                           BM_L_PG_Pin */
  GPIO_InitStruct.Pin = LT_Alert_Pin | LT_PG_Pin | AF1_AF2_Alert_Pin | BM_L_Alert_Pin | BM_L_PG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : DSMS_ON_Pin */
  GPIO_InitStruct.Pin = DSMS_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DSMS_ON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : IS_Vout_Pin */
  GPIO_InitStruct.Pin = IS_Vout_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IS_Vout_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : A2_Pin */
  GPIO_InitStruct.Pin = A2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(A2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SH_EN_Pin */
  GPIO_InitStruct.Pin = SH_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SH_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LV_PG_Pin LV_Alert_Pin */
  GPIO_InitStruct.Pin = LV_PG_Pin | LV_Alert_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
