/**
 ******************************************************************************
 * @file           : FEB_P_Sense.h
 * @brief          : ARC Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_P_SENSE_H
#define FEB_P_SENSE_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
   
  typedef enum {
    OFF, // Initial state
    NOT_READY,
    READY
  } DSB_Status;

  
  
  #define TAG_P_SENSE "[P_SENSE]"

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */