/**
 ******************************************************************************
 * @file           : FEB_FEET_Control.h
 * @brief          : ARC Application Header
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
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
    //Function defs here:
  

  typedef enum {
    OFF,       // Both lights off
    READY,     // Yellow continuous
    DRIVING,   // Yellow  flashing
    FINISHED,  // Blue continuous
    EMERGENCY, // Blue flashing
} FEET_Status;
  
  #define TAG_FEET "[FEET]"

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */