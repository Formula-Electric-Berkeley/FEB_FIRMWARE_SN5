/**
 * @file FEB_HW_Relay.h
 * @brief Hardware abstraction for relay control and sensing
 * @author Formula Electric @ Berkeley
 *
 * Provides GPIO abstraction for:
 * - AIR+ contactor control (PC_AIR_Pin)
 * - Precharge relay control (PC_RELAY_Pin)
 * - BMS indicator output (BMS_IND_Pin)
 * - Fault indicator LED (INDICATOR_Pin)
 * - Buzzer control (BUZZER_EN_Pin)
 * - AIR/Shutdown sensing inputs
 */

#ifndef INC_FEB_HW_RELAY_H_
#define INC_FEB_HW_RELAY_H_

#include <stdbool.h>
#include <stdint.h>

/* Relay state definitions (matches GPIO logic) */
typedef enum
{
  FEB_RELAY_STATE_OPEN = 0, /* GPIO_PIN_RESET = relay open */
  FEB_RELAY_STATE_CLOSE = 1 /* GPIO_PIN_SET = relay closed */
} FEB_Relay_State_t;

/* ============================================================================
 * Relay Control Functions
 * ============================================================================ */

/**
 * @brief Set AIR+ (main contactor) state
 * @param closed true = close contactor, false = open contactor
 */
void FEB_HW_AIR_Plus_Set(bool closed);

/**
 * @brief Set precharge relay state
 * @param closed true = close relay, false = open relay
 */
void FEB_HW_Precharge_Set(bool closed);

/**
 * @brief Set BMS shutdown relay state
 * @param closed true = close relay (enable HV), false = open relay (disable HV)
 */
void FEB_HW_BMS_Shutdown_Set(bool closed);

/**
 * @brief Read back the actual BMS shutdown relay pin level (PC1)
 * @return true if the pin is high (relay commanded closed)
 */
bool FEB_HW_BMS_Shutdown_Get(void);

/**
 * @brief Set BMS indicator output
 * @param on true = indicator on, false = indicator off
 */
void FEB_HW_BMS_Indicator_Set(bool on);

/**
 * @brief Read back the actual BMS indicator pin level (PC0)
 * @return true if the indicator output is driven on
 */
bool FEB_HW_BMS_Indicator_Get(void);

/**
 * @brief Set fault indicator LED
 * @param on true = LED on (fault active), false = LED off
 */
void FEB_HW_Fault_Indicator_Set(bool on);

/**
 * @brief Set buzzer state
 * @param on true = buzzer on, false = buzzer off
 */
void FEB_HW_Buzzer_Set(bool on);

/**
 * @brief Set the Tractive System Status Indicator (TSSI_IN, PB2)
 * @param green true = high (green / healthy), false = low (red / fault)
 */
void FEB_HW_TSSI_Set(bool green);

/* ============================================================================
 * Sense Input Functions
 * ============================================================================ */

/**
 * @brief Read AIR- (main contactor) sense input
 * @return FEB_RELAY_STATE_CLOSE if contactor is closed, FEB_RELAY_STATE_OPEN otherwise
 */
FEB_Relay_State_t FEB_HW_AIR_Minus_Sense(void);

/**
 * @brief Read AIR+ sense input
 * @return FEB_RELAY_STATE_CLOSE if contactor is closed, FEB_RELAY_STATE_OPEN otherwise
 */
FEB_Relay_State_t FEB_HW_AIR_Plus_Sense(void);

/**
 * @brief Read precharge relay sense
 * @return FEB_RELAY_STATE_CLOSE if relay is closed, FEB_RELAY_STATE_OPEN otherwise
 */
FEB_Relay_State_t FEB_HW_Precharge_Sense(void);

/**
 * @brief Read shutdown loop input (SHS_IN)
 * @return FEB_RELAY_STATE_CLOSE if shutdown loop is complete, FEB_RELAY_STATE_OPEN otherwise
 */
FEB_Relay_State_t FEB_HW_Shutdown_Sense(void);

/**
 * @brief Read IMD shutdown sense input
 * @return FEB_RELAY_STATE_CLOSE if IMD OK, FEB_RELAY_STATE_OPEN if IMD fault
 */
FEB_Relay_State_t FEB_HW_IMD_Sense(void);

/**
 * @brief Drive the TSMS indicator light output (PC11)
 */
void FEB_HW_TSMS_Indicator_Set(bool on);

/**
 * @brief Read back the commanded TSMS indicator light state
 * @return true if the indicator output is driven on
 */
bool FEB_HW_TSMS_Indicator_Get(void);

/**
 * @brief Read BMS reset button state
 * @return true if button is pressed (active low)
 */
bool FEB_HW_Reset_Button_Pressed(void);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Open all high-voltage relays (safe state)
 * Opens AIR+, precharge, and BMS shutdown relay
 */
void FEB_HW_Open_All_Relays(void);

/**
 * @brief Check if all HV paths are safe (all relays open)
 * @return true if all relays sensed open, false otherwise
 */
bool FEB_HW_Is_HV_Safe(void);

#endif /* INC_FEB_HW_RELAY_H_ */
