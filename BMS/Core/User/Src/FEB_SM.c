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
#include "FEB_CAN_Charger.h"
#include "FEB_CAN_Heartbeat.h"
#include "FEB_Const.h"
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <math.h>
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

/* Heartbeat freshness window for CAN-presence (BATTERY_FREE <-> LV_POWER) */
#define HB_PRESENCE_TIMEOUT_MS 1000

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

/* Debounce timers for evaluate_faults() (0 = condition currently clear) */
static uint32_t overcurrent_start_tick = 0;
static uint32_t imd_open_start_tick = 0;
static uint32_t contactor_mismatch_start_tick = 0;

/* Special DEFAULT value for transition function calls during FEB_SM_Process */
#define BMS_STATE_DEFAULT BMS_STATE_COUNT

/* ============================================================================
 * Forward Declarations - Helper Functions
 * ============================================================================ */

static bool isFaultState(BMS_State_t state);
static void fault_begin(BMS_State_t fault_type);
static bool fault_process(void);
static void check_reset_button(void);
static void evaluate_faults(void);

/* TODO(spec 5->10 BSPD): no BSPD GPIO/CAN input on SN5 yet. Drive-only fault.
 * Safe default: never trips until a real BSPD source is wired in. */
static inline bool BSPD_brake_fault(void)
{
  return false;
}

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
  /* Faults latch until power cycle: block exit from ANY fault state.
   * fault_begin() sets the state directly, so fault entry is unaffected. */
  if (isFaultState(SM_Current_State))
  {
    return SM_Current_State;
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

  /* Stop cell balancing immediately */
  FEB_Stop_Balance();

  /* Open BMS shutdown relay immediately (disables HV path) */
  FEB_HW_BMS_Shutdown_Set(false);
  LOG_W(TAG_SM, "BMS shutdown relay opened");

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

/**
 * @brief Centralized safety-condition evaluation (runs every SM tick).
 *
 * Faults route per the spec diagram by state group:
 *  - drive group (BOOT..DRIVE)             -> FAULT_BMS / FAULT_IMD
 *  - charger group (BATTERY_FREE..BALANCE) -> FAULT_CHARGING
 *
 * Thread-safety: reads only lock-free sources. Cell V/T violations and
 * cell-monitor staleness cross from the ADBMS task via latched volatile flags
 * (FEB_ADBMS_Get_Fault_Flags / _Get_Last_Update_Tick). IVT current, IMD, and
 * contactor sense are single-register reads. No mutex is taken on this path.
 */
static void evaluate_faults(void)
{
  BMS_State_t s = SM_Current_State;

  /* Already latched or mid-entry: nothing to do. */
  if (isFaultState(s) || fault_pending)
  {
    return;
  }

  bool charger_group = (s == BMS_STATE_BATTERY_FREE || s == BMS_STATE_CHARGER_PRECHARGE || s == BMS_STATE_CHARGING ||
                        s == BMS_STATE_BALANCE);
  BMS_State_t grp_fault = charger_group ? BMS_STATE_FAULT_CHARGING : BMS_STATE_FAULT_BMS;

  /* (a) Cell voltage / temperature violations latched by the ADBMS task. */
  uint32_t af = FEB_ADBMS_Get_Fault_Flags();
  if (af & (ADBMS_FAULT_FLAG_VOLTAGE | ADBMS_FAULT_FLAG_TEMP))
  {
    LOG_E(TAG_SM, "Cell V/T violation (flags=0x%02lX)", (unsigned long)af);
    fault_begin(grp_fault);
    return;
  }

  /* (b) Cell-monitor sensor timeout (guard against the pre-first-scan zero). */
#if !FEB_BMS_DISABLE_ADBMS_CHECKS
  uint32_t last = FEB_ADBMS_Get_Last_Update_Tick();
  if (last != 0 && (HAL_GetTick() - last) > FEB_ADBMS_DATA_TIMEOUT_MS)
  {
    LOG_E(TAG_SM, "Cell-monitor data timeout");
    fault_begin(grp_fault);
    return;
  }
#endif

  /* (c) IVT overcurrent + sensor timeout, only while current can actually flow
   * (HV contactors closed). BATTERY_FREE and BALANCE are excluded: the pack is
   * isolated there and the IVT may legitimately be silent, so checking it would
   * spuriously fault. */
  bool hv_current_path =
      (s >= BMS_STATE_PRECHARGE && s <= BMS_STATE_DRIVE) || s == BMS_STATE_CHARGER_PRECHARGE || s == BMS_STATE_CHARGING;
  if (hv_current_path)
  {
    if (!FEB_CAN_IVT_IsDataFresh(FEB_IVT_FAULT_TIMEOUT_MS))
    {
      LOG_E(TAG_SM, "IVT current-sensor timeout");
      fault_begin(grp_fault);
      return;
    }

    bool charging_path = (s == BMS_STATE_CHARGER_PRECHARGE || s == BMS_STATE_CHARGING);
    float ilim = charging_path ? FEB_CHARGE_OVERCURRENT_A : FEB_DISCHARGE_OVERCURRENT_A;
    if (fabsf(FEB_CAN_IVT_GetCurrent()) > ilim)
    {
      if (overcurrent_start_tick == 0)
      {
        overcurrent_start_tick = HAL_GetTick();
      }
      else if ((HAL_GetTick() - overcurrent_start_tick) >= FEB_OVERCURRENT_CONFIRM_MS)
      {
        LOG_E(TAG_SM, "Overcurrent event (|I| > %.0fA)", (double)ilim);
        fault_begin(grp_fault);
        return;
      }
    }
    else
    {
      overcurrent_start_tick = 0;
    }
  }
  else
  {
    overcurrent_start_tick = 0;
  }

  /* (d) Continuous IMD monitoring (debounced; suppressed during BOOT). */
  if (s != BMS_STATE_BOOT && FEB_HW_IMD_Sense() == FEB_RELAY_STATE_OPEN)
  {
    if (imd_open_start_tick == 0)
    {
      imd_open_start_tick = HAL_GetTick();
    }
    else if ((HAL_GetTick() - imd_open_start_tick) >= FEB_IMD_FAULT_CONFIRM_MS)
    {
      LOG_E(TAG_SM, "IMD triggered");
      fault_begin(charger_group ? BMS_STATE_FAULT_CHARGING : BMS_STATE_FAULT_IMD);
      return;
    }
  }
  else
  {
    imd_open_start_tick = 0;
  }

  /* (e) RECOMMENDED: contactor feedback plausibility (weld/stuck detection).
   * Compare commanded vs sensed AIR+/precharge in steady HV states. Skipped
   * while the non-blocking energize/charging settle is in flight (the sense
   * legitimately disagrees with the state label during that window). */
  {
    bool expect_air_plus = (s == BMS_STATE_ENERGIZED || s == BMS_STATE_DRIVE || s == BMS_STATE_CHARGING);
    bool expect_precharge = (s == BMS_STATE_PRECHARGE || s == BMS_STATE_CHARGER_PRECHARGE);
    bool check_contactors = (s == BMS_STATE_ENERGIZED || s == BMS_STATE_DRIVE || s == BMS_STATE_CHARGING ||
                             s == BMS_STATE_PRECHARGE || s == BMS_STATE_CHARGER_PRECHARGE) &&
                            !energize_pending && !charging_pending;

    if (check_contactors)
    {
      bool air_plus_closed = (FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_CLOSE);
      bool precharge_closed = (FEB_HW_Precharge_Sense() == FEB_RELAY_STATE_CLOSE);
      if (air_plus_closed != expect_air_plus || precharge_closed != expect_precharge)
      {
        if (contactor_mismatch_start_tick == 0)
        {
          contactor_mismatch_start_tick = HAL_GetTick();
        }
        else if ((HAL_GetTick() - contactor_mismatch_start_tick) >= FEB_CONTACTOR_FEEDBACK_TIMEOUT_MS)
        {
          LOG_E(TAG_SM, "Contactor feedback mismatch (AIR+ %d/exp %d, PrC %d/exp %d)", air_plus_closed, expect_air_plus,
                precharge_closed, expect_precharge);
          fault_begin(grp_fault);
          return;
        }
      }
      else
      {
        contactor_mismatch_start_tick = 0;
      }
    }
    else
    {
      contactor_mismatch_start_tick = 0;
    }
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

  /* Evaluate safety conditions before anything else so a fault is caught even
   * during contactor-settle windows and before per-handler soft returns. */
  evaluate_faults();

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
      /* Open precharge after AIR+ has settled, then command the charger on */
      FEB_HW_Precharge_Set(false);
      FEB_CAN_Charger_Start_Charge();
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
    /* 1->2: shutdown loop ("ESC/TSMS") closed -> bus health check. */
    if (FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_CLOSE)
    {
      LVPowerTransition(BMS_STATE_BUS_HEALTH_CHECK);
      break;
    }

    /* 1->6 (Disconnection): only the charger is on CAN (no DASH/PCU heartbeat)
     * -> BATTERY_FREE. Mirrors SN4 disconnection semantics. */
    if (!FEB_CAN_Heartbeat_OthersPresent(HB_PRESENCE_TIMEOUT_MS) && FEB_CAN_Charger_Received())
    {
      LOG_I(TAG_SM, "Only charger on CAN, entering BATTERY_FREE");
      LVPowerTransition(BMS_STATE_BATTERY_FREE);
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

    /* Precharge completion gate (3->4): IVT voltage >= 90% of pack voltage.
     * Snapshot read: never take the ADBMS mutex on this 1ms path. */
    {
      float ivt_voltage = FEB_CAN_IVT_GetVoltage();
      float pack_voltage = FEB_ADBMS_Snapshot_Total_Voltage();
      if (pack_voltage > 0.0f && ivt_voltage >= PRECHARGE_THRESHOLD_PCT * pack_voltage)
      {
        LOG_I(TAG_SM, "Precharge complete: IVT=%.1fV Pack=%.1fV", (double)ivt_voltage, (double)pack_voltage);
        precharge_start_time = 0;
        shutdown_open_count = 0; /* Reset debounce counter */
        PrechargeTransition(BMS_STATE_ENERGIZED);
      }
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
      LOG_W(TAG_SM, "Shutdown/AIR- open while energized, returning to LV_POWER");
      EnergizedTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* Ready-to-drive gate (4->5): enter DRIVE only on a fresh R2D from DASH. */
    if (FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      LOG_I(TAG_SM, "R2D signal received, entering DRIVE");
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
      LOG_W(TAG_SM, "Shutdown/AIR- open while driving, returning to LV_POWER");
      DriveTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* 5->10 (Brake Fault): BSPD reports a fault. No BSPD input on SN5 yet, so
     * this stub never trips (see BSPD_brake_fault above). Drive-only per spec. */
    if (BSPD_brake_fault())
    {
      LOG_E(TAG_SM, "BSPD fault while driving");
      DriveTransition(BMS_STATE_FAULT_BSPD);
      break;
    }

    /* 5->4: spec trigger is APPS-deactivated / Park, which the BMS cannot see
     * today. R2D-loss is the available proxy: drop back to ENERGIZED. */
    if (!FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      LOG_I(TAG_SM, "R2D signal lost, returning to ENERGIZED");
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
    /* Charger group: any fault lands in FAULT_CHARGING (diagram 6,7,8->12;
     * matches SN4's coercion). evaluate_faults() routes the same way. */
    fault_begin(BMS_STATE_FAULT_CHARGING);
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
  {
    /* 6->1 (Reconnection): other subsystems (DASH/PCU) back on CAN -> LV_POWER. */
    if (FEB_CAN_Heartbeat_OthersPresent(HB_PRESENCE_TIMEOUT_MS))
    {
      LOG_I(TAG_SM, "Other subsystems on CAN, returning to LV_POWER");
      FreeTransition(BMS_STATE_LV_POWER);
      break;
    }

    /* Charge decision (mirrors SN4 FreeTransition). */
    int8_t charging_status = FEB_CAN_Charging_Status();
    if (charging_status == -1)
    {
      LOG_E(TAG_SM, "Charging fault detected in BATTERY_FREE");
      FreeTransition(BMS_STATE_FAULT_CHARGING);
      break;
    }

    /* 6->7 (begin charge): charger present on CAN, AIR- closed, no charge fault. */
    if (FEB_CAN_Charger_Received() && charging_status == 0 && FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_CLOSE)
    {
      LOG_I(TAG_SM, "Charger detected, entering charger precharge");
      FreeTransition(BMS_STATE_CHARGER_PRECHARGE);
    }
    break;
  }

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
    FEB_CAN_Charger_Stop_Charge();
    /* Charger group: coerce to FAULT_CHARGING (diagram 6,7,8->12; SN4 parity) */
    fault_begin(BMS_STATE_FAULT_CHARGING);
    break;

  case BMS_STATE_BATTERY_FREE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    FEB_CAN_Charger_Stop_Charge();
    updateStateProtected(next_state);
    break;

  case BMS_STATE_CHARGING:
    /* Close AIR+ and start non-blocking delay (charger commanded on after
     * settle, in FEB_SM_Process). */
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

    /* Check precharge completion (snapshot read: no ADBMS mutex on 1ms path) */
    float ivt_voltage = FEB_CAN_IVT_GetVoltage();
    float pack_voltage = FEB_ADBMS_Snapshot_Total_Voltage();
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
    FEB_CAN_Charger_Stop_Charge();
    /* Charger group: coerce to FAULT_CHARGING (diagram 6,7,8->12; SN4 parity) */
    fault_begin(BMS_STATE_FAULT_CHARGING);
    break;

  case BMS_STATE_LV_POWER:
  case BMS_STATE_BATTERY_FREE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    FEB_CAN_Charger_Stop_Charge();
    updateStateProtected(BMS_STATE_BATTERY_FREE);
    break;

  case BMS_STATE_DEFAULT:
  {
    /* Safety: AIR- open -> back to FREE (mirrors SN4). */
    if (FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      ChargingTransition(BMS_STATE_BATTERY_FREE);
      break;
    }

    /* SN4 charge decision: 1 = done / soft V or T limit -> FREE; -1 = hard
     * over-V/T -> FAULT_CHARGING. (Other charger-group faults are caught by
     * evaluate_faults().) */
    int8_t charge_status = FEB_CAN_Charging_Status();
    if (charge_status == 1)
    {
      LOG_I(TAG_SM, "Charge complete, returning to BATTERY_FREE");
      ChargingTransition(BMS_STATE_BATTERY_FREE);
    }
    else if (charge_status == -1)
    {
      LOG_E(TAG_SM, "Charging hard fault");
      ChargingTransition(BMS_STATE_FAULT_CHARGING);
    }
    break;
  }

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
    /* Charger group: coerce to FAULT_CHARGING (diagram 6,7,8->12; SN4 parity) */
    fault_begin(BMS_STATE_FAULT_CHARGING);
    break;

  case BMS_STATE_LV_POWER:
  case BMS_STATE_BATTERY_FREE:
    FEB_HW_AIR_Plus_Set(false);
    FEB_HW_Precharge_Set(false);
    FEB_Stop_Balance();
    updateStateProtected(BMS_STATE_BATTERY_FREE);
    break;

  case BMS_STATE_DEFAULT:
  {
    /* Safety check: go back to FREE if AIR- opens */
    if (FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_OPEN)
    {
      BalanceTransition(BMS_STATE_BATTERY_FREE);
      break;
    }

    /* 8->6 (balance complete): cell delta within slippage threshold. Checked at
     * ~1 Hz to avoid hammering the mutex-taking status call every 1 ms tick. */
    static uint32_t balance_check_tick = 0;
    if ((HAL_GetTick() - balance_check_tick) >= 1000)
    {
      balance_check_tick = HAL_GetTick();
      if (!FEB_Cell_Balancing_Status())
      {
        LOG_I(TAG_SM, "Cells balanced, returning to BATTERY_FREE");
        BalanceTransition(BMS_STATE_BATTERY_FREE);
      }
    }
    break;
  }

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
