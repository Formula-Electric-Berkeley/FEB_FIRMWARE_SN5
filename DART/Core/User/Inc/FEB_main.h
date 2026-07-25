#ifndef INC_FEB_MAIN_H_
#define INC_FEB_MAIN_H_

#ifdef __cplusplus
extern "C"
{
#endif

  // ********************************** Includes & External **********************************

#include "FEB_Fan.h"
#include "FEB_CAN.h"
#include <string.h>

  // ********************************** Function Prototypes **********************************

  void FEB_Init(void);
  void FEB_Main_Loop(void);
  void FEB_Error_Handler();

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_MAIN_H_ */
