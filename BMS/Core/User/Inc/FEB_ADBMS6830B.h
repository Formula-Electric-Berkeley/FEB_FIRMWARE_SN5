#ifndef INC_FEB_ADBMS6830B_H_
#define INC_FEB_ADBMS6830B_H_

#ifdef __cplusplus
extern "C"
{
#endif

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

  // ********************************** Voltage ************************************

  float FEB_ADBMS_GET_ACC_MIN_Voltage(void);
  float FEB_ADBMS_GET_ACC_MAX_Voltage(void);
  bool FEB_ADBMS_Precharge_Complete(void);
  float FEB_ADBMS_GET_ACC_Total_Voltage(void);
  float FEB_ADBMS_GET_Cell_Voltage(uint8_t bank, uint16_t cell);
  float FEB_ADBMS_GET_Cell_Voltage_S(uint8_t bank, uint16_t cell);
  uint8_t FEB_ADBMS_GET_Cell_Violations(uint8_t bank, uint16_t cell);
  uint8_t FEB_ADBMS_GET_Cell_Discharging(uint8_t bank, uint16_t cell);

  // ********************************** Temperature ********************************

  float FEB_ADBMS_GET_ACC_AVG_Temp(void);
  float FEB_ADBMS_GET_ACC_MIN_Temp(void);
  float FEB_ADBMS_GET_ACC_MAX_Temp(void);
  float FEB_ADBMS_GET_Cell_Temperature(uint8_t bank, uint16_t cell);
  uint16_t FEB_ADBMS_GET_Therm_Raw_Code(uint8_t bank, uint16_t sensor);
  float FEB_ADBMS_GET_Therm_Raw_mV(uint8_t bank, uint16_t sensor);

  // ********************************** Balancing **********************************

  void FEB_Stop_Balance(void);             // lock-free stop request; safe from the 1ms SM task
  void FEB_Cell_Balance_ServiceStop(void); // ADBMSTask only: bus writes for a pending stop
  void FEB_Cell_Balance_Start(void);
  void FEB_Cell_Balance_Process(void);
  bool FEB_Cell_Balancing_Status(void);
  uint16_t FEB_ADBMS_GET_Balancing_Cell_Count(void); // # of cells with discharge active
  float FEB_ADBMS_GET_Cell_Voltage_Delta_mV(void);   // pack max-min cell delta in mV, -1 if no valid data
  bool FEB_Cell_Balance_Complete(void);              // true when valid readings AND delta < threshold

  // ********************************** Error Type *********************************

  uint8_t FEB_ADBMS_Get_Error_Type(void);
  void FEB_ADBMS_Update_Error_Type(uint8_t error);

// ********************************** Fault Flags (SM handoff) *******************
// Sticky flags set by the ADBMS task (under mutex) and read lock-free by the
// state machine task. 32-bit aligned reads are atomic on Cortex-M4.
#define ADBMS_FAULT_FLAG_VOLTAGE (1u << 0)
#define ADBMS_FAULT_FLAG_TEMP (1u << 1)
#define ADBMS_FAULT_FLAG_SENSOR (1u << 2) // temperature telemetry lost (too few valid sensor reads)

  /** @brief Latched cell V/T fault flags (ADBMS_FAULT_FLAG_*). */
  uint32_t FEB_ADBMS_Get_Fault_Flags(void);

  /** @brief HAL tick of last completed V/T process (0 = never). Used for the
   *  cell-monitor sensor-timeout check in the state machine. */
  uint32_t FEB_ADBMS_Get_Last_Update_Tick(void);

  // ********************************** Validation Limit Profiles ******************
  // ONE validation path (validate_voltages/validate_temps in the ADBMS task) with
  // runtime-switchable limits. The SM selects CHARGING while in
  // CHARGER_PRECHARGE/CHARGING (Li-ion charge accept tops out ~45C); every other
  // state (including BALANCE, which discharges) uses NORMAL. The charger module
  // (FEB_CAN_Charger.c) does NOT validate the pack — it only soft-gates charge
  // start/stop against the FEB_CONFIG_CELL_SOFT_* management thresholds.
  typedef enum
  {
    FEB_VALIDATION_PROFILE_NORMAL = 0, // 2800-4200 mV, -20..60 C (FEB_CELL_* macros)
    FEB_VALIDATION_PROFILE_CHARGING,   // same voltages, max temp 45.0 C
    FEB_VALIDATION_PROFILE_COUNT
  } FEB_Validation_Profile_t;

  /** @brief Select the active validation limit profile. SM task only; single
   *  aligned store, read lock-free by the ADBMS task (atomic on Cortex-M4). */
  void FEB_ADBMS_Set_Validation_Profile(FEB_Validation_Profile_t profile);

  /** @brief Currently active profile (for console/diagnostics). */
  FEB_Validation_Profile_t FEB_ADBMS_Get_Validation_Profile(void);

  // ********************************** Lock-free Snapshots ************************
  // Pack-level values published by the ADBMS task at the end of each scan and
  // readable WITHOUT the ADBMS mutex (atomic 32-bit reads). Use these — not the
  // FEB_ADBMS_GET_ACC_* getters — from the 1ms state-machine task: the mutex is
  // held for tens of ms during a temperature scan and would stall the SM.

  /** @brief Pack total voltage [V] from the last scan (0 until first scan). */
  float FEB_ADBMS_Snapshot_Total_Voltage(void);

  /** @brief Highest cell voltage [V] from the last scan. */
  float FEB_ADBMS_Snapshot_Max_Cell_Voltage(void);

  float FEB_ADBMS_Snapshot_Max_Temp(void);

  float FEB_ADBMS_Snapshot_Max_Valid_Temp(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_ADBMS6830B_H_ */
