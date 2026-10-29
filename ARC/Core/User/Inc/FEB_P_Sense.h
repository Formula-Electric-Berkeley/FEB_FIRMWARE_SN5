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
    DSB_STATUS_OFF,
    DSB_STATUS_NOT_READY,
    DSB_STATUS_READY
  } DSB_Status;

  DSB_Status get_status(void);
  void set_status(DSB_Status status);
  void START_P_SENSE(void const *argument);
  
  #define TAG_P_SENSE "[P_SENSE]"

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */