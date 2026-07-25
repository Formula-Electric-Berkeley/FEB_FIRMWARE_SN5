#ifndef INC_FEB_CAN_DIAGNOSTICS_H_
#define INC_FEB_CAN_DIAGNOSTICS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "FEB_ADC.h"
#include "feb_can.h"
#include "feb_can_lib.h"

  void FEB_CAN_Diagnostics_TransmitBrakeData(void);
  void FEB_CAN_Diagnostics_TransmitAPPSData(void);
  void FEB_CAN_Diagnostics_TransmitPedalVoltages(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_CAN_DIAGNOSTICS_H_ */
