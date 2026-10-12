/**
 ******************************************************************************
 * @file           : FEB_ARC_State.h
 * @brief          : ARC Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */
#include <stdint.h>

#ifndef FEB_ARC_STATE
#define FEB_ARC_STATE

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
  
  //Todo: Define state stuff here
  typedef enum {
    TPS_STATUS // 0
    //Add more stuff here
  } State_item;

  uint16_t *get_state_arr();
  uint16_t get_state_at(State_item i);
  void set_state_at(State_item i, uint16_t val);

  
  #define TAG_ARC_STATE "[STATE]"

#ifdef __cplusplus
}
#endif

#endif 