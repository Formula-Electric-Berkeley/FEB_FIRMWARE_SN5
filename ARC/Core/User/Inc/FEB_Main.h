/**
 ******************************************************************************
 * @file           : FEB_Main.h
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
  void FEB_Init(void);
  
  #define TAG_MAIN "[MAIN]"

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */