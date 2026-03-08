/**
 * @file FEB_HW_Relay.c
 * @brief Hardware abstraction for relay control and sensing
 * @author Formula Electric @ Berkeley
 *
 * Pin mapping (from main.h):
 * - PC_AIR_Pin (PC2): AIR+ contactor control
 * - PC_RELAY_Pin (PD2): Precharge relay control
 * - BMS_A_Pin (PC1): BMS shutdown relay (BMS_SHUTDOWN in SN4)
 * - BMS_IND_Pin (PC0): BMS indicator output
 * - INDICATOR_Pin (PC13): Fault indicator LED
 * - BUZZER_EN_Pin (PA0): Buzzer enable
 * - AIR_M_SENSE_Pin (PC4): AIR- feedback input
 * - AIR_P_SENSE_Pin (PC5): AIR+ feedback input
 * - SHS_IN_Pin (PC12): Shutdown loop input
 * - SHS_IMD_Pin (PC10): IMD shutdown input
 * - SHS_TSMS_Pin (PC11): TSMS shutdown input
 */

#include "FEB_HW_Relay.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* ============================================================================
 * Relay Control Functions
 * ============================================================================ */

void FEB_HW_AIR_Plus_Set(bool closed)
{
  HAL_GPIO_WritePin(PC_AIR_GPIO_Port, PC_AIR_Pin, closed ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void FEB_HW_Precharge_Set(bool closed)
{
  HAL_GPIO_WritePin(PC_RELAY_GPIO_Port, PC_RELAY_Pin, closed ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void FEB_HW_BMS_Shutdown_Set(bool closed)
{
  /* BMS_A_Pin controls the BMS shutdown relay (BMS_SHUTDOWN in SN4 = PN_BMS_SHUTDOWN = PC1) */
  HAL_GPIO_WritePin(BMS_A_GPIO_Port, BMS_A_Pin, closed ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void FEB_HW_BMS_Indicator_Set(bool on)
{
  HAL_GPIO_WritePin(BMS_IND_GPIO_Port, BMS_IND_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void FEB_HW_Fault_Indicator_Set(bool on)
{
  HAL_GPIO_WritePin(INDICATOR_GPIO_Port, INDICATOR_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void FEB_HW_Buzzer_Set(bool on)
{
  HAL_GPIO_WritePin(BUZZER_EN_GPIO_Port, BUZZER_EN_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================================
 * Sense Input Functions
 * ============================================================================ */

FEB_Relay_State_t FEB_HW_AIR_Minus_Sense(void)
{
  return (FEB_Relay_State_t)HAL_GPIO_ReadPin(AIR_M_SENSE_GPIO_Port, AIR_M_SENSE_Pin);
}

FEB_Relay_State_t FEB_HW_AIR_Plus_Sense(void)
{
  return (FEB_Relay_State_t)HAL_GPIO_ReadPin(AIR_P_SENSE_GPIO_Port, AIR_P_SENSE_Pin);
}

FEB_Relay_State_t FEB_HW_Precharge_Sense(void)
{
  /* Read the precharge relay output state directly (no separate sense pin in SN5) */
  return (FEB_Relay_State_t)HAL_GPIO_ReadPin(PC_RELAY_GPIO_Port, PC_RELAY_Pin);
}

FEB_Relay_State_t FEB_HW_Shutdown_Sense(void)
{
  return (FEB_Relay_State_t)HAL_GPIO_ReadPin(SHS_IN_GPIO_Port, SHS_IN_Pin);
}

FEB_Relay_State_t FEB_HW_IMD_Sense(void)
{
  return (FEB_Relay_State_t)HAL_GPIO_ReadPin(SHS_IMD_GPIO_Port, SHS_IMD_Pin);
}

FEB_Relay_State_t FEB_HW_TSMS_Sense(void)
{
  return (FEB_Relay_State_t)HAL_GPIO_ReadPin(SHS_TSMS_GPIO_Port, SHS_TSMS_Pin);
}

bool FEB_HW_Reset_Button_Pressed(void)
{
  /* Assuming active-low button with pull-up */
  return (HAL_GPIO_ReadPin(BMS_RESET_GPIO_Port, BMS_RESET_Pin) == GPIO_PIN_RESET);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void FEB_HW_Open_All_Relays(void)
{
  FEB_HW_AIR_Plus_Set(false);
  FEB_HW_Precharge_Set(false);
  FEB_HW_BMS_Shutdown_Set(false);
}

bool FEB_HW_Is_HV_Safe(void)
{
  /* Check that both AIRs are sensed open */
  return (FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_OPEN) && (FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN);
}
