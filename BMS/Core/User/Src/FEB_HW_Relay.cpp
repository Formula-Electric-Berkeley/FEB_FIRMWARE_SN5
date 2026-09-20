/**
 * @file FEB_HW_Relay.cpp
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
 * - SHS_TSMS_Pin (PC11): TSMS indicator light output
 * - TSSI_IN_Pin (PB2): Tractive System Status Indicator (high=green, low=red)
 */

#include "FEB_HW_Relay.h"
#include "main.h"
#include "stm32f4xx_hal.h"

namespace
{

inline void write_pin(GPIO_TypeDef *port, uint16_t pin, bool high)
{
  HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

inline bool read_pin(GPIO_TypeDef *port, uint16_t pin)
{
  return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}

inline FEB_Relay_State_t read_relay_state(GPIO_TypeDef *port, uint16_t pin)
{
  return read_pin(port, pin) ? FEB_RELAY_STATE_CLOSE : FEB_RELAY_STATE_OPEN;
}

} // namespace

extern "C"
{

/* ============================================================================
 * Relay Control Functions
 * ============================================================================ */

void FEB_HW_AIR_Plus_Set(bool closed)
{
  write_pin(PC_AIR_GPIO_Port, PC_AIR_Pin, closed);
}

void FEB_HW_Precharge_Set(bool closed)
{
  write_pin(PC_RELAY_GPIO_Port, PC_RELAY_Pin, closed);
}

void FEB_HW_BMS_Shutdown_Set(bool closed)
{
  /* BMS_A_Pin controls the BMS shutdown relay (BMS_SHUTDOWN in SN4 = PN_BMS_SHUTDOWN = PC1) */
  write_pin(BMS_A_GPIO_Port, BMS_A_Pin, closed);
}

bool FEB_HW_BMS_Shutdown_Get(void)
{
  return read_pin(BMS_A_GPIO_Port, BMS_A_Pin);
}

bool FEB_HW_BMS_Indicator_Get(void)
{
  return read_pin(BMS_IND_GPIO_Port, BMS_IND_Pin);
}

void FEB_HW_BMS_Indicator_Set(bool on)
{
  write_pin(BMS_IND_GPIO_Port, BMS_IND_Pin, on);
}

void FEB_HW_Fault_Indicator_Set(bool on)
{
  write_pin(INDICATOR_GPIO_Port, INDICATOR_Pin, on);
}

void FEB_HW_Buzzer_Set(bool on)
{
  write_pin(BUZZER_EN_GPIO_Port, BUZZER_EN_Pin, on);
}

void FEB_HW_TSSI_Set(bool green)
{
  /* TSSI_IN (PB2): high = green (healthy), low = red (fault). */
  write_pin(TSSI_IN_GPIO_Port, TSSI_IN_Pin, green);
}

/* ============================================================================
 * Sense Input Functions
 * ============================================================================ */

FEB_Relay_State_t FEB_HW_AIR_Minus_Sense(void)
{
  return read_relay_state(AIR_M_SENSE_GPIO_Port, AIR_M_SENSE_Pin);
}

FEB_Relay_State_t FEB_HW_AIR_Plus_Sense(void)
{
  return read_relay_state(AIR_P_SENSE_GPIO_Port, AIR_P_SENSE_Pin);
}

FEB_Relay_State_t FEB_HW_Precharge_Sense(void)
{
  /* Read the precharge relay output state directly (no separate sense pin in SN5) */
  return read_relay_state(PC_RELAY_GPIO_Port, PC_RELAY_Pin);
}

FEB_Relay_State_t FEB_HW_Shutdown_Sense(void)
{
  return read_relay_state(SHS_IN_GPIO_Port, SHS_IN_Pin);
}

FEB_Relay_State_t FEB_HW_IMD_Sense(void)
{
  return read_relay_state(SHS_IMD_GPIO_Port, SHS_IMD_Pin);
}

void FEB_HW_TSMS_Indicator_Set(bool on)
{
  write_pin(SHS_TSMS_GPIO_Port, SHS_TSMS_Pin, on);
}

bool FEB_HW_TSMS_Indicator_Get(void)
{
  return read_pin(SHS_TSMS_GPIO_Port, SHS_TSMS_Pin);
}

bool FEB_HW_Reset_Button_Pressed(void)
{
  /* Assuming active-low button with pull-up */
  return !read_pin(BMS_RESET_GPIO_Port, BMS_RESET_Pin);
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

} // extern "C"
