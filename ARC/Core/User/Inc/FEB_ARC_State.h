/**
 ******************************************************************************
 * @file           : FEB_ARC_State.h
 * @brief          : ARC Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */
#ifndef FEB_ARC_STATE
#define FEB_ARC_STATE

#include "FEB_FEET_Control.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
  
  //Todo: Define state stuff here
  typedef enum {
    TPS_STATUS, // 0
    DSMS_ON, //ON or OFF
    TSACTIVATION, //ON or OFF
    DSB_STATE, // Need to know ON, OFF, 
    EBS_ACTIVATED, //Y, N
    STARTUP_CHECK, // Done or not done 
    READY_TO_DRIVE, //Yes or No
    BRAKES_ENGAGED, //Y/N
    STANDSTILL, //Y,N
    SDC_OPEN_RSS, //Y, N
    MISSION, // IDK yet, better check the CAN messages
    DRIVERLESS_STATUS, // Driverless status based on fsae documentation
    STATE_ITEM_COUNT
  } State_item;

  typedef enum {
    ARC_MISSION_MANUAL = 0,
    ARC_MISSION_AUTO,
    ARC_MISSION_UNKNOWN
  } MISSION_STATE;

  typedef enum {
    ARC_NO = 0,
    ARC_YES
  } Y_N;

  typedef enum {
    ARC_OFF = 0,
    ARC_ON
  } ON_OFF;

  typedef enum {
    ARC_NOT_OK = 0,
    ARC_OK
  } IS_OK;

  typedef enum {
    ARC_NOT_DONE = 0,
    ARC_DONE
  } DONE_STATE;

  uint16_t *get_state_arr(void);
  uint16_t get_state_at(State_item i);
  void set_state_at(State_item i, uint16_t val);
  void set_driverless_state(FEET_Status status);
  void FEB_EBS_Init(void);
  void FEB_DSB_Init(void);
  void FEB_Startup_Init(void);
  void FEB_RToD_Init(void);
  void FEB_Brakes_Init(void);
  void FEB_Standstill_Init(void);
  void FEB_SDC_Init(void);
  void FEB_Mission_Init(void);

  
  #define TAG_ARC_STATE "[STATE]"

#ifdef __cplusplus
}
#endif

#endif 