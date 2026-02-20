// ********************************** Includes & Externs *************************

#include "FEB_ADBMS6830B.h"
#include "FEB_SM.h"
#include "FEB_HW.h"
#include "FEB_Const.h"
#include "FEB_Config.h"
// #include "FEB_CAN_IVT.h"
#include "FEB_CMDCODES.h"
#include "FEB_Cell_Temp_LUT.h"
#include "FEB_AD68xx_Interface.h"
#include "FEB_ADBMS6830B_Driver.h"

#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include "defs.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "bms_tasks.h"

// ********************************** Variables **********************************

cell_asic IC_Config[FEB_NUM_IC];
accumulator_t FEB_ACC = {0};

int balancing_cycle = 0;
uint16_t balancing_mask = 0xAAAA;

uint8_t ERROR_TYPE = 0; // HEXDIGIT 1 voltage faults; HEXDIGIT 2 temp faults; HEXDIGIT 3 relay faults

// ********************************** Config Bits ********************************

static bool refon = 0;
static bool cth_bits[3] = {1, 1, 1};
static bool gpio_bits[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static bool dcto_bits[6] = {1, 1, 1, 1, 1, 1};
static uint16_t uv = 0x0010;
static uint16_t ov = 0x3FF0;

static float FEB_MIN_SLIPPAGE_V = 0.03;

// ********************************** Helper Functions ***************************

static inline float convert_voltage(int16_t raw_code)
{
  return raw_code * ADBMS_ADC_LSB_V + ADBMS_ADC_OFFSET_V;
}

// ********************************** Static Functions ***************************

// ********************************** Voltage ************************************

static void start_adc_cell_voltage_measurements()
{
  DEBUG_VOLTAGE_PRINT("Starting ADC cell voltage measurements");
  ADBMS6830B_adcv(1, 0, 1, 0, OWVR);
  osDelay(pdMS_TO_TICKS(1));
  // ADBMS6830B_pollAdc();
  DEBUG_VOLTAGE_PRINT("ADC cell voltage measurement command sent");
}

// Helper function to check and report PEC errors to redundancy system
static void check_and_report_pec_errors()
{
#if (ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
  // Check if any PEC errors occurred across all ICs
  bool pec_error_detected = false;

  for (uint8_t ic = 0; ic < FEB_NUM_IC; ic++)
  {
    // Check cell voltage PEC errors
    for (uint8_t reg = 0; reg < 6; reg++)
    {
      if (IC_Config[ic].cells.pec_match[reg] != 0)
      {
        pec_error_detected = true;
        break;
      }
    }

    // Check aux register PEC errors
    for (uint8_t reg = 0; reg < 4; reg++)
    {
      if (IC_Config[ic].aux.pec_match[reg] != 0)
      {
        pec_error_detected = true;
        break;
      }
    }

    // Check config register PEC errors
    if (IC_Config[ic].configa.rx_pec_match != 0 || IC_Config[ic].configb.rx_pec_match != 0)
    {
      pec_error_detected = true;
    }

    if (pec_error_detected)
      break;
  }

  // Report to redundancy system
  if (pec_error_detected)
  {
    FEB_spi_report_pec_error();
    // Log PEC error for diagnostics
    static uint32_t pec_error_count = 0;
    pec_error_count++;
    if ((pec_error_count % 10) == 1)
    { // Log every 10th error to avoid spam
      printf("[ADBMS] PEC error detected (count: %u)\r\n", (unsigned int)pec_error_count);
    }
  }
  else
  {
    FEB_spi_report_pec_success();
  }
#endif
}

static void read_cell_voltages()
{
  DEBUG_VOLTAGE_PRINT("Reading cell voltages from %d ICs", FEB_NUM_IC);
  ADBMS6830B_rdcv(FEB_NUM_IC, IC_Config);
  ADBMS6830B_rdsv(FEB_NUM_IC, IC_Config);
  DEBUG_VOLTAGE_PRINT("Cell voltage read complete");

  // Check and report PEC errors for redundancy failover
  check_and_report_pec_errors();
}

static void store_cell_voltages()
{
  DEBUG_VOLTAGE_PRINT("Storing cell voltages for %d banks", FEB_NBANKS);
  FEB_ACC.total_voltage_V = 0;

  float min_cell_V = FLT_MAX;
  float max_cell_V = -FLT_MAX;

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_ACC.banks[bank].badReadV = 0; // Reset PEC error counter for this bank

    for (uint8_t ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      uint8_t ic_idx = ic + bank * FEB_NUM_ICPBANK;

      for (uint8_t cell = 0; cell < FEB_NUM_CELLS_PER_IC; cell++)
      {
        // Check PEC error for this cell's register group (3 cells per register)
        uint8_t reg_idx = cell / 3;
        if (IC_Config[ic_idx].cells.pec_match[reg_idx] != 0)
        {
          // PEC error detected - skip this cell and count as bad read
          FEB_ACC.banks[bank].badReadV++;
          DEBUG_VOLTAGE_PRINT("PEC error: Bank %d IC %d Cell %d Reg %d", bank, ic, cell, reg_idx);
          continue;
        }

        float CVoltage = convert_voltage(IC_Config[ic_idx].cells.c_codes[cell]);
        float SVoltage = convert_voltage(IC_Config[ic_idx].cells.s_codes[cell]);

        FEB_ACC.banks[bank].cells[cell + ic * FEB_NUM_CELLS_PER_IC].voltage_V = CVoltage;
        FEB_ACC.banks[bank].cells[cell + ic * FEB_NUM_CELLS_PER_IC].voltage_S = SVoltage;
        FEB_ACC.total_voltage_V += CVoltage;

        if (CVoltage >= 0.0f)
        {
          if (CVoltage < min_cell_V)
            min_cell_V = CVoltage;
          if (CVoltage > max_cell_V)
            max_cell_V = CVoltage;
        }
      }
    }
    DEBUG_VOLTAGE_PRINT("Bank %d: badReadV=%d", bank, FEB_ACC.banks[bank].badReadV);
  }
  FEB_ACC.pack_min_voltage_V = min_cell_V;
  FEB_ACC.pack_max_voltage_V = max_cell_V;
  DEBUG_VOLTAGE_PRINT("Voltage storage complete: Total=%.3fV Min=%.3fV Max=%.3fV", FEB_ACC.total_voltage_V, min_cell_V,
                      max_cell_V);
}

static void validate_voltages()
{
  DEBUG_VOLTAGE_PRINT("Validating voltages");
  uint16_t vMax = FEB_Config_Get_Cell_Max_Voltage_mV();
  uint16_t vMin = FEB_Config_Get_Cell_Min_Voltage_mV();
  DEBUG_VOLTAGE_PRINT("Voltage limits: Min=%.3fV Max=%.3fV", vMin / 1000.0f, vMax / 1000.0f);

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    // Note: badReadV is now set in store_cell_voltages() via PEC checking
    for (uint8_t cell = 0; cell < FEB_NUM_CELL_PER_BANK; cell++)
    {
      float voltageC = FEB_ACC.banks[bank].cells[cell].voltage_V * 1000;
      float voltageS = FEB_ACC.banks[bank].cells[cell].voltage_S * 1000;
      if (voltageC > vMax || voltageC < vMin)
      {
        DEBUG_VOLTAGE_PRINT("Voltage violation detected: Bank %d Cell %d C=%.3fV S=%.3fV", bank, cell,
                            voltageC / 1000.0f, voltageS / 1000.0f);
        // Check redundant S-code measurement to confirm violation
        if (voltageS > vMax || voltageS < vMin)
        {
          FEB_ACC.banks[bank].cells[cell].violations += 1;
          DEBUG_VOLTAGE_PRINT("Both C and S codes confirm violation: violations=%d",
                              FEB_ACC.banks[bank].cells[cell].violations);
          if (FEB_ACC.banks[bank].cells[cell].violations == FEB_VOLTAGE_ERROR_THRESH)
          {
            printf("[ADBMS] FAULT: Cell voltage out of range - Bank %d Cell %d: %.3fV (limits: %.3f-%.3fV)\r\n", bank,
                   cell, voltageC / 1000.0f, vMin / 1000.0f, vMax / 1000.0f);
            FEB_ADBMS_Update_Error_Type(ERROR_TYPE_VOLTAGE_VIOLATION);
            // FEB_SM_Transition(FEB_SM_ST_FAULT_BMS);
          }
        }
        else
        {
          DEBUG_VOLTAGE_PRINT("S-code does not confirm violation, resetting counter");
          FEB_ACC.banks[bank].cells[cell].violations = 0;
        }
      }
      else
      {
        FEB_ACC.banks[bank].cells[cell].violations = 0;
      }
    }
  }
  DEBUG_VOLTAGE_PRINT("Voltage validation complete");
}

