/**
 ******************************************************************************
 * @file           : feb_time.c
 * @brief          : FEB Time Library - DWT-backed 64-bit microsecond clock
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_time.h"

/* CMSIS core header brings in CoreDebug, DWT, and __disable_irq/__set_PRIMASK.
 * Going through main.h keeps us consistent with every other file in the tree. */
#include "main.h"

static uint32_t cyc_per_us = 1;

#if (__CORTEX_M >= 3U)

static volatile uint64_t cycle_hi = 0;
static volatile uint32_t last_cyc = 0;

void FEB_Time_Init(void)
{
  /* Enable the trace unit so DWT can run. */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  /* Reset and enable the cycle counter. */
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  uint32_t hz = SystemCoreClock;
  cyc_per_us = (hz >= 1000000U) ? (hz / 1000000U) : 1U;

  cycle_hi = 0;
  last_cyc = 0;
}

static inline uint64_t sample_cycles_locked(void)
{
  /* Caller holds interrupts disabled. Read CYCCNT, detect wrap against the
   * last cycle value we've seen, update the 64-bit accumulator. */
  uint32_t cyc = DWT->CYCCNT;
  if (cyc < last_cyc)
  {
    cycle_hi += (uint64_t)1 << 32;
  }
  last_cyc = cyc;
  return cycle_hi + (uint64_t)cyc;
}

uint64_t FEB_Time_Us(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint64_t total_cycles = sample_cycles_locked();
  __set_PRIMASK(primask);
  return total_cycles / cyc_per_us;
}

uint32_t FEB_Time_Us32(void)
{
  return (uint32_t)FEB_Time_Us();
}

void FEB_Time_OnSysTick(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  (void)sample_cycles_locked();
  __set_PRIMASK(primask);
}

#else /* __CORTEX_M < 3U — no DWT. Fall back to HAL_GetTick + SysTick->VAL. */

static uint32_t cyc_per_tick = 1;

void FEB_Time_Init(void)
{
  uint32_t hz = SystemCoreClock;
  cyc_per_us = (hz >= 1000000U) ? (hz / 1000000U) : 1U;
  cyc_per_tick = SysTick->LOAD + 1U;
}

static inline uint64_t sample_cycles_locked(void)
{
  /* Caller holds interrupts disabled, so HAL's uwTick cannot advance, but
   * the SysTick hardware counter keeps running. If it wraps between our
   * reads of HAL_GetTick() and SysTick->VAL, COUNTFLAG latches high; we
   * detect that and bump ms by one to compensate. Reading CTRL clears the
   * flag, so grab VAL again afterwards for a consistent pair. */
  uint32_t ms  = HAL_GetTick();
  uint32_t val = SysTick->VAL;
  if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
  {
    ms += 1U;
    val = SysTick->VAL;
  }
  uint32_t cyc_in_tick = (cyc_per_tick - 1U) - val;
  return (uint64_t)ms * (uint64_t)cyc_per_tick + (uint64_t)cyc_in_tick;
}

uint64_t FEB_Time_Us(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint64_t total_cycles = sample_cycles_locked();
  __set_PRIMASK(primask);
  return total_cycles / cyc_per_us;
}

uint32_t FEB_Time_Us32(void)
{
  return (uint32_t)FEB_Time_Us();
}

void FEB_Time_OnSysTick(void)
{
  /* HAL_GetTick() is already the 32-bit accumulator; there's no wrap-prone
   * 32-bit cycle counter to catch up on. */
}

#endif /* __CORTEX_M */
