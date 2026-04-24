// ********************************** Includes & Externs *************************

#include "FEB_ADBMS6830B.h"
#include "FEB_SM.h"
#include "FEB_HW.h"
#include "FEB_Const.h"
#include "FEB_Config.h"
#include "FEB_Commands.h"
// #include "FEB_CAN_IVT.h"
#include "FEB_CMDCODES.h"
#include "FEB_Thermistor.h"
#include "FEB_AD68xx_Interface.h"
#include "FEB_ADBMS6830B_Driver.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "feb_log.h"

/* External mutex from freertos.c */
extern osMutexId_t ADBMSMutexHandle;

/* Debug macros using UART logging */
#define TAG_VOLTAGE "[VOLT]"
#define TAG_TEMP "[TEMP]"
#define TAG_BALANCE "[BALANCE]"
// #define DEBUG_VOLTAGE_PRINT(...) LOG_D(TAG_VOLTAGE, __VA_ARGS__)
// #define DEBUG_TEMP_PRINT(...) LOG_D(TAG_TEMP, __VA_ARGS__)
#define DEBUG_VOLTAGE_PRINT(...) ((void)0)
#define DEBUG_TEMP_PRINT(...) ((void)0)

// ********************************** Variables **********************************

cell_asic IC_Config[FEB_NUM_IC];
accumulator_t FEB_ACC = {0};

int balancing_cycle = 0;
uint16_t balancing_mask = 0xAAAA;

uint8_t ERROR_TYPE = 0; // HEXDIGIT 1 voltage faults; HEXDIGIT 2 temp faults; HEXDIGIT 3 relay faults

// ********************************** Config Bits ********************************

static bool refon = 1;
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

  // Start C-ADC (primary voltage)
  ADBMS6830B_adcv(1, 0, 1, 0, ADBMS_OW_DETECTION_MODE);
  /* Note: Using osDelay instead of ADBMS6830B_pollAdc() because:
   * - pollAdc() uses busy-wait which blocks the RTOS task scheduler
   * - ADBMS6830B typical ADC conversion time is ~500µs to 1ms
   * - 2ms delay provides sufficient margin and allows other tasks to run
   */
  osDelay(pdMS_TO_TICKS(2));

  // Start S-ADC (secondary/redundant voltage)
  transmitCMD(ADSV);
  osDelay(pdMS_TO_TICKS(2));
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
    for (uint8_t cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
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
  // GPIO1-6 are ADC inputs for MUX6..MUX1 outputs respectively.
  // a_codes[0]=GPIO1=MUX6, a_codes[1]=GPIO2=MUX5, ..., a_codes[5]=GPIO6=MUX1.
  gpio_bits[0] = 0b1; /* GPIO1 = ADC (MUX6 out) */
  gpio_bits[1] = 0b1; /* GPIO2 = ADC (MUX5 out) */
  gpio_bits[2] = 0b1; /* GPIO3 = ADC (MUX4 out) */
  gpio_bits[3] = 0b1; /* GPIO4 = ADC (MUX3 out) */
  gpio_bits[4] = 0b1; /* GPIO5 = ADC (MUX2 out) */
  gpio_bits[5] = 0b1; /* GPIO6 = ADC (MUX1 out) */
  // GPIO7-9 drive MUX SEL1/SEL2/SEL3 (3-bit channel select, 0..6 used).
  gpio_bits[6] = ((channel >> 0) & 0b1); /* GPIO7 = MUX SEL1 */
  gpio_bits[7] = ((channel >> 1) & 0b1); /* GPIO8 = MUX SEL2 */
  gpio_bits[8] = ((channel >> 2) & 0b1); /* GPIO9 = MUX SEL3 */
  DEBUG_TEMP_PRINT("GPIO sel bits [6..8]=%d%d%d (channel=%d)", gpio_bits[6], gpio_bits[7], gpio_bits[8], channel);
  for (uint8_t icn = 0; icn < FEB_NUM_IC; icn++)
  {
    ADBMS6830B_set_cfgr(icn, IC_Config, refon, cth_bits, gpio_bits, 0, dcto_bits, uv, ov);
  }
  ADBMS6830B_wrcfga(FEB_NUM_IC, IC_Config);
  DEBUG_TEMP_PRINT("GPIO configuration written to %d ICs", FEB_NUM_IC);
}

static void start_aux_voltage_measurements()
{
  DEBUG_TEMP_PRINT("Starting aux voltage measurements (all GPIOs)");
  /* Note: Using osDelay instead of ADBMS6830B_pollAdc() - see comment in
   * start_adc_cell_voltage_measurements() for rationale */
  /* CH=0 converts all GPIO channels; we need GPIO1-6 for the 6 MUX outputs. */
  ADBMS6830B_adax(AUX_OW_OFF, PUP_DOWN, 0);
  osDelay(pdMS_TO_TICKS(5));
  DEBUG_TEMP_PRINT("Aux all-channel measurement complete");
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

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (uint8_t icn = 0; icn < FEB_NUM_ICPBANK; icn++)
    {
      uint8_t ic_idx = FEB_NUM_ICPBANK * bank + icn;

      // a_codes[0..5] map to MUX6..MUX1 (GPIO1..GPIO6). Iterate physical MUX 1..6 and
      // store contiguously: sensors [mux*7 + channel] so MUX1 occupies [0..6], MUX2 [7..13], etc.
      for (uint8_t mux = 0; mux < 6; mux++)
      {
        uint8_t a_idx = 5 - mux; // MUX1=a_codes[5], MUX6=a_codes[0]
        // RDAUXA covers a_codes[0..2] (pec_match[0]), RDAUXB covers a_codes[3..5] (pec_match[1]).
        uint8_t reg_idx = a_idx / 3;
        uint16_t sensor_idx = icn * FEB_NUM_TEMP_SENSE_PER_IC + mux * 7 + channel;

        if (IC_Config[ic_idx].aux.pec_match[reg_idx] != 0)
        {
          FEB_ACC.banks[bank].temp_sensor_readings_V[sensor_idx] = NAN;
          FEB_ACC.banks[bank].therm_raw_voltages_mV[sensor_idx] = NAN;
          FEB_ACC.banks[bank].therm_raw_codes[sensor_idx] = 0xFFFF;
          DEBUG_TEMP_PRINT("PEC error: Bank %d IC %d MUX%d ch%d reg%d -> idx=%d (NaN)", bank, icn, mux + 1, channel,
                           reg_idx, sensor_idx);
          continue;
        }

        uint16_t code = IC_Config[ic_idx].aux.a_codes[a_idx];
        float V_mV = convert_voltage(code) * 1000.0f;
        float T_C = FEB_Thermistor_Voltage_To_Temp_C(V_mV);

        FEB_ACC.banks[bank].temp_sensor_readings_V[sensor_idx] = T_C;
        FEB_ACC.banks[bank].therm_raw_codes[sensor_idx] = code;
        FEB_ACC.banks[bank].therm_raw_voltages_mV[sensor_idx] = V_mV;

        DEBUG_TEMP_PRINT("Bank %d IC %d MUX%d ch%d: code=0x%04X V=%.1fmV T=%.1fC -> idx=%d", bank, icn, mux + 1,
                         channel, code, V_mV, T_C, sensor_idx);
      }
    }
  }
}

static void compute_pack_temp_stats(void)
{
  float min_C = FLT_MAX;
  float max_C = -FLT_MAX;
  float sum_C = 0.0f;
  uint16_t count = 0;

  const float t_min_valid = TEMP_VALID_MIN_DC / 10.0f;
  const float t_max_valid = TEMP_VALID_MAX_DC / 10.0f;

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (uint16_t s = 0; s < FEB_NUM_TEMP_SENSORS; s++)
    {
      float T = FEB_ACC.banks[bank].temp_sensor_readings_V[s];
      // Positive-form check rejects NaN (all comparisons with NaN are false).
      if (!(T >= t_min_valid && T <= t_max_valid))
        continue;
      if (T < min_C)
        min_C = T;
      if (T > max_C)
        max_C = T;
      sum_C += T;
      count++;
    }
  }

  if (count > 0)
  {
    FEB_ACC.pack_min_temp = min_C;
    FEB_ACC.pack_max_temp = max_C;
    FEB_ACC.average_pack_temp = sum_C / (float)count;
    DEBUG_TEMP_PRINT("Pack stats: Count=%d Min=%.1fC Max=%.1fC Avg=%.1fC", count, min_C, max_C,
                     FEB_ACC.average_pack_temp);
  }
  else
  {
    // All sensor readings were invalid. Publish NaN so consumers can detect
    // the condition (NaN propagates through comparisons as false), and raise
    // the same diagnostic that validate_temps() uses for chronically-low reads.
    FEB_ACC.pack_min_temp = NAN;
    FEB_ACC.pack_max_temp = NAN;
    FEB_ACC.average_pack_temp = NAN;
    FEB_ADBMS_Update_Error_Type(ERROR_TYPE_LOW_TEMP_READS);
    DEBUG_TEMP_PRINT("Pack stats: no valid readings");
  }
}

static void validate_temps()
{
  DEBUG_TEMP_PRINT("Validating temperatures");
  int16_t tMax = (int16_t)FEB_Config_Get_Cell_Max_Temperature_dC();
  int16_t tMin = (int16_t)FEB_Config_Get_Cell_Min_Temperature_dC();
  DEBUG_TEMP_PRINT("Temperature limits: Min=%.1fC Max=%.1fC", tMin / 10.0f, tMax / 10.0f);
  int totalReads = 0;

  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_ACC.banks[bank].tempRead = 0;
    for (uint16_t sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      float temp = FEB_ACC.banks[bank].temp_sensor_readings_V[sensor] * 10;

      // Check if temperature is within physically reasonable range
      // Valid range: -40C to +85C (typical automotive operating range)
      if (temp >= TEMP_VALID_MIN_DC && temp <= TEMP_VALID_MAX_DC)
      {
        FEB_ACC.banks[bank].tempRead += 1;
      }
      else
      {
        // Invalid reading - outside physically reasonable range
        DEBUG_TEMP_PRINT("Invalid temp reading: Bank %d Sensor %d Temp=%.1fC (outside valid range)", bank, sensor,
                         temp / 10.0f);
        continue;
      }

      if (temp > tMax || temp < (float)tMin)
      {
        DEBUG_TEMP_PRINT("Temperature violation: Bank %d Sensor %d Temp=%.1fC violations=%d", bank, sensor,
                         temp / 10.0f, FEB_ACC.banks[bank].temp_violations[sensor] + 1);
        FEB_ACC.banks[bank].temp_violations[sensor]++;
        if (FEB_ACC.banks[bank].temp_violations[sensor] == FEB_TEMP_ERROR_THRESH)
        {
          printf("[ADBMS] FAULT: Cell temperature out of range - Bank %d Sensor %d: %.1fC (limits: %.1f-%.1fC)\r\n",
                 bank, sensor, temp / 10.0f, tMin / 10.0f, tMax / 10.0f);
          FEB_ADBMS_Update_Error_Type(ERROR_TYPE_TEMP_VIOLATION);
          // FEB_SM_Transition(FEB_SM_ST_FAULT_BMS);
        }
      }
      else
      {
        FEB_ACC.banks[bank].temp_violations[sensor] = 0;
      }
    }
    totalReads += FEB_ACC.banks[bank].tempRead;
    DEBUG_TEMP_PRINT("Bank %d: tempRead=%d", bank, FEB_ACC.banks[bank].tempRead);
  }

  float read_ratio = totalReads / (float)(FEB_NUM_TEMP_SENSORS * FEB_NBANKS);
  DEBUG_TEMP_PRINT("Total reads: %d/%d (%.1f%%)", totalReads, FEB_NUM_TEMP_SENSORS * FEB_NBANKS, read_ratio * 100.0f);
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

bool FEB_ADBMS_Init(void)
{
  printf("[ADBMS] Initializing ADBMS\r\n");
  for (uint8_t bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_ACC.banks[bank].badReadV = 0;
    FEB_ACC.banks[bank].tempRead = 0;
    FEB_ACC.banks[bank].total_voltage_V = 0;
    for (uint8_t cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      FEB_ACC.banks[bank].cells[cell].voltage_V = 0;
      FEB_ACC.banks[bank].cells[cell].voltage_S = 0;
      FEB_ACC.banks[bank].cells[cell].violations = 0;
      FEB_ACC.banks[bank].cells[cell].discharging = 0;
    }
    for (uint8_t sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      FEB_ACC.banks[bank].temp_violations[sensor] = 0;
      FEB_ACC.banks[bank].temp_sensor_readings_V[sensor] = 0;
      // Seed raw thermistor telemetry with PEC-failure sentinels so consumers
      // can tell "not yet scanned" from a genuine 0 reading.
      FEB_ACC.banks[bank].therm_raw_codes[sensor] = 0xFFFF;
      FEB_ACC.banks[bank].therm_raw_voltages_mV[sensor] = NAN;
    }
  }

  // Seed pack-wide temp stats to NaN so FEB_Cell_Balancing_Status() fails closed
  // before the first temperature scan completes. Zero would let the gate
  // (max < soft limit) pass with no telemetry.
  FEB_ACC.pack_max_temp = NAN;
  FEB_ACC.pack_min_temp = NAN;
  FEB_ACC.average_pack_temp = NAN;

  // Initialize ADBMS configuration FIRST (matching SN4 sequence)
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

  // Read the Serial ID AFTER configuration is established
  ADBMS6830B_rdsid(FEB_NUM_IC, IC_Config);
  osDelay(pdMS_TO_TICKS(1));
  printf("[ADBMS] Serial IDs read for %d ICs\r\n", FEB_NUM_IC);

  // Validate serial IDs - check that at least one IC has a valid (non-zero, non-0xFF) serial ID
  uint8_t valid_ic_count = 0;
  for (uint8_t i = 0; i < FEB_NUM_IC; i++)
  {
    printf("[ADBMS] IC%d SID: %02X:%02X:%02X:%02X:%02X:%02X\r\n", i, IC_Config[i].sid[0], IC_Config[i].sid[1],
           IC_Config[i].sid[2], IC_Config[i].sid[3], IC_Config[i].sid[4], IC_Config[i].sid[5]);

    // Check if serial ID is valid (not all zeros and not all 0xFF)
    bool all_zero = true;
    bool all_ff = true;
    for (uint8_t j = 0; j < 6; j++)
    {
      if (IC_Config[i].sid[j] != 0x00)
        all_zero = false;
      if (IC_Config[i].sid[j] != 0xFF)
        all_ff = false;
    }
    if (!all_zero && !all_ff)
    {
      valid_ic_count++;
    }
  }

  if (valid_ic_count != FEB_NUM_IC)
  {
    printf("[ADBMS] ERROR: Only %d/%d ICs have valid serial IDs\r\n", valid_ic_count, FEB_NUM_IC);
    return false;
  }

  return true;
}

void FEB_ADBMS_Voltage_Process()
{
  // Note: Caller must hold ADBMSMutexHandle (acquired in FEB_Task_ADBMS.c)
  DEBUG_VOLTAGE_PRINT("=== Voltage Process Started ===");
  start_adc_cell_voltage_measurements();
  read_cell_voltages();
  store_cell_voltages();
  validate_voltages();
  DEBUG_VOLTAGE_PRINT("=== Voltage Process Completed ===");
}

void FEB_ADBMS_Temperature_Process()
{
  // Note: Caller must hold ADBMSMutexHandle (acquired in FEB_Task_ADBMS.c)
  DEBUG_TEMP_PRINT("=== Temperature Process Started ===");
  gpio_bits[9] ^= 0b1;
  DEBUG_TEMP_PRINT("Toggled gpio_bits[9] to %d", gpio_bits[9]);

  // 6 MUXes on GPIO1..GPIO6, 7 channels per MUX selected via GPIO7..GPIO9 (SEL1..SEL3).
  for (uint8_t channel = 0; channel < 7; channel++)
  {
    DEBUG_TEMP_PRINT("--- Processing channel %d ---", channel);
    configure_gpio_bits(channel);
    start_aux_voltage_measurements();
    read_aux_voltages();
    store_cell_temps(channel);
    DEBUG_TEMP_PRINT("--- Channel %d complete ---", channel);
  }
  compute_pack_temp_stats();
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
  if (bank >= FEB_NBANKS || cell >= FEB_NUM_CELLS_PER_BANK)
  {
    return -1.0f;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float voltage = FEB_ACC.banks[bank].cells[cell].voltage_V;
  osMutexRelease(ADBMSMutexHandle);
  return voltage;
}

float FEB_ADBMS_GET_Cell_Voltage_S(uint8_t bank, uint16_t cell)
{
  if (bank >= FEB_NBANKS || cell >= FEB_NUM_CELLS_PER_BANK)
  {
    return -1.0f;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float voltage = FEB_ACC.banks[bank].cells[cell].voltage_S;
  osMutexRelease(ADBMSMutexHandle);
  return voltage;
}

uint8_t FEB_ADBMS_GET_Cell_Violations(uint8_t bank, uint16_t cell)
{
  if (bank >= FEB_NBANKS || cell >= FEB_NUM_CELLS_PER_BANK)
  {
    return 0;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  uint8_t violations = FEB_ACC.banks[bank].cells[cell].violations;
  osMutexRelease(ADBMSMutexHandle);
  return violations;
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

uint16_t FEB_ADBMS_GET_Therm_Raw_Code(uint8_t bank, uint16_t sensor)
{
  if (bank >= FEB_NBANKS || sensor >= FEB_NUM_TEMP_SENSORS)
  {
    return 0xFFFF;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  uint16_t code = FEB_ACC.banks[bank].therm_raw_codes[sensor];
  osMutexRelease(ADBMSMutexHandle);
  return code;
}

float FEB_ADBMS_GET_Therm_Raw_mV(uint8_t bank, uint16_t sensor)
{
  if (bank >= FEB_NBANKS || sensor >= FEB_NUM_TEMP_SENSORS)
  {
    return NAN;
  }

  osMutexAcquire(ADBMSMutexHandle, osWaitForever);
  float mV = FEB_ACC.banks[bank].therm_raw_voltages_mV[sensor];
  osMutexRelease(ADBMSMutexHandle);
  return mV;
}

// ********************************** Balancing **********************************

void FEB_Cell_Balance_Start()
{
  LOG_I(TAG_BALANCE, "Starting cell balancing");
  FEB_cs_high();
  ADBMS6830B_init_cfg(FEB_NUM_IC, IC_Config);
  ADBMS6830B_wrALL(FEB_NUM_IC, IC_Config);
  FEB_Cell_Balance_Process();
}

void FEB_Cell_Balance_Process()
{
  // Thermal safety gate. Mirrors FEB_Cell_Balancing_Status() so direct callers
  // (e.g. FEB_Cell_Balance_Start / FEB_Task_ADBMS) cannot bypass the check.
  // NaN fails the comparison → stops balancing when telemetry is unavailable.
  const float gate_max_temp_dC = FEB_ADBMS_GET_ACC_MAX_Temp() * 10.0f;
  if (!(gate_max_temp_dC < FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC))
  {
    LOG_W(TAG_BALANCE, "Temp limit: pack max=%.1fC, skipping balance cycle", gate_max_temp_dC / 10.0f);
    return;
  }

  determineMinV();

#if !FEB_CELL_BALANCE_ALL_AT_ONCE
  if (balancing_cycle == 3)
  {
    balancing_mask = ~balancing_mask;
    LOG_D(TAG_BALANCE, "Mask flipped to 0x%04X", balancing_mask);
    balancing_cycle = 0;
  }
  balancing_cycle++;
#endif
  // Use the pack-wide minimum written by store_cell_voltages().
  // FEB_ACC.min_voltage_V is never written → reading it yields 0 and every
  // cell clears the slippage threshold.
  float min_cell_voltage = FEB_ACC.pack_min_voltage_V;
  LOG_D(TAG_BALANCE, "Cycle %d: min=%.3fV max=%.3fV mask=0x%04X", balancing_cycle, min_cell_voltage,
        FEB_ACC.pack_max_voltage_V, balancing_mask);

  uint8_t total_balancing = 0;
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
#if FEB_CELL_BALANCE_ALL_AT_ONCE
        FEB_ACC.banks[icn / FEB_NUM_ICPBANK].cells[cell + FEB_NUM_CELLS_PER_IC * (icn % FEB_NUM_ICPBANK)].discharging =
            0b1; // All qualifying cells discharge
#else
        FEB_ACC.banks[icn / FEB_NUM_ICPBANK].cells[cell + FEB_NUM_CELLS_PER_IC * (icn % FEB_NUM_ICPBANK)].discharging =
            0b1 & ((balancing_mask & bits) >> cell); // Only cells allowed by mask
#endif
      }
      else
      {
        FEB_ACC.banks[icn / FEB_NUM_ICPBANK].cells[cell + FEB_NUM_CELLS_PER_IC * (icn % FEB_NUM_ICPBANK)].discharging =
            0b0;
      }
    }
#if FEB_CELL_BALANCE_ALL_AT_ONCE
    uint16_t applied = bits; // Balance all qualifying cells at once
#else
    uint16_t applied = bits & balancing_mask; // Alternate odd/even cells
#endif
    if (applied != 0)
    {
      LOG_D(TAG_BALANCE, "IC%d: discharge=0x%04X (raw=0x%04X)", icn, applied, bits);
      total_balancing += __builtin_popcount(applied);
    }
    ADBMS6830B_set_cfgr(icn, IC_Config, refon, cth_bits, gpio_bits, applied, dcto_bits, uv, ov);
  }

  if (total_balancing > 0)
  {
    LOG_I(TAG_BALANCE, "Balancing %d cells, delta=%.0fmV", total_balancing,
          (FEB_ACC.pack_max_voltage_V - min_cell_voltage) * 1000);
  }

  ADBMS6830B_wrcfgb(FEB_NUM_IC, IC_Config);
}

bool FEB_Cell_Balancing_Status(void)
{
  // The per-cell loop used to index temp_sensor_readings by cell index, which
  // only covered FEB_NUM_CELLS_PER_BANK (14) of the FEB_NUM_TEMP_SENSORS (42)
  // sensors. Use the pack-wide max computed by compute_pack_temp_stats() so
  // every MUX output is considered. NaN (all-invalid readings) fails the
  // comparison and stops balancing — the safe default when we have no thermal
  // telemetry.
  const float max_temp_dC = FEB_ADBMS_GET_ACC_MAX_Temp() * 10.0f;
  if (!(max_temp_dC < FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC))
  {
    LOG_W(TAG_BALANCE, "Temp limit: pack max=%.1fC, stopping", max_temp_dC / 10.0f);
    return false;
  }

  float min_v = FLT_MAX;
  float max_v = FLT_MIN;

  for (size_t i = 0; i < FEB_NBANKS; ++i)
  {
    for (size_t j = 0; j < FEB_NUM_CELLS_PER_BANK; ++j)
    {
      const float voltage = FEB_ADBMS_GET_Cell_Voltage(i, j) * 1000.0f;

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
    LOG_W(TAG_BALANCE, "Invalid voltage readings, cannot balance");
    return false;
  }

  const float delta_v = max_v - min_v;
  LOG_D(TAG_BALANCE, "Status: delta=%.0fmV (threshold=%.0fmV)", delta_v, FEB_MIN_SLIPPAGE_V * 1000.0f);

  // delta_v is in millivolts, FEB_MIN_SLIPPAGE_V is in volts (0.03V = 30mV)
  if (delta_v >= FEB_MIN_SLIPPAGE_V * 1000.0f)
  {
    return true;
  }

  LOG_D(TAG_BALANCE, "Cells balanced within threshold");
  return false;
}

void FEB_Stop_Balance()
{
  LOG_D(TAG_BALANCE, "Stopping all cell discharge");

  // Reset balancing mask and cycle
  balancing_mask = 0x0000;
  balancing_cycle = 0;

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
