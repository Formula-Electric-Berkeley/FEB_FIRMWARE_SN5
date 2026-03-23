/**
 ******************************************************************************
 * @file           : feb_rtos_utils.h
 * @brief          : FEB RTOS Utilities Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Common RTOS utility macros shared across FEB boards.
 *
 ******************************************************************************
 */

#ifndef FEB_RTOS_UTILS_H
#define FEB_RTOS_UTILS_H

/**
 * @brief Fail-fast macro for RTOS handle validation
 * @param handle The RTOS handle to check (mutex, semaphore, queue, task)
 *
 * If handle is NULL, calls Error_Handler() to halt system.
 * Use during initialization to catch allocation failures early.
 *
 * Example:
 *   myMutexHandle = osMutexNew(&myMutex_attributes);
 *   REQUIRE_RTOS_HANDLE(myMutexHandle);
 */
#define REQUIRE_RTOS_HANDLE(handle) \
  do { if ((handle) == NULL) { Error_Handler(); } } while(0)

#endif /* FEB_RTOS_UTILS_H */
