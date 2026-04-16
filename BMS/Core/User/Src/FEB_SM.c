/**
 * @file FEB_SM.c
 * @brief BMS State Machine Implementation
 * @author Formula Electric @ Berkeley
 *
 * Implements the BMS state machine with 14 transition functions:
 * - bootTransition, LVPowerTransition, HealthCheckTransition
 * - PrechargeTransition, EnergizedTransition, DriveTransition
 * - FreeTransition, ChargingPrechargeTransition, ChargingTransition
 * - BalanceTransition
 * - BMSFaultTransition, BSPDFaultTransition, IMDFaultTransition, ChargingFaultTransition
 *
 * Based on SN4 FEB_SM.c with adaptations for SN5 architecture.
 */

#include "FEB_SM.h"
#include "FEB_HW_Relay.h"
#include "FEB_CAN_State.h"
#include "FEB_CAN_DASH.h"
#include "FEB_ADBMS_App.h"
#include "FEB_CAN_IVT.h"
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

/* Logging tag for state machine */
#define TAG_SM "[SM]"

/* Critical section macros for ISR/task shared variables */
#define SM_ENTER_CRITICAL()                                                                                            \
  uint32_t _primask = __get_PRIMASK();                                                                                 \
  __disable_irq()
#define SM_EXIT_CRITICAL() __set_PRIMASK(_primask)

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Precharge voltage threshold (90% of pack voltage) */
#define PRECHARGE_THRESHOLD_PCT 0.90f

/* R2D timeout for freshness check */
#define R2D_TIMEOUT_MS 500

/* Contactor settling delay in milliseconds */
#define CONTACTOR_SETTLE_DELAY_MS 500

/* Precharge timeout - abort if not complete within this time */
#define PRECHARGE_TIMEOUT_MS 10000

/* ============================================================================
 * Internal State
 * ============================================================================ */

/* Current state machine state (volatile for ISR/task access) */
static volatile BMS_State_t SM_Current_State = BMS_STATE_BOOT;

/* Non-blocking delay state for fault handling */
static volatile bool fault_pending = false;
static volatile uint32_t fault_delay_start = 0;
static volatile BMS_State_t pending_fault_type = BMS_STATE_BOOT;

/* Non-blocking delay state for precharge->energized transition */
static volatile bool energize_pending = false;
static volatile uint32_t energize_delay_start = 0;

/* Non-blocking delay state for charger precharge->charging transition */
static volatile bool charging_pending = false;
static volatile uint32_t charging_delay_start = 0;

/* Precharge start time for timeout */
static volatile uint32_t precharge_start_time = 0;
static volatile uint32_t charger_precharge_start_time = 0;

/* Reset button state tracking */
static volatile bool reset_button_last_state = false;
static volatile uint32_t reset_button_debounce_tick = 0;
#define RESET_BUTTON_DEBOUNCE_MS 50

/* Shutdown sense debounce - require multiple consecutive readings to filter transients */
static volatile uint8_t shutdown_open_count = 0;
#define SHUTDOWN_DEBOUNCE_COUNT 3 /* Require 3 consecutive OPEN readings */

/* Special DEFAULT value for transition function calls during FEB_SM_Process */
#define BMS_STATE_DEFAULT BMS_STATE_COUNT

/* ============================================================================
 * Forward Declarations - Helper Functions
 * ============================================================================ */

static bool isFaultState(BMS_State_t state);
static void fault_begin(BMS_State_t fault_type);
static bool fault_process(void);
static void check_reset_button(void);

/* ============================================================================
 * Forward Declarations - Transition Functions
 * ============================================================================ */

static void bootTransition(BMS_State_t next_state);
static void LVPowerTransition(BMS_State_t next_state);
static void HealthCheckTransition(BMS_State_t next_state);
static void PrechargeTransition(BMS_State_t next_state);
static void EnergizedTransition(BMS_State_t next_state);
static void DriveTransition(BMS_State_t next_state);
static void FreeTransition(BMS_State_t next_state);
static void ChargingPrechargeTransition(BMS_State_t next_state);
static void ChargingTransition(BMS_State_t next_state);
static void BalanceTransition(BMS_State_t next_state);
static void BMSFaultTransition(BMS_State_t next_state);
static void BSPDFaultTransition(BMS_State_t next_state);
static void IMDFaultTransition(BMS_State_t next_state);
static void ChargingFaultTransition(BMS_State_t next_state);

/* Transition function vector indexed by state */
static void (*transitionVector[14])(BMS_State_t) = {
    bootTransition,              /* BMS_STATE_BOOT */
    LVPowerTransition,           /* BMS_STATE_LV_POWER */
    HealthCheckTransition,       /* BMS_STATE_BUS_HEALTH_CHECK */
    PrechargeTransition,         /* BMS_STATE_PRECHARGE */
    EnergizedTransition,         /* BMS_STATE_ENERGIZED */
    DriveTransition,             /* BMS_STATE_DRIVE */
    FreeTransition,              /* BMS_STATE_BATTERY_FREE */
    ChargingPrechargeTransition, /* BMS_STATE_CHARGER_PRECHARGE */
    ChargingTransition,          /* BMS_STATE_CHARGING */
    BalanceTransition,           /* BMS_STATE_BALANCE */
    BMSFaultTransition,          /* BMS_STATE_FAULT_BMS */
    BSPDFaultTransition,         /* BMS_STATE_FAULT_BSPD */
    IMDFaultTransition,          /* BMS_STATE_FAULT_IMD */
    ChargingFaultTransition,     /* BMS_STATE_FAULT_CHARGING */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Update state with protection against fault state exit
 */
static BMS_State_t updateStateProtected(BMS_State_t next_state)
{
  /* Don't allow exit from BMS fault state */
  if (SM_Current_State == BMS_STATE_FAULT_BMS)
  {
    return BMS_STATE_FAULT_BMS;
  }

  BMS_State_t prev_state = SM_Current_State;
  SM_Current_State = next_state;

  /* Log state transition */
  LOG_I(TAG_SM, "%s -> %s", FEB_CAN_State_GetStateName(prev_state), FEB_CAN_State_GetStateName(next_state));

  /* Update CAN-published state */
  FEB_CAN_State_SetState(next_state);
  return next_state;
}

/**
 * @brief Begin fault state entry - starts non-blocking delay
 * @note Opens shutdown relay immediately, AIR+/precharge after delay
 */
static void fault_begin(BMS_State_t fault_type)
{
  /* Don't re-enter if already in fault */
  if (fault_pending || isFaultState(SM_Current_State))
  {
    return;
  }

  LOG_E(TAG_SM, "FAULT ENTRY: %s", FEB_CAN_State_GetStateName(fault_type));

  SM_Current_State = fault_type;
  FEB_CAN_State_SetState(fault_type);

  /* Open BMS shutdown relay immediately (disables HV path) */
  FEB_HW_BMS_Shutdown_Set(false);
  LOG_W(TAG_SM, "BMS shutdown relay opened");

  /* Stop cell balancing after relay is open */
  FEB_Stop_Balance();

  /* Turn on fault indicator */
  FEB_HW_Fault_Indicator_Set(true);

  /* Start non-blocking delay for contactor opening */
  fault_pending = true;
  fault_delay_start = HAL_GetTick();
  pending_fault_type = fault_type;
}

/**
 * @brief Process pending fault delay
 * @return true if fault handling is complete
 */
static bool fault_process(void)
{
  if (!fault_pending)
  {
    return true;
  }

  /* Check if delay has elapsed */
  if ((HAL_GetTick() - fault_delay_start) >= CONTACTOR_SETTLE_DELAY_MS)
  {
    /* Open AIR+ and precharge for redundancy */
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    fault_pending = false;
    LOG_W(TAG_SM, "Fault settling complete, contactors opened");
    return true;
  }

  return false;
}

/**
 * @brief Check if target is a fault state
 */
static bool isFaultState(BMS_State_t state)
{
  return (state == BMS_STATE_FAULT_BMS || state == BMS_STATE_FAULT_BSPD || state == BMS_STATE_FAULT_IMD ||
          state == BMS_STATE_FAULT_CHARGING);
}

/**
 * @brief Check reset button state with debouncing
 */
static void check_reset_button(void)
{
  bool current_state = FEB_HW_Reset_Button_Pressed();

  /* Debounce: only register change if stable for debounce period */
  if (current_state != reset_button_last_state)
  {
    if ((HAL_GetTick() - reset_button_debounce_tick) >= RESET_BUTTON_DEBOUNCE_MS)
    {
      reset_button_last_state = current_state;
      reset_button_debounce_tick = HAL_GetTick();

      if (current_state)
      {
        LOG_I(TAG_SM, "RESET BUTTON PRESSED");
      }
      else
      {
        LOG_D(TAG_SM, "Reset button released");
      }
    }
  }
  else
  {
    reset_button_debounce_tick = HAL_GetTick();
  }
}

/* ============================================================================
 * Public Interface
 * ============================================================================ */

void FEB_SM_Init(void)
{
  LOG_I(TAG_SM, "State machine initializing");

  SM_Current_State = BMS_STATE_BOOT;

  /* Open AIR+ and precharge relays (safe state) */
  FEB_HW_AIR_Plus_Set(false);
  FEB_HW_Precharge_Set(false);
  LOG_D(TAG_SM, "AIR+ and precharge relays opened");

  /* Reset indicators */
  FEB_HW_BMS_Indicator_Set(false);
  FEB_HW_Fault_Indicator_Set(false);

  /* Close BMS shutdown relay (enables HV path when shutdown loop complete) */
  FEB_HW_BMS_Shutdown_Set(true);
  LOG_D(TAG_SM, "BMS shutdown relay closed");

  /* Update CAN state */
  FEB_CAN_State_SetState(BMS_STATE_BOOT);

  /* Transition to LV_POWER */
  LOG_I(TAG_SM, "Init complete, transitioning to LV_POWER");
  FEB_SM_Transition(BMS_STATE_LV_POWER);
}

BMS_State_t FEB_SM_Get_Current_State(void)
{
  return SM_Current_State;
}

void FEB_SM_Transition(BMS_State_t next_state)
{
  if (SM_Current_State < BMS_STATE_COUNT)
  {
    transitionVector[SM_Current_State](next_state);
  }
}

void FEB_SM_Process(void)
{
  /* Check reset button */
  check_reset_button();

  /* Process pending fault delay first */
  fault_process();

  /* Process pending energize delay (precharge->energized) */
  if (energize_pending)
  {
    if ((HAL_GetTick() - energize_delay_start) >= CONTACTOR_SETTLE_DELAY_MS)
    {
      /* Open precharge after AIR+ has settled */
      FEB_HW_Precharge_Set(false);
      updateStateProtected(BMS_STATE_ENERGIZED);
      energize_pending = false;
    }
    return; /* Don't process normal transitions while pending */
  }

  /* Process pending charging delay (charger precharge->charging) */
  if (charging_pending)
  {
    if ((HAL_GetTick() - charging_delay_start) >= CONTACTOR_SETTLE_DELAY_MS)
    {
      /* Open precharge after AIR+ has settled */
      FEB_HW_Precharge_Set(false);
      updateStateProtected(BMS_STATE_CHARGING);
      charging_pending = false;
    }
    return; /* Don't process normal transitions while pending */
  }

  /* Call current state's transition function with DEFAULT */
  if (SM_Current_State < BMS_STATE_COUNT)
  {
    transitionVector[SM_Current_State](BMS_STATE_DEFAULT);
  }
}

void FEB_SM_Fault(BMS_State_t fault_type)
{
  if (isFaultState(fault_type))
  {
    fault_begin(fault_type);
  }
}

bool FEB_SM_Is_Faulted(void)
{
  return isFaultState(SM_Current_State);
}

bool FEB_SM_Is_HV_Active(void)
{
  BMS_State_t state = SM_Current_State;
  return (state == BMS_STATE_ENERGIZED || state == BMS_STATE_DRIVE || state == BMS_STATE_CHARGING ||
          state == BMS_STATE_BALANCE);
}

bool FEB_SM_Is_Drive_Ready(void)
{
  BMS_State_t state = SM_Current_State;
  return (state == BMS_STATE_ENERGIZED || state == BMS_STATE_DRIVE);
}

/* ============================================================================
 * Transition Functions
 * ============================================================================ */

static void bootTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_BSPD:
    fault_begin(next_state);
    break;

  case BMS_STATE_LV_POWER:
    updateStateProtected(BMS_STATE_LV_POWER);
    break;

  case BMS_STATE_DEFAULT:
    /* Auto-transition to LV when relays are in correct state */
    if (FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_OPEN && FEB_HW_Precharge_Sense() == FEB_RELAY_STATE_OPEN &&
        FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_CLOSE)
    {
      bootTransition(BMS_STATE_LV_POWER);
    }
    break;

  default:
    break;
  }
}

static void LVPowerTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_BSPD:
    fault_begin(next_state);
    break;

  case BMS_STATE_BUS_HEALTH_CHECK:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_BATTERY_FREE:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_DEFAULT:
    /* Check if shutdown loop is complete before going to health check */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_CLOSE)
    {
      LVPowerTransition(BMS_STATE_BUS_HEALTH_CHECK);
    }
    break;

  default:
    break;
  }
}

static void HealthCheckTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_BSPD:
    fault_begin(next_state);
    break;

  case BMS_STATE_LV_POWER:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_BATTERY_FREE:
    LOG_I(TAG_SM, "Entering BATTERY_FREE from BUS_HEALTH_CHECK");
    updateStateProtected(next_state);
    break;

  case BMS_STATE_PRECHARGE:
    /* Should be open but included for redundancy */
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(true);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_DEFAULT:
    /* Go back to LV if shutdown not completed */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_OPEN)
    {
      LOG_W(TAG_SM, "Shutdown open during health check, returning to LV_POWER");
      HealthCheckTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* Check conditions for precharge:
     * - AIR- closed, AIR+ open, precharge open */
    if (FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_CLOSE && FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_OPEN &&
        FEB_HW_Precharge_Sense() == FEB_RELAY_STATE_OPEN)
    {
      HealthCheckTransition(BMS_STATE_PRECHARGE);
    }
    break;

  default:
    break;
  }
}

static void PrechargeTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_BSPD:
    fault_begin(next_state);
    break;

  case BMS_STATE_LV_POWER:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_BUS_HEALTH_CHECK:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_ENERGIZED:
    /* Close AIR+ and start non-blocking delay */
    FEB_HW_AIR_Plus_Set(true);
    energize_pending = true;
    energize_delay_start = HAL_GetTick();
    /* Note: FEB_SM_Process() will complete the transition after delay */
    break;

  case BMS_STATE_DEFAULT:
    /* Safety check with debounce: require multiple consecutive OPEN readings to filter transients */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_OPEN || FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      shutdown_open_count++;
      if (shutdown_open_count >= SHUTDOWN_DEBOUNCE_COUNT)
      {
        LOG_W(TAG_SM, "Shutdown/AIR- open during precharge, returning to LV_POWER");
        shutdown_open_count = 0;
        PrechargeTransition(BMS_STATE_LV_POWER);
        break;
      }
    }
    else
    {
      shutdown_open_count = 0; /* Reset counter on good reading */
    }

    /* Start precharge timer on first entry */
    if (precharge_start_time == 0)
    {
      precharge_start_time = HAL_GetTick();
      LOG_I(TAG_SM, "Precharge started");
    }

    /* Check precharge timeout */
    if ((HAL_GetTick() - precharge_start_time) >= PRECHARGE_TIMEOUT_MS)
    {
      /* Precharge failed - enter fault state */
      LOG_E(TAG_SM, "Precharge timeout (%dms), entering fault", PRECHARGE_TIMEOUT_MS);
      fault_begin(BMS_STATE_FAULT_BMS);
      precharge_start_time = 0;
      break;
    }

    /* Keep AIR+ open for redundancy */
    FEB_HW_AIR_Plus_Set(false);

    /* Hold precharge relay closed for redundancy */
    FEB_HW_Precharge_Set(true);

    /* Check precharge completion: IVT voltage >= 90% of pack voltage */
    // {
    //   float ivt_voltage = FEB_CAN_IVT_GetVoltage();
    //   float pack_voltage = FEB_ADBMS_GET_ACC_Total_Voltage();
    //   if (pack_voltage > 0.0f && ivt_voltage >= PRECHARGE_THRESHOLD_PCT * pack_voltage)
    //   {
    //     LOG_I(TAG_SM, "Precharge complete: IVT=%.1fV Pack=%.1fV", ivt_voltage, pack_voltage);
    //     precharge_start_time = 0;
    //     shutdown_open_count = 0;  /* Reset debounce counter */
    PrechargeTransition(BMS_STATE_ENERGIZED);
    //   }
    // }
    break;

  default:
    break;
  }
}

static void EnergizedTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_BSPD:
    fault_begin(next_state);
    break;

  case BMS_STATE_DRIVE:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_LV_POWER:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_BUS_HEALTH_CHECK:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_DEFAULT:
    /* Safety check: go back to LV if shutdown or AIR- opens */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_OPEN || FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      LOG_W(TAG_SM, "Shutdown/AIR- open while energized, returning to LV_POWER");
      EnergizedTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* Check for ready-to-drive signal from DASH */
    // if (FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    // {
    LOG_I(TAG_SM, "R2D signal received, entering DRIVE");
    EnergizedTransition(BMS_STATE_DRIVE);
    // }
    break;

  default:
    break;
  }
}

static void DriveTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_BSPD:
    fault_begin(next_state);
    break;

  case BMS_STATE_LV_POWER:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_BUS_HEALTH_CHECK:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_ENERGIZED:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_DEFAULT:
    /* Safety check: go back to LV if shutdown or AIR- opens */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_OPEN || FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      LOG_W(TAG_SM, "Shutdown/AIR- open while driving, returning to LV_POWER");
      DriveTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* If driver no longer requests R2D, go back to energized */
    // if (!FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    // {
    //   LOG_I(TAG_SM, "R2D signal lost, returning to ENERGIZED");
    // DriveTransition(BMS_STATE_ENERGIZED);
    // }
    break;

  default:
    break;
  }
}

static void FreeTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_CHARGING:
    fault_begin(next_state);
    break;

  case BMS_STATE_BATTERY_FREE:
  case BMS_STATE_LV_POWER:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_CHARGER_PRECHARGE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(true);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_BALANCE:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_DEFAULT:
    /* In battery free state, wait for charger connection or return to LV */
    /* Note: Charger detection logic would be added here when charger CAN is implemented */
    break;

  default:
    break;
  }
}

static void ChargingPrechargeTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_CHARGING:
    fault_begin(next_state);
    break;

  case BMS_STATE_BATTERY_FREE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    updateStateProtected(next_state);
    break;

  case BMS_STATE_CHARGING:
    /* Close AIR+ and start non-blocking delay */
    FEB_HW_AIR_Plus_Set(true);
    charging_pending = true;
    charging_delay_start = HAL_GetTick();
    /* Note: FEB_SM_Process() will complete the transition after delay */
    break;

  case BMS_STATE_BALANCE:
    updateStateProtected(next_state);
    break;

  case BMS_STATE_DEFAULT:
    /* Go back to FREE if shutdown opens */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_OPEN)
    {
      ChargingPrechargeTransition(BMS_STATE_BATTERY_FREE);
      charger_precharge_start_time = 0;
      break;
    }

    /* Start precharge timer on first entry */
    if (charger_precharge_start_time == 0)
    {
      charger_precharge_start_time = HAL_GetTick();
    }

    /* Check precharge timeout */
    if ((HAL_GetTick() - charger_precharge_start_time) >= PRECHARGE_TIMEOUT_MS)
    {
      /* Precharge failed - enter fault state */
      fault_begin(BMS_STATE_FAULT_CHARGING);
      charger_precharge_start_time = 0;
      break;
    }

    /* Keep AIR+ open for redundancy */
    FEB_HW_AIR_Plus_Set(false);

    /* Hold precharge relay closed */
    FEB_HW_Precharge_Set(true);

    /* Check precharge completion */
    float ivt_voltage = FEB_CAN_IVT_GetVoltage();
    float pack_voltage = FEB_ADBMS_GET_ACC_Total_Voltage();
    if (pack_voltage > 0.0f && ivt_voltage >= PRECHARGE_THRESHOLD_PCT * pack_voltage)
    {
      charger_precharge_start_time = 0;
      ChargingPrechargeTransition(BMS_STATE_CHARGING);
    }
    break;

  default:
    break;
  }
}

static void ChargingTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_CHARGING:
    /* Note: Would stop charger here when charger CAN is implemented */
    fault_begin(next_state);
    break;

  case BMS_STATE_LV_POWER:
  case BMS_STATE_BATTERY_FREE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    /* Note: Would stop charger here when charger CAN is implemented */
    updateStateProtected(BMS_STATE_BATTERY_FREE);
    break;

  case BMS_STATE_DEFAULT:
    /* Safety check: go back to FREE if AIR- opens */
    if (FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      ChargingTransition(BMS_STATE_BATTERY_FREE);
    }
    /* Note: Would check charging status here when charger CAN is implemented */
    break;

  default:
    break;
  }
}

static void BalanceTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_FAULT_BMS:
  case BMS_STATE_FAULT_IMD:
  case BMS_STATE_FAULT_CHARGING:
    FEB_Stop_Balance();
    fault_begin(next_state);
    break;

  case BMS_STATE_LV_POWER:
  case BMS_STATE_BATTERY_FREE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    FEB_Stop_Balance();
    updateStateProtected(BMS_STATE_BATTERY_FREE);
    break;

  case BMS_STATE_DEFAULT:
    /* Safety check: go back to FREE if AIR- opens */
    if (FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      BalanceTransition(BMS_STATE_BATTERY_FREE);
    }
    break;

  default:
    break;
  }
}

/* ============================================================================
 * Fault Transition Functions
 * ============================================================================ */

static void BMSFaultTransition(BMS_State_t next_state)
{
  switch (next_state)
  {
  case BMS_STATE_DEFAULT:
    /* Perpetually fault until reset */
    FEB_HW_BMS_Indicator_Set(true);
    fault_begin(BMS_STATE_FAULT_BMS);
    break;

  default:
    break;
  }
}

static void BSPDFaultTransition(BMS_State_t next_state)
{
  if (next_state == BMS_STATE_DEFAULT)
  {
    return;
  }
  fault_begin(SM_Current_State);
}

static void IMDFaultTransition(BMS_State_t next_state)
{
  if (next_state == BMS_STATE_DEFAULT)
  {
    return;
  }
  fault_begin(SM_Current_State);
}

static void ChargingFaultTransition(BMS_State_t next_state)
{
  if (next_state == BMS_STATE_DEFAULT)
  {
    return;
  }
  FEB_HW_BMS_Indicator_Set(true);
  fault_begin(SM_Current_State);
}