// ********************************** Temperature ********************************

static void configure_gpio_bits(uint8_t channel)
{
  DEBUG_TEMP_PRINT("Configuring GPIO bits for channel %d", channel);
  gpio_bits[0] = 0b1;                    /* ADC Channel */
  gpio_bits[1] = 0b1;                    /* ADC Channel */
  gpio_bits[2] = ((channel >> 0) & 0b1); /* MUX Sel 1 */
  gpio_bits[3] = ((channel >> 1) & 0b1); /* MUX Sel 1 */
  gpio_bits[4] = ((channel >> 2) & 0b1); /* MUX Sel 1 */
  gpio_bits[5] = 0b1;                    /* ADC Channel */
  gpio_bits[6] = 0b1;                    /* ADC Channel */
  DEBUG_TEMP_PRINT("GPIO bits configured: [0]=%d [1]=%d [2]=%d [3]=%d [4]=%d [5]=%d [6]=%d", gpio_bits[0], gpio_bits[1],
                   gpio_bits[2], gpio_bits[3], gpio_bits[4], gpio_bits[5], gpio_bits[6]);
  for (uint8_t icn = 0; icn < FEB_NUM_IC; icn++)
  {
    ADBMS6830B_set_cfgr(icn, IC_Config, refon, cth_bits, gpio_bits, 0, dcto_bits, uv, ov);
  }
  ADBMS6830B_wrcfga(FEB_NUM_IC, IC_Config);
  DEBUG_TEMP_PRINT("GPIO configuration written to %d ICs", FEB_NUM_IC);
}

static void start_aux_voltage_measurements()
{
  DEBUG_TEMP_PRINT("Starting aux voltage measurements");
  ADBMS6830B_adax(AUX_OW_OFF, PUP_DOWN, 1);
  osDelay(pdMS_TO_TICKS(2));
  // ADBMS6830B_pollAdc();
  DEBUG_TEMP_PRINT("Aux measurement 1 complete");
  ADBMS6830B_adax(AUX_OW_OFF, PUP_DOWN, 2);
  osDelay(pdMS_TO_TICKS(2));
  DEBUG_TEMP_PRINT("Aux measurement 2 complete");
}

static void read_aux_voltages()
{
  DEBUG_TEMP_PRINT("Reading aux voltages from %d ICs", FEB_NUM_IC);
  ADBMS6830B_rdaux(FEB_NUM_IC, IC_Config);
  DEBUG_TEMP_PRINT("Aux voltage read complete");

  // Check and report PEC errors for redundancy failover
  check_and_report_pec_errors();
}

static void store_cell_temps(uint8_t channel)
{
  DEBUG_TEMP_PRINT("Storing cell temperatures for channel %d", channel);
  float total_temp_C = 0.0f;
  uint16_t temp_count = 0;

  float min_temp_C = FLT_MAX;
  float max_temp_C = -FLT_MAX;

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (uint8_t icn = 0; icn < FEB_NUM_ICPBANK; icn++)
    {
      uint16_t mux1 = IC_Config[FEB_NUM_ICPBANK * bank + icn].aux.a_codes[0];
      uint16_t mux2 = IC_Config[FEB_NUM_ICPBANK * bank + icn].aux.a_codes[1];
      float V1 = (convert_voltage(mux1) * 1000);
      float V2 = (convert_voltage(mux2) * 1000);

      float T1 = FEB_Cell_Temp_LUT_Get_Temp_100mC((int)V1) * 0.1f;
      float T2 = FEB_Cell_Temp_LUT_Get_Temp_100mC((int)V2) * 0.1f;

      DEBUG_TEMP_PRINT("Bank %d IC %d: V1=%.1fmV V2=%.1fmV T1=%.1f°C T2=%.1f°C", bank, icn, V1, V2, T1, T2);

      FEB_ACC.banks[bank].temp_sensor_readings_V[icn * FEB_NUM_TEMP_SENSE_PER_IC + channel] = T1;
      FEB_ACC.banks[bank].temp_sensor_readings_V[icn * FEB_NUM_TEMP_SENSE_PER_IC + channel + 5] = T2;

      if (T1 >= 0)
      {
        if (T1 < min_temp_C)
          min_temp_C = T1;
        if (T1 > max_temp_C)
          max_temp_C = T1;
        total_temp_C += T1;
        temp_count++;
      }
    }
  }

  if (temp_count > 0)
  {
    FEB_ACC.pack_min_temp = min_temp_C;
    FEB_ACC.pack_max_temp = max_temp_C;
    FEB_ACC.average_pack_temp = total_temp_C / (double)temp_count;
    DEBUG_TEMP_PRINT("Channel %d temps stored: Count=%d Min=%.1f°C Max=%.1f°C Avg=%.1f°C", channel, temp_count,
                     min_temp_C, max_temp_C, FEB_ACC.average_pack_temp);
  }
  else
  {
    DEBUG_TEMP_PRINT("Channel %d: No valid temperature readings", channel);
  }
}

static void validate_temps()
{
  DEBUG_TEMP_PRINT("Validating temperatures");
  uint16_t tMax = FEB_Config_Get_Cell_Max_Temperature_dC();
  uint16_t tMin = FEB_Config_Get_Cell_Min_Temperature_dC();
  DEBUG_TEMP_PRINT("Temperature limits: Min=%.1f°C Max=%.1f°C", tMin / 10.0f, tMax / 10.0f);
  int totalReads = 0;

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_ACC.banks[bank].tempRead = 0;
    for (uint8_t cell = 0; cell < FEB_NUM_CELL_PER_BANK; cell++)
    {
      float temp = FEB_ACC.banks[bank].temp_sensor_readings_V[cell] * 10;

      // Check if temperature is within physically reasonable range
      // Valid range: -40°C to +85°C (typical automotive operating range)
      if (temp >= TEMP_VALID_MIN_DC && temp <= TEMP_VALID_MAX_DC)
      {
        FEB_ACC.banks[bank].tempRead += 1;
      }
      else
      {
        // Invalid reading - outside physically reasonable range
        DEBUG_TEMP_PRINT("Invalid temp reading: Bank %d Cell %d Temp=%.1f°C (outside valid range)", bank, cell,
                         temp / 10.0f);
        continue;
      }

      if (temp > tMax || temp < (float)tMin)
      {
        DEBUG_TEMP_PRINT("Temperature violation: Bank %d Cell %d Temp=%.1f°C violations=%d", bank, cell, temp / 10.0f,
                         FEB_ACC.banks[bank].temp_violations[cell] + 1);
        FEB_ACC.banks[bank].temp_violations[cell]++;
        if (FEB_ACC.banks[bank].temp_violations[cell] == FEB_TEMP_ERROR_THRESH)
        {
          printf("[ADBMS] FAULT: Cell temperature out of range - Bank %d Sensor %d: %.1f°C (limits: %.1f-%.1f°C)\r\n",
                 bank, cell, temp / 10.0f, tMin / 10.0f, tMax / 10.0f);
          FEB_ADBMS_Update_Error_Type(ERROR_TYPE_TEMP_VIOLATION);
          // FEB_SM_Transition(FEB_SM_ST_FAULT_BMS);
        }
      }
      else
      {
        FEB_ACC.banks[bank].temp_violations[cell] = 0;
      }
    }
    totalReads += FEB_ACC.banks[bank].tempRead;
    DEBUG_TEMP_PRINT("Bank %d: tempRead=%d", bank, FEB_ACC.banks[bank].tempRead);
  }

  float read_ratio = totalReads / (float)(FEB_NUM_CELL_PER_BANK * FEB_NBANKS);
  DEBUG_TEMP_PRINT("Total reads: %d/%d (%.1f%%)", totalReads, FEB_NUM_CELL_PER_BANK * FEB_NBANKS, read_ratio * 100.0f);
  if (read_ratio < 0.2)
  {
    DEBUG_TEMP_PRINT("WARNING: Low temperature read ratio (%.1f%%)", read_ratio * 100.0f);
    FEB_ADBMS_Update_Error_Type(ERROR_TYPE_LOW_TEMP_READS);
    // FEB_SM_Transition(FEB_SM_ST_FAULT_BMS);
  }
  DEBUG_TEMP_PRINT("Temperature validation complete");
}

// ********************************** Balancing **********************************

static void determineMinV()
{
  transmitCMD(ADCV | AD_CONT | AD_RD);
  osDelay(pdMS_TO_TICKS(1));
  read_cell_voltages();
  store_cell_voltages();
  validate_voltages();
}

// ********************************** Functions **********************************

void FEB_ADBMS_Init()
{

  printf("[ADBMS] Initializing ADBMS\r\n");
  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_ACC.banks[bank].badReadV = 0;
    FEB_ACC.banks[bank].tempRead = 0;
    FEB_ACC.banks[bank].total_voltage_V = 0;
    for (uint8_t ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      for (uint8_t cell = 0; cell < FEB_NUM_CELLS_PER_IC; cell++)
      {
        FEB_ACC.banks[bank].cells[cell].voltage_V = 0;
        FEB_ACC.banks[bank].cells[cell].voltage_S = 0;
        FEB_ACC.banks[bank].cells[cell].violations = 0;
        FEB_ACC.banks[bank].cells[cell].discharging = 0;
        FEB_ACC.banks[bank].temp_violations[cell] = 0;
      }
    }
  }

  // Read the Serial ID for each ADBMS IC in the daisy chain
  ADBMS6830B_rdsid(FEB_NUM_IC, IC_Config);
  osDelay(pdMS_TO_TICKS(1));
  printf("[ADBMS] Serial IDs read for %d ICs\r\n", FEB_NUM_IC);
  for (uint8_t i = 0; i < FEB_NUM_IC; i++)
  {
    printf("[ADBMS] IC%d SID: %02X:%02X:%02X:%02X:%02X:%02X\r\n", i, IC_Config[i].sid[0], IC_Config[i].sid[1],
           IC_Config[i].sid[2], IC_Config[i].sid[3], IC_Config[i].sid[4], IC_Config[i].sid[5]);
  }

  printf("[ADBMS] Initializing ADBMS Configuration\r\n");
  FEB_cs_high();
  printf("[ADBMS] High CS\r\n");
  ADBMS6830B_init_cfg(FEB_NUM_IC, IC_Config);
  printf("[ADBMS] Resetting ADBMS CRC Count\r\n");
  ADBMS6830B_reset_crc_count(FEB_NUM_IC, IC_Config);
  printf("[ADBMS] Initializing ADBMS Register Limits\r\n");
  ADBMS6830B_init_reg_limits(FEB_NUM_IC, IC_Config);
  printf("[ADBMS] Writing ADBMS Configuration to ICs\r\n");
  ADBMS6830B_wrALL(FEB_NUM_IC, IC_Config);
  printf("[ADBMS] ADBMS Configuration Initialized\r\n");
}

void FEB_ADBMS_Voltage_Process()
{
  DEBUG_VOLTAGE_PRINT("=== Voltage Process Started ===");
  start_adc_cell_voltage_measurements();
  read_cell_voltages();
  store_cell_voltages();
  validate_voltages();
  DEBUG_VOLTAGE_PRINT("=== Voltage Process Completed ===");
}

void FEB_ADBMS_Temperature_Process()
{
  DEBUG_TEMP_PRINT("=== Temperature Process Started ===");
  gpio_bits[9] ^= 0b1;
  DEBUG_TEMP_PRINT("Toggled gpio_bits[9] to %d", gpio_bits[9]);
  for (uint8_t channel = 0; channel < 5; channel++)
  {
    DEBUG_TEMP_PRINT("--- Processing channel %d ---", channel);
    configure_gpio_bits(channel);
    start_aux_voltage_measurements();
    read_aux_voltages();
    store_cell_temps(channel);
    DEBUG_TEMP_PRINT("--- Channel %d complete ---", channel);
  }
  validate_temps();
  DEBUG_TEMP_PRINT("=== Temperature Process Completed ===");
}

// ********************************** Voltage ************************************

float FEB_ADBMS_GET_ACC_Total_Voltage()
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float voltage = FEB_ACC.total_voltage_V;
  osMutexRelease(ADBMSMutexHandle);
  return voltage;
}

float FEB_ADBMS_GET_ACC_MIN_Voltage()
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float voltage = FEB_ACC.pack_min_voltage_V;
  osMutexRelease(ADBMSMutexHandle);
  return voltage;
}

float FEB_ADBMS_GET_ACC_MAX_Voltage()
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float voltage = FEB_ACC.pack_max_voltage_V;
  osMutexRelease(ADBMSMutexHandle);
  return voltage;
}

float FEB_ADBMS_GET_Cell_Voltage(uint8_t bank, uint16_t cell)
{
  if (bank >= FEB_NBANKS || cell >= FEB_NUM_CELL_PER_BANK)
  {
    return -1.0f;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float voltage = FEB_ACC.banks[bank].cells[cell].voltage_V;
  osMutexRelease(ADBMSMutexHandle);
  return voltage;
}

bool FEB_ADBMS_Precharge_Complete(void)
{
  // float voltage_V = (float)FEB_IVT_V1_Voltage() * 0.001f;
  // return (voltage_V >= (0.9f * FEB_ADBMS_GET_ACC_Total_Voltage()));
  return false;
}

// ********************************** Temperature ********************************

float FEB_ADBMS_GET_ACC_AVG_Temp()
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float temp = FEB_ACC.average_pack_temp;
  osMutexRelease(ADBMSMutexHandle);
  return temp;
}

float FEB_ADBMS_GET_ACC_MIN_Temp()
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float temp = FEB_ACC.pack_min_temp;
  osMutexRelease(ADBMSMutexHandle);
  return temp;
}

float FEB_ADBMS_GET_ACC_MAX_Temp()
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float temp = FEB_ACC.pack_max_temp;
  osMutexRelease(ADBMSMutexHandle);
  return temp;
}

float FEB_ADBMS_GET_Cell_Temperature(uint8_t bank, uint16_t cell)
{
  if (bank >= FEB_NBANKS || cell >= FEB_NUM_TEMP_SENSORS)
  {
    return -1.0f;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float temp = FEB_ACC.banks[bank].temp_sensor_readings_V[cell];
  osMutexRelease(ADBMSMutexHandle);
  return temp;
}

// ********************************** Print Accumulator **************************

void FEB_ADBMS_Print_Accumulator(void)
{
  osMutexAcquire(ADBMSMutexHandle, osWaitForever);

  printf("\r\n========== ACCUMULATOR STATUS ==========\r\n");
  printf("Pack Total Voltage: %.3fV\r\n", FEB_ACC.total_voltage_V);
  printf("Pack Min Voltage: %.3fV\r\n", FEB_ACC.pack_min_voltage_V);
  printf("Pack Max Voltage: %.3fV\r\n", FEB_ACC.pack_max_voltage_V);
  printf("Pack Min Temp: %.1f°C\r\n", FEB_ACC.pack_min_temp);
  printf("Pack Max Temp: %.1f°C\r\n", FEB_ACC.pack_max_temp);
  printf("Pack Avg Temp: %.1f°C\r\n", FEB_ACC.average_pack_temp);
  printf("Error Type: 0x%02X\r\n", FEB_ACC.error_type);

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    printf("\r\n--- Bank %d ---\r\n", bank);
    printf("  Total Voltage: %.3fV\r\n", FEB_ACC.banks[bank].total_voltage_V);
    printf("  Min Voltage: %.3fV, Max Voltage: %.3fV\r\n", FEB_ACC.banks[bank].min_voltage_V,
           FEB_ACC.banks[bank].max_voltage_V);
    printf("  Avg Temp: %.1f°C, Min Temp: %.1f°C, Max Temp: %.1f°C\r\n", FEB_ACC.banks[bank].avg_temp_C,
           FEB_ACC.banks[bank].min_temp_C, FEB_ACC.banks[bank].max_temp_C);
    printf("  Volt Reads: %d, Temp Reads: %d, Bad Volt Reads: %d\r\n", FEB_ACC.banks[bank].voltRead,
           FEB_ACC.banks[bank].tempRead, FEB_ACC.banks[bank].badReadV);

    printf("  Cell Voltages: ");
    for (uint16_t cell = 0; cell < FEB_NUM_CELL_PER_BANK; cell++)
    {
      printf("%.3f ", FEB_ACC.banks[bank].cells[cell].voltage_V);
    }
    printf("\r\n");

    printf("  Cell Temps: ");
    for (uint16_t cell = 0; cell < FEB_NUM_TEMP_SENSORS; cell++)
    {
      printf("%.1f ", FEB_ACC.banks[bank].temp_sensor_readings_V[cell]);
    }
    printf("\r\n");
  }

  printf("==========================================\r\n");

  osMutexRelease(ADBMSMutexHandle);
}

// ********************************** Balancing **********************************

void FEB_Cell_Balance_Start()
{
  FEB_cs_high();
  ADBMS6830B_init_cfg(FEB_NUM_IC, IC_Config);
  ADBMS6830B_wrALL(FEB_NUM_IC, IC_Config);
  FEB_Cell_Balance_Process();
}

void FEB_Cell_Balance_Process()
{
  // if (FEB_SM_Get_Current_State() != FEB_SM_ST_BALANCING) {
  // 	return;
  // }

  FEB_Stop_Balance();
  determineMinV();

  if (balancing_cycle == 3)
  {
    balancing_mask = ~balancing_mask;
    balancing_cycle = 0;
  }
  balancing_cycle++;
  // Use the actual minimum voltage from the pack instead of static value
  float min_cell_voltage = FEB_ACC.pack_min_voltage_V;

  for (uint8_t icn = 0; icn < FEB_NUM_IC; icn++)
  {
    uint16_t bits = 0x0000;
    for (uint8_t cell = 0; cell < FEB_NUM_CELLS_PER_IC; cell++)
    {
      float volt =
          FEB_ACC.banks[icn / FEB_NUM_ICPBANK].cells[cell + FEB_NUM_CELLS_PER_IC * (icn % FEB_NUM_ICPBANK)].voltage_V;
      float diff = volt - min_cell_voltage;
      if (diff > FEB_MIN_SLIPPAGE_V)
      {
        bits |= (0b1 << cell);
        FEB_ACC.banks[icn / FEB_NUM_ICPBANK].cells[cell + FEB_NUM_CELLS_PER_IC * (icn % FEB_NUM_ICPBANK)].discharging =
            0b1 & ((balancing_mask & bits) >> cell);
      }
      else
      {
        FEB_ACC.banks[icn / FEB_NUM_ICPBANK].cells[cell + FEB_NUM_CELLS_PER_IC * (icn % FEB_NUM_ICPBANK)].discharging =
            0b0;
      }
    }
    ADBMS6830B_set_cfgr(icn, IC_Config, refon, cth_bits, gpio_bits, (bits & balancing_mask), dcto_bits, uv, ov);
  }
  ADBMS6830B_wrcfgb(FEB_NUM_IC, IC_Config);
}

bool FEB_Cell_Balancing_Status(void)
{
  float min_v = FLT_MAX;
  float max_v = FLT_MIN;

  for (size_t i = 0; i < FEB_NBANKS; ++i)
  {
    for (size_t j = 0; j < FEB_NUM_CELL_PER_BANK; ++j)
    {
      const float voltage = FEB_ADBMS_GET_Cell_Voltage(i, j) * 1000.0f;
      const float temp = FEB_ADBMS_GET_Cell_Temperature(i, j) * 10.0f;

      if (temp >= FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC)
      {
        return false;
      }

      if (voltage < 0)
      {
        continue;
      }

      if (voltage < min_v)
        min_v = voltage;
      if (voltage > max_v)
        max_v = voltage;
    }
  }

  if (max_v < 0 || min_v > 1e8f)
  {
    return false;
  }

  const float delta_v = max_v - min_v;
  if (delta_v >= FEB_MIN_SLIPPAGE_V)
  {
    return true;
  }

  return false;
}

void FEB_Stop_Balance()
{
  for (uint8_t ic = 0; ic < FEB_NUM_IC; ic++)
  {
    ADBMS6830B_set_cfgr(ic, IC_Config, refon, cth_bits, gpio_bits, 0, dcto_bits, uv, ov);
  }
  ADBMS6830B_wrALL(FEB_NUM_IC, IC_Config);
  transmitCMD(ADCV | AD_DCP);
}

// ********************************** Error Type *********************************

uint8_t FEB_ADBMS_Get_Error_Type()
{
  return ERROR_TYPE;
}

void FEB_ADBMS_Update_Error_Type(uint8_t error)
{
  ERROR_TYPE = error;
}
