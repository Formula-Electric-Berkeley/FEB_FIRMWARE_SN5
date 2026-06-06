/**
 ******************************************************************************
 * @file           : feb_time.h
 * @brief          : FEB Time Library - microsecond-precision monotonic clock
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a 64-bit microsecond clock. On Cortex-M3+ (F446RE boards) it is
 * backed by the DWT cycle counter - zero peripheral cost, uses only the
 * CoreDebug/DWT core units. On Cortex-M0 (F042 / DART), DWT is not present,
 * so the library falls back to HAL_GetTick() plus SysTick->VAL for sub-tick
 * resolution. The API and units are identical on both paths.
 *
 * Usage:
 *   FEB_Time_Init();                 // once at boot, before use
 *   uint64_t t0 = FEB_Time_Us();
 *   ...
 *   uint64_t dt = FEB_Time_Us() - t0;
 *
 * Wrap handling:
 *   DWT->CYCCNT is 32 bits and wraps every (2^32)/SystemCoreClock seconds
 *   (~23.86 s at 180 MHz). FEB_Time_Us() self-maintains a 64-bit counter
 *   by detecting wraps on each read. As long as FEB_Time_Us() is called
 *   at least once within each wrap window, counting is exact.
 *
 *   In practice, any CSV console traffic polls the counter many times
 *   per second, so this is never an issue. For extra safety on idle
 *   systems, wire FEB_Time_OnSysTick() into a periodic hook (SysTick,
 *   FreeRTOS tick, or any task running at >= 0.05 Hz).
 *
 ******************************************************************************
 */

#ifndef FEB_TIME_H
#define FEB_TIME_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

  /**
   * @brief Initialize the FEB Time subsystem
   *
   * Enables the DWT cycle counter and captures SystemCoreClock. Must be called
   * once at boot, after SystemInit() has set SystemCoreClock to its final value.
   *
   * Safe to call multiple times (idempotent).
   */
  void FEB_Time_Init(void);

  /**
   * @brief Get monotonic microseconds since FEB_Time_Init()
   *
   * @return 64-bit microsecond count
   *
   * Thread/ISR safe: briefly disables interrupts while sampling the cycle
   * counter and wrap accumulator (~a dozen instructions).
   */
  uint64_t FEB_Time_Us(void);

  /**
   * @brief Get monotonic microseconds as 32-bit (wraps every ~71 minutes)
   *
   * Cheaper than FEB_Time_Us() for call sites that don't need the full range.
   */
  uint32_t FEB_Time_Us32(void);

  /**
   * @brief Optional periodic wrap-catcher
   *
   * If the application may be idle for longer than one DWT wrap period
   * (~23.86 s at 180 MHz) without ever calling FEB_Time_Us(), wire this
   * into any hook that runs at least once per ~20 s. Safe to call from ISR.
   *
   * Not required when CSV console traffic is active (each row calls Us()).
   */
  void FEB_Time_OnSysTick(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TIME_H */
