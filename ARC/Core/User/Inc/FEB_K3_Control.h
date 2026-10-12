/**
 ******************************************************************************
 * @file           : FEB_K3_Control.h
 * @brief          : ARC Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_K3
#define FEB_K3

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
  
  //TODO: Define funcs and stuff here
  typedef enum {
    OPEN, // 0
    CLOSED
  } K3_Status;
  
  #define TAG_K3 "[K3]"

#ifdef __cplusplus
}
#endif

#endif