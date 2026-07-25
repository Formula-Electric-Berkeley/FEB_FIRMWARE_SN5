/**
 ******************************************************************************
 * @file           : FEB_CAN_LinearPotentiometer.h
 * @brief          : CAN Linear Potentiometer Reporter Module Interface.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_LINEAR_POTENTIOMETER_H
#define FEB_CAN_LINEAR_POTENTIOMETER_H

#ifdef __cplusplus
extern "C"
{
#endif

  void FEB_CAN_LinearPotentiometer_Init(void);
  void FEB_CAN_LinearPotentiometer_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_LINEAR_POTENTIOMETER_H */
