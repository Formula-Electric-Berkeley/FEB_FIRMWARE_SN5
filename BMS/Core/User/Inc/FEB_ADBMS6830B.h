#ifndef INC_FEB_ADBMS6830B_H_
#define INC_FEB_ADBMS6830B_H_

// ********************************** Includes ***********************************

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ********************************** ADBMS6830B Configuration *******************

typedef enum
{
  RD_OFF = 0x00,
  RD_ON
} RD;

typedef enum
{
  DCP_OFF = 0x00,
  DCP_ON
} DCP;

typedef enum
{
  SINGLE = 0x00,
  CONTINUOUS
} CONT;

typedef enum
{
  RSTF_OFF = 0x00,
  RSTF_ON
} RSTF;

typedef enum
{
  OW_OFF_ALL_CH = 0x00,
  OW_ON_EVEN_CH,
  OW_ON_ODD_CH,
  OW_ON_ALL_CH,
} OW;

typedef enum
{
  AUX_OW_OFF = 0x00,
  AUX_OW_ON
} AUX_OW;

typedef enum
{
  PUP_DOWN = 0x00,
  PUP_UP
} PUP;

typedef enum
{
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

/**
 * @brief Initialize ADBMS6830B chips and validate communication
 * @return true if all ICs initialized successfully, false if validation failed
 */
bool FEB_ADBMS_Init(void);

void FEB_ADBMS_Voltage_Process(void);
void FEB_ADBMS_Temperature_Process(void);

void FEB_Cell_Balance_Start(void);
void FEB_Cell_Balance_Process(void);
void FEB_Stop_Balance(void);

// ********************************** Voltage ************************************

float FEB_ADBMS_GET_ACC_MIN_Voltage(void);
float FEB_ADBMS_GET_ACC_MAX_Voltage(void);
bool FEB_ADBMS_Precharge_Complete(void);
float FEB_ADBMS_GET_ACC_Total_Voltage(void);
float FEB_ADBMS_GET_Cell_Voltage(uint8_t bank, uint16_t cell);
float FEB_ADBMS_GET_Cell_Voltage_S(uint8_t bank, uint16_t cell);
uint8_t FEB_ADBMS_GET_Cell_Violations(uint8_t bank, uint16_t cell);

// ********************************** Temperature ********************************

float FEB_ADBMS_GET_ACC_AVG_Temp(void);
float FEB_ADBMS_GET_ACC_MIN_Temp(void);
float FEB_ADBMS_GET_ACC_MAX_Temp(void);
float FEB_ADBMS_GET_Cell_Temperature(uint8_t bank, uint16_t cell);
uint16_t FEB_ADBMS_GET_Therm_Raw_Code(uint8_t bank, uint16_t sensor);
float FEB_ADBMS_GET_Therm_Raw_mV(uint8_t bank, uint16_t sensor);

// ********************************** Balancing **********************************

void FEB_Stop_Balance(void);
void FEB_Cell_Balance_Start(void);
void FEB_Cell_Balance_Process(void);
bool FEB_Cell_Balancing_Status(void);

// ********************************** Error Type *********************************

uint8_t FEB_ADBMS_Get_Error_Type(void);
void FEB_ADBMS_Update_Error_Type(uint8_t error);

// ********************************** Fault Flags (SM handoff) *******************
// Sticky flags set by the ADBMS task (under mutex) and read lock-free by the
// state machine task. 32-bit aligned reads are atomic on Cortex-M4.
#define ADBMS_FAULT_FLAG_VOLTAGE (1u << 0)
#define ADBMS_FAULT_FLAG_TEMP (1u << 1)
#define ADBMS_FAULT_FLAG_SENSOR (1u << 2) // reserved: PEC / comm failure

/** @brief Latched cell V/T fault flags (ADBMS_FAULT_FLAG_*). */
uint32_t FEB_ADBMS_Get_Fault_Flags(void);

/** @brief HAL tick of last completed V/T process (0 = never). Used for the
 *  cell-monitor sensor-timeout check in the state machine. */
uint32_t FEB_ADBMS_Get_Last_Update_Tick(void);

// ********************************** Lock-free Snapshots ************************
// Pack-level values published by the ADBMS task at the end of each scan and
// readable WITHOUT the ADBMS mutex (atomic 32-bit reads). Use these — not the
// FEB_ADBMS_GET_ACC_* getters — from the 1ms state-machine task: the mutex is
// held for tens of ms during a temperature scan and would stall the SM.

/** @brief Pack total voltage [V] from the last scan (0 until first scan). */
float FEB_ADBMS_Snapshot_Total_Voltage(void);

/** @brief Highest cell voltage [V] from the last scan. */
float FEB_ADBMS_Snapshot_Max_Cell_Voltage(void);

/** @brief Highest pack temperature [C] from the last scan (NaN until first scan). */
float FEB_ADBMS_Snapshot_Max_Temp(void);

#endif /* INC_FEB_ADBMS6830B_H_ */
