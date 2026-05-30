/**
 ******************************************************************************
 * @file           : FEB_Main.h
 * @brief          : DCU Application - Console and Communication Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_MAIN_H
#define FEB_MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initialize FEB libraries - called from MX_FREERTOS_Init() after FreeRTOS objects created
   */
  void FEB_Init(void);

  /**
   * @brief UART RX task for USART2 (primary console) - overrides weak stub in freertos.c
   * @param argument Not used
   */
  void StartUartRxTask(void *argument);

  /**
   * @brief UART RX task for UART4 (secondary console, identical command set)
   * @param argument Not used
   */
  void StartUart4RxTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */
