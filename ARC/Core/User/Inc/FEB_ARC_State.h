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
    DSMS_ON, //ON or OFF
    TSACTIVATION, //ON or OFF
    DSB_STATE, // Need to know ON, OFF, 
    EBS_ACTIVATED, //Y, N
    STARTUP_CHECK, // Done or not done 
    READY_TO_DRIVE, //Yes or No
    BRAKES_ENGAGED, //Y/N
    STANDSTILL, //Y,N
    SDC_OPEN_RSS, //Y, N
  } State_item;

  typedef enum {
    NO, //0
    YES //1
  } Y_N;

  typedef enum {
    OFF, //0
    ON //1
  } ON_OFF;

  typedef enum {
    NOT_OK, //0
    OK //1
  } IS_OK;

  typedef enum {
    NOT_DONE, //0
    DONE //1
  } DONE;

  uint16_t *get_state_arr();
  uint16_t get_state_at(State_item i);
  void set_state_at(State_item i, uint16_t val);

  
  #define TAG_ARC_STATE "[STATE]"

#ifdef __cplusplus
}
#endif

#endif 