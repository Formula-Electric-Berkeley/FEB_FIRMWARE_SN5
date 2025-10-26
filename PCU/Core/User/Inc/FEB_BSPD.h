#ifndef INC_FEB_BSPD_H_
#define INC_FEB_BSPD_H_

#include "stm32f4xx_hal.h"

#include "FEB_CAN_TX.h"

typedef struct BSPD_TYPE {
    int8_t state;
} BSPD_TYPE;

/* Global variable - defined in FEB_BSPD.c */
extern BSPD_TYPE BSPD;

void FEB_BSPD_CheckReset(void);
void FEB_BSPD_CAN_Transmit(void);

#endif /* INC_FEB_BSPD_H_ */