#ifndef INC_FEB_FAN_H_
#define INC_FEB_FAN_H_

// ********************************** Includes & External **********************************

#include <stm32f0xx_hal.h>
#include <stdbool.h>

// ********************************** Defines **********************************

// Fan: DC Fan San Ace 80 (9HV0824P1G003) – https://products.sanyodenki.com/info/sanace/en/technical_material/pwm.html

#define TACH_FILTER_EXPONENT 2

#define TIMCLOCK (uint32_t)48000000
#define PRESCALAR (uint32_t)1
#define REF_CLOCK (TIMCLOCK / PRESCALAR)
#define NUM_FANS (uint32_t)5
#define PWM_SIZE (uint32_t)40
#define PWM_COUNTER ((uint32_t)((TIMCLOCK * PWM_SIZE) / 1000000u))
#define PWM_START_PERCENT 1u

#define TEMP_START_FAN 25
#define TEMP_END_FAN 45

// Fail-safe: if no BMS temp frame seen for this long, ramp fans to 100%.
#define BMS_RX_TIMEOUT_MS 2000u

// Sanyo San Ace 80 9HV0824P1G003 max rated speed (RPM). Used for tach percent.
#define FAN_MAX_RPM 14000u

// ********************************** Function Prototypes **********************************

void FEB_Fan_Init(void);

void FEB_Fan_CAN_Msg_Process(uint8_t *FEB_CAN_Rx_Data);
void FEB_Fan_Watchdog_Tick(void);
void FEB_Fan_SetManualOverride(bool enable, uint8_t percent);
void FEB_Fan_SetManualFan(uint8_t fan_idx, uint8_t percent);
int16_t FEB_Fan_GetLastMaxCellTemp(void);
uint32_t FEB_Fan_GetStalenessMs(void);
bool FEB_Fan_IsManualOverride(void);
uint8_t FEB_Fan_GetCommandedPercent(uint8_t fan_idx);
uint32_t FEB_Fan_GetCommandedCounts(uint8_t fan_idx);

void FEB_Fan_PWM_Init(void);
void FEB_Fan_All_Speed_Set(uint32_t speed);
void FEB_Fan_Speed_Set(uint8_t fan_idx, uint32_t speed);

void FEB_Fan_TACH_Init(void);
void FEB_Fan_TACH_Callback(TIM_HandleTypeDef *htim);

#endif /* INC_FEB_FAN_H_ */
