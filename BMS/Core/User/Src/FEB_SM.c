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
#include "FEB_ADBMS6830B.h"
#include "FEB_CAN_IVT.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

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

/* Special DEFAULT value for transition function calls during FEB_SM_Process */
#define BMS_STATE_DEFAULT BMS_STATE_COUNT

/* ============================================================================
 * Forward Declarations - Helper Functions
 * ============================================================================ */

static bool isFaultState(BMS_State_t state);
static void fault_begin(BMS_State_t fault_type);
static bool fault_process(void);

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
  SM_Current_State = next_state;
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

  SM_Current_State = fault_type;
  FEB_CAN_State_SetState(fault_type);

  /* Open BMS shutdown relay immediately (disables HV path) */
  FEB_HW_BMS_Shutdown_Set(false);

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

/* ============================================================================
 * Public Interface
 * ============================================================================ */

void FEB_SM_Init(void)
{
  SM_Current_State = BMS_STATE_BOOT;

  /* Open AIR+ and precharge relays (safe state) */
  FEB_HW_AIR_Plus_Set(false);
  FEB_HW_Precharge_Set(false);

  /* Reset indicators */
  FEB_HW_BMS_Indicator_Set(false);
  FEB_HW_Fault_Indicator_Set(false);

  /* Close BMS shutdown relay (enables HV path when shutdown loop complete) */
  FEB_HW_BMS_Shutdown_Set(true);

  /* Update CAN state */
  FEB_CAN_State_SetState(BMS_STATE_BOOT);

  /* Transition to LV_POWER */
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
    /* Safety check: go back to LV if shutdown or AIR- opens */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_OPEN || FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      PrechargeTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* Start precharge timer on first entry - protected for ISR/task access */
    {
      SM_ENTER_CRITICAL();
      if (precharge_start_time == 0)
      {
        precharge_start_time = HAL_GetTick();
      }
      SM_EXIT_CRITICAL();
    }

    /* Check precharge timeout - protected for ISR/task access */
    {
      SM_ENTER_CRITICAL();
      uint32_t start_time = precharge_start_time;
      SM_EXIT_CRITICAL();

      if ((HAL_GetTick() - start_time) >= PRECHARGE_TIMEOUT_MS)
      {
        /* Precharge failed - enter fault state */
        fault_begin(BMS_STATE_FAULT_BMS);
        SM_ENTER_CRITICAL();
        precharge_start_time = 0;
        SM_EXIT_CRITICAL();
        break;
      }
    }

    /* Keep AIR+ open for redundancy */
    FEB_HW_AIR_Plus_Set(false);

    /* Hold precharge relay closed for redundancy */
    FEB_HW_Precharge_Set(true);

    /* Check precharge completion: IVT voltage >= 90% of pack voltage */
    float ivt_voltage = FEB_CAN_IVT_GetVoltage();
    float pack_voltage = FEB_ADBMS_GET_ACC_Total_Voltage();
    if (pack_voltage > 0.0f && ivt_voltage >= PRECHARGE_THRESHOLD_PCT * pack_voltage)
    {
      SM_ENTER_CRITICAL();
      precharge_start_time = 0; /* Reset timer */
      SM_EXIT_CRITICAL();
      PrechargeTransition(BMS_STATE_ENERGIZED);
    }
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
      EnergizedTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* Check for ready-to-drive signal from DASH */
    if (FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      EnergizedTransition(BMS_STATE_DRIVE);
    }
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
      DriveTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* If driver no longer requests R2D, go back to energized */
    if (!FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      DriveTransition(BMS_STATE_ENERGIZED);
    }
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
      SM_ENTER_CRITICAL();
      charger_precharge_start_time = 0;
      SM_EXIT_CRITICAL();
      break;
    }

    /* Start precharge timer on first entry - protected for ISR/task access */
    {
      SM_ENTER_CRITICAL();
      if (charger_precharge_start_time == 0)
      {
        charger_precharge_start_time = HAL_GetTick();
      }
      SM_EXIT_CRITICAL();
    }

    /* Check precharge timeout - protected for ISR/task access */
    {
      SM_ENTER_CRITICAL();
      uint32_t start_time = charger_precharge_start_time;
      SM_EXIT_CRITICAL();

      if ((HAL_GetTick() - start_time) >= PRECHARGE_TIMEOUT_MS)
      {
        /* Precharge failed - enter fault state */
        fault_begin(BMS_STATE_FAULT_CHARGING);
        SM_ENTER_CRITICAL();
        charger_precharge_start_time = 0;
        SM_EXIT_CRITICAL();
        break;
      }
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
      SM_ENTER_CRITICAL();
      charger_precharge_start_time = 0; /* Reset timer */
      SM_EXIT_CRITICAL();
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
