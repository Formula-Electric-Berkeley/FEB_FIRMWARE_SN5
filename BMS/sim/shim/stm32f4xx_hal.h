/**
 * @file stm32f4xx_hal.h (sim shim)
 * @brief Host stand-in for the STM32 HAL: only what the simulated User
 *        sources touch. HAL_GetTick() reads the sim clock.
 */
#ifndef SIM_STM32F4XX_HAL_H
#define SIM_STM32F4XX_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t HAL_GetTick(void);

/* No interrupts on the host: critical sections are no-ops. */
static inline uint32_t __get_PRIMASK(void)
{
  return 0;
}
static inline void __disable_irq(void)
{
}
static inline void __set_PRIMASK(uint32_t primask)
{
  (void)primask;
}

#ifdef __cplusplus
}
#endif

#endif /* SIM_STM32F4XX_HAL_H */
