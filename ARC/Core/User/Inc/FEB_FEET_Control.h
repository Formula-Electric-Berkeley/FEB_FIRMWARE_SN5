/**
 ******************************************************************************
 * @file           : FEB_FEET_Control.h
 * @brief          : ARC Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_FEET_H
#define FEB_FEET_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
  
  typedef enum {
    FEET_STATE_OFF,
    FEET_STATE_READY,
    FEET_STATE_DRIVING,
    FEET_STATE_FINISHED,
    FEET_STATE_EMERGENCY,
} FEET_Status;
  
void update_feet_state(void);

//Helpers
  void toggle_blue(void);
  void toggle_yellow(void);
  void set_blue(void);
  void set_yellow(void);
  void lights_off(void);

  #define TAG_FEET "[FEET]"

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */