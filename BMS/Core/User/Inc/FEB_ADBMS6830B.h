#ifndef INC_FEB_ADBMS6830B_H_
#define INC_FEB_ADBMS6830B_H_

// ********************************** Includes ***********************************

#include <stdint.h>
#include <stdbool.h>

// ********************************** ADBMS6830B Configuration *******************

typedef enum {
	RD_OFF = 0x00,
	RD_ON
} RD;

typedef enum {
	DCP_OFF = 0x00,
	DCP_ON
} DCP;

typedef enum {
	SINGLE = 0x00,
	CONTINUOUS
} CONT;


typedef enum {
	RSTF_OFF = 0x00,
	RSTF_ON
} RSTF;

typedef enum {
	OW_OFF_ALL_CH = 0x00,
	OW_ON_EVEN_CH,
	OW_ON_ODD_CH,
	OW_ON_ALL_CH,
} OW;

typedef enum {
	AUX_OW_OFF = 0x00,
	AUX_OW_ON
} AUX_OW;

typedef enum {
	PUP_DOWN = 0x00,
	PUP_UP
} PUP;

typedef enum {
	AUX_ALL = 0x00,
	GPIO1,
	GPIO2,
	GPIO3,
	GPIO4,
	GPIO5,
	GPIO6,
	GPIO7,
	GPIO8,
	GPIO9,
	GPIO10,
	VREF2,
	VD,
	VA,
	ITEMP,
	VPV,
	VMV,
	VRES
} AUX_CH;


// ********************************** Functions **********************************

void FEB_ADBMS_Init(void);

void FEB_ADBMS_Voltage_Process(void);
void FEB_ADBMS_Temperature_Process(void);

void FEB_Cell_Balance_Start(void);
void FEB_Cell_Balance_Process(void);
void FEB_Stop_Balance(void);

// ********************************** Voltage ************************************

float FEB_ADBMS_GET_ACC_MIN_Voltage(void);
float FEB_ADBMS_GET_ACC_MAX_Voltage(void);
bool  FEB_ADBMS_Precharge_Complete(void);
float FEB_ADBMS_GET_ACC_Total_Voltage(void);
float FEB_ADBMS_GET_Cell_Voltage(uint8_t bank, uint16_t cell);

// ********************************** Temperature ********************************

float FEB_ADBMS_GET_ACC_AVG_Temp(void);
float FEB_ADBMS_GET_ACC_MIN_Temp(void);
float FEB_ADBMS_GET_ACC_MAX_Temp(void);
float FEB_ADBMS_GET_Cell_Temperature(uint8_t bank, uint16_t cell);

// ********************************** Balancing **********************************

void FEB_Stop_Balance(void);
void FEB_Cell_Balance_Start(void);
void FEB_Cell_Balance_Process(void);
bool FEB_Cell_Balancing_Status(void);

// ********************************** Error Type *********************************

uint8_t FEB_ADBMS_Get_Error_Type(void);
void FEB_ADBMS_Update_Error_Type(uint8_t error);

#endif /* INC_FEB_ADBMS6830B_H_ */