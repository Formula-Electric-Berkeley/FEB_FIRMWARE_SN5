#ifndef INC_FEB_CONST_H_
#define INC_FEB_CONST_H_

// ********************************** ADBMS Configuration Constants **************

// Number of ADBMS6830 ICs per bank
#define FEB_NUM_ICPBANK 1

// Number of banks in the system
#define FEB_NBANKS 10

// Total number of ICs in the daisy chain
#define FEB_NUM_IC (FEB_NUM_ICPBANK * FEB_NBANKS)

// Number of cells per IC
#define FEB_NUM_CELLS_PER_IC 14

// Total number of cells per bank
#define FEB_NUM_CELLS_PER_BANK (FEB_NUM_CELLS_PER_IC * FEB_NUM_ICPBANK)

// Number of temperature sensors per IC: 6 MUXes × 7 channels = 42
#define FEB_NUM_TEMP_SENSE_PER_IC 42

// Total number of temperature sensors per bank
#define FEB_NUM_TEMP_SENSORS (FEB_NUM_TEMP_SENSE_PER_IC * FEB_NUM_ICPBANK)

// ********************************** ADBMS6830B ADC Conversion Constants ********
// From ADBMS6830B datasheet - Cell voltage measurement

#define ADBMS_ADC_LSB_UV 150      // ADC resolution: 150 µV/LSB
#define ADBMS_ADC_LSB_V 0.000150f // ADC resolution in volts
#define ADBMS_ADC_OFFSET_V 1.5f   // ADC bipolar offset voltage

// ********************************** ADBMS6830B Open Wire Detection **************
// Open Wire (OW) detection configuration for cell voltage measurements
// OW detection applies a test current to detect disconnected sense wires
// Options: 0x00=OFF, 0x01=EVEN_CH, 0x02=ODD_CH, 0x03=ALL_CH
#define ADBMS_OW_DETECTION_MODE 0x00 // OW detection disabled

// ********************************** Thermistor Beta Parameter Constants *********
// NTC Thermistor conversion using Beta parameter equation

#define THERM_R_REF_OHMS 10000.0f    // Reference resistance at 25°C (10k NTC)
#define THERM_T_REF_K 298.15f        // Reference temperature in Kelvin (25°C)
#define THERM_BETA 3428.0f           // Beta coefficient from datasheet
#define THERM_R_PULLUP_OHMS 10000.0f // Pull-up resistor value (R1)
#define THERM_VS_MV 5000.0f          // Supply voltage in mV (5V)

// Pre-computed values for optimization
#define THERM_INV_T_REF (1.0f / THERM_T_REF_K) // 1/T_ref
#define THERM_INV_BETA (1.0f / THERM_BETA)     // 1/B
#define THERM_KELVIN_OFFSET 273.15f            // K to C conversion

// Voltage bounds for valid thermistor readings (in mV)
#define THERM_MIN_VOLTAGE_MV 100.0f  // Below this: open circuit / disconnected
#define THERM_MAX_VOLTAGE_MV 4900.0f // Above this: short circuit / sensor fault

// ********************************** Error Type Codes ****************************

#define ERROR_TYPE_TEMP_VIOLATION 0x10
#define ERROR_TYPE_LOW_TEMP_READS 0x20
#define ERROR_TYPE_VOLTAGE_VIOLATION 0x30
#define ERROR_TYPE_INIT_FAILURE 0x40

// ********************************** Temperature Validation Range ****************
// Valid operating range for temperature sensors (in deci-Celsius)

#define TEMP_VALID_MIN_DC (-400) // -40.0°C minimum valid reading
#define TEMP_VALID_MAX_DC 850    // 85.0°C maximum valid reading

// ********************************** Voltage and Temperature Limits *************

// Cell voltage limits (in millivolts)
#define FEB_CELL_MAX_VOLTAGE_MV 4200      // Maximum safe cell voltage (Li-ion typical)
#define FEB_CELL_MIN_VOLTAGE_MV 2800      // Minimum safe cell voltage (Li-ion typical)
#define FEB_CELL_BALANCE_THRESHOLD_MV 10  // Start balancing if cell is >10mV above minimum
#define FEB_CELL_BALANCE_INTERVAL_MS 1000 // Balancing cycle interval (1 second)
#define FEB_CELL_BALANCE_ALL_AT_ONCE 1    // 1=balance all qualifying cells, 0=alternate odd/even

// Cell temperature limits (in deci-Celsius, 1 dC = 0.1°C)
#define FEB_CELL_MAX_TEMP_DC 600             // 60.0°C maximum cell temperature
#define FEB_CELL_MIN_TEMP_DC -200            // -20.0°C minimum cell temperature
#define FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC 400 // 40.0°C soft limit (stop charging; also gates balancing)

// Error thresholds (consecutive violations before triggering fault)
#define FEB_VOLTAGE_ERROR_THRESH 3 // Trigger fault after 3 consecutive voltage violations
// Per-sensor consecutive out-of-range scans before latching a temp fault. This
// is checked PER SENSOR against the uint8_t temp_violations[] counter, so it
// must stay small: ANY single cell over/under limit must fault — we never wait
// for a fraction of the pack to overheat. (A pack-wide 0.8*N aggregate here was
// both wrong semantically and unreachable: 0.8*42*10=336 > uint8_t max.)
#define FEB_TEMP_ERROR_THRESH 3 // Trigger fault after 3 consecutive temp violations

// Temperature scan cadence and FSAE fault-timing budget.
// The temp scan (FEB_ADBMS_Temperature_Process) is gated to this period in
// FEB_Task_ADBMS.c. FSAE requires the AMS to open the shutdown circuit within
// ~1 s of an out-of-range cell temperature (and within 1 s of a temp sense-wire
// disconnect). The per-sensor debounce above latches in at most
// (FEB_TEMP_ERROR_THRESH + 1) scans; the static assertion proves that stays
// under budget, so it cannot silently regress if the period or threshold changes.
#define FEB_TEMP_SCAN_PERIOD_MS 100  // temperature scan cadence (10 Hz)
#define FEB_TEMP_FAULT_BUDGET_MS 900 // margin under the 1 s FSAE temperature window
_Static_assert((FEB_TEMP_ERROR_THRESH + 1) * FEB_TEMP_SCAN_PERIOD_MS <= FEB_TEMP_FAULT_BUDGET_MS,
               "Over/under-temp fault must latch within the FSAE temperature window");

// Temperature-telemetry-loss fail-safe (FSAE: a disconnected temperature sense
// wire must open the shutdown circuit within 1 s). If fewer than
// FEB_TEMP_MIN_VALID_FRACTION of the POPULATED sensors read in-range for longer
// than FEB_TEMP_TELEMETRY_TIMEOUT_MS, validate_temps() latches a sensor fault.
// NOTE: *_POPULATED_PER_BANK is a HARDWARE FACT — set it to the number of
// thermistors actually wired per bank. The array holds FEB_NUM_TEMP_SENSORS=42,
// but index 39 (MUX6[4]) is unconnected by design, so 41 are expected. A wrong
// value here either false-trips or under-protects.
#define FEB_TEMP_SENSORS_POPULATED_PER_BANK 41 // VERIFY against the harness
#define FEB_TEMP_MIN_VALID_FRACTION 0.50f      // TUNE: fault below this fraction valid
#define FEB_TEMP_TELEMETRY_TIMEOUT_MS 800      // sustained low reads this long -> fault (< 1 s)

// Bad-connection (outlier) rejection. Cells are thermally coupled and have mass,
// so a thermistor reading more than FEB_TEMP_OUTLIER_MARGIN_DC from its bank's
// median is implausible as a real cell temperature -> treated as a faulty sense
// connection, NOT a cell over/under-temp, and excluded from valid coverage. A
// bank needs at least FEB_TEMP_MIN_SENSORS_FOR_MEDIAN valid readings for its
// median to be trustworthy; below that, outlier-based over/under-temp faulting is
// skipped for the bank (the telemetry-loss fault covers collapsed coverage).
#define FEB_TEMP_OUTLIER_MARGIN_DC 250    // TUNE: 25.0C max deviation from bank median
#define FEB_TEMP_MIN_SENSORS_FOR_MEDIAN 5 // min valid readings to trust the bank median

// ********************************** Charging Limits (SN4-derived) **************
// Used by FEB_CAN_Charging_Status(). Soft limit -> stop charging and return to
// BATTERY_FREE; hard limit -> FAULT_CHARGING. SN5 values (do not copy SN4's
// pack-specific thermal numbers verbatim).
#define FEB_CONFIG_CELL_HARD_MAX_VOLTAGE_mV 4200 // Hard cell over-voltage -> FAULT_CHARGING
#define FEB_CONFIG_CELL_SOFT_MAX_VOLTAGE_mV 4180 // TUNE: soft target -> stop charge
#define FEB_CONFIG_CELL_HARD_MAX_TEMP_dC 450     // 45.0C hard charge over-temp -> FAULT_CHARGING
// Charge thermal limits are stricter than the 60.0C discharge ceiling
// (FEB_CELL_MAX_TEMP_DC): Li-ion charge accept tops out ~45C. Soft limit
// (FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC, 40.0C above) stops charge with margin
// before this hard fault.

// Pack hard-max voltage in VOLTS (compared against pack/IVT voltage in volts).
#define FEB_CONFIG_PACK_HARD_MAX_VOLTAGE_V                                                                             \
  ((FEB_NBANKS * FEB_NUM_CELLS_PER_BANK * FEB_CONFIG_CELL_HARD_MAX_VOLTAGE_mV) / 1000.0f)

// ********************************** Charger CAN (SN4-derived) ******************
// Charger: Elcon/HK HK-J-H650-12 GEN3 (170-650 VDC). Keep the target voltage
// inside that window. Frames pack/unpack via the generated CAN lib (elcon.dbc).
// Charger command (BMS -> charger) targets, in charger units (deci-amps / deci-volts).
#define FEB_CHARGE_CURRENT_dA 40 // TUNE: 4.0 A nominal charge current
#define FEB_TRICKLE_CHARGE_CURRENT_dA (FEB_CHARGE_CURRENT_dA / 2)
#define FEB_TRICKLE_CHARGE_INTERVAL_MS 5000 // toggle interval near full charge
#define FEB_CHARGE_TARGET_VOLTAGE_dV ((uint16_t)(FEB_CONFIG_PACK_HARD_MAX_VOLTAGE_V * 10.0f * 0.99f))        // TUNE
#define FEB_TRICKLE_CHARGE_START_VOLTAGE_dV ((uint16_t)(FEB_CONFIG_PACK_HARD_MAX_VOLTAGE_V * 10.0f * 0.98f)) // TUNE
#define FEB_CHARGER_RX_TIMEOUT_MS 1000 // charger considered absent after this

// ********************************** Fault Evaluation Thresholds ****************
#define FEB_DISCHARGE_OVERCURRENT_A 350.0f    // TUNE: drive-side limit (fuse/AIR rating)
#define FEB_CHARGE_OVERCURRENT_A 60.0f        // TUNE: charge-side limit (charger rating)
#define FEB_OVERCURRENT_CONFIRM_MS 50         // |I| must exceed limit this long before fault
#define FEB_IVT_FAULT_TIMEOUT_MS 1000         // IVT CAN staleness -> sensor timeout fault
#define FEB_ADBMS_DATA_TIMEOUT_MS 1000        // cell-monitor staleness -> sensor timeout fault
#define FEB_ADBMS_BOOT_GRACE_MS 3000          // no first scan by now -> cell monitor never initialized -> fault
#define FEB_IMD_FAULT_CONFIRM_MS 100          // debounce IMD-open before faulting
#define FEB_CONTACTOR_FEEDBACK_TIMEOUT_MS 200 // cmd vs sense mismatch -> weld/stuck fault

// ********************************** isoSPI Communication Mode ******************

// isoSPI Mode Selection - Choose ONE of the following:
#define ISOSPI_MODE_REDUNDANT 0 // Dual SPI with automatic PEC-error failover
#define ISOSPI_MODE_SPI1_ONLY 1 // Use only SPI1 (primary channel)
#define ISOSPI_MODE_SPI2_ONLY 2 // Use only SPI2 (backup channel)

// *** SELECT MODE HERE ***
#define ISOSPI_MODE ISOSPI_MODE_SPI1_ONLY

// Redundant Mode Configuration (only used when ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
#define ISOSPI_FAILOVER_PEC_THRESHOLD 5 // Number of PEC errors before failover
#define ISOSPI_FAILOVER_LOCKOUT_MS 1000 // Milliseconds to wait before allowing failover again
#define ISOSPI_PRIMARY_CHANNEL 1        // Primary channel: 1=SPI1, 2=SPI2

// ********************************** IVT-S Sensor Configuration *****************

// IVT-S voltage channel carrying the HV pack sense line (which IVT input the
// pack is physically wired to). Selects the channel FEB_CAN_IVT_GetVoltage()
// reports — used by precharge and the BMS|ivt console. All channels are always
// decoded via the DBC unpack; this only picks the reported one.
//   1 -> U1 (frame 0x522)   2 -> U2 (0x523)   3 -> U3 (0x524)
#ifndef FEB_IVT_PACK_VOLTAGE_CHANNEL
#define FEB_IVT_PACK_VOLTAGE_CHANNEL 2
#endif

// ********************************** Temperature Enforcement Override (BENCH ONLY)

// Set to 1 to disable ALL cell-temperature enforcement: the over/under-temp
// fault latch (validate_temps), the charging temp soft-stop/hard-fault block
// (FEB_CAN_Charging_Status), and the balance thermal gates. For bench bring-up
// of ADBMS modules with unpopulated thermistors: floating inputs can latch
// spurious temp faults, and all-NaN readings block balancing entirely.
// Readings and diagnostics remain visible; only enforcement is suppressed.
// Logs a warning at boot while enabled. NEVER enable with a real pack.
#ifndef FEB_BMS_DISABLE_TEMP_CHECKS
#define FEB_BMS_DISABLE_TEMP_CHECKS 0
#endif

// ********************************** Cell-Voltage Enforcement Override (BENCH ONLY)

// Bench bring-up of modules with desoldered cells: floating taps read garbage
// on both ADBMS6830B voltage ADCs and latch spurious under/over-voltage faults.
// Each macro removes one ADC's readings from the voltage-fault judgment in
// validate_voltages():
//   PRIMARY   -> C-ADC readings (cells[].voltage_V). Also compiles out the
//                charging pack/cell over-voltage limits in
//                FEB_CAN_Charging_Status() (computed from primary readings).
//   SECONDARY -> S-ADC readings (cells[].voltage_S, the redundancy confirmation).
// With one side disabled, the other alone judges violations. With BOTH set to
// 1, voltage faults never latch — would-be violations print a one-shot warning
// instead (set both for a module with desoldered cells). Readings stay visible.
// Logs warnings at boot while enabled. NEVER enable with a real pack.
#ifndef FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
#define FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS 0
#endif
#ifndef FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
#define FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS 0
#endif

// ********************************** ADBMS Master Override (BENCH ONLY)

// Set to 1 to disable EVERY ADBMS-derived check at once, for testing the state
// machine in isolation with no functional cell monitor attached. Implies all
// three enforcement overrides above (temp + primary/secondary voltage) and
// additionally bypasses the cell-monitor data-staleness fault in
// FEB_SM evaluate_faults() — the one check the per-domain macros don't cover.
// Also forces the reported pack total voltage to FEB_BMS_BENCH_PACK_VOLTAGE_V
// so precharge can complete against a low-voltage bench supply. Per-cell
// readings and diagnostics remain visible; only enforcement is suppressed.
// Logs warnings at boot while enabled. NEVER enable with a real pack.
#ifndef FEB_BMS_DISABLE_ADBMS_CHECKS
#define FEB_BMS_DISABLE_ADBMS_CHECKS 0
#endif

#if FEB_BMS_DISABLE_ADBMS_CHECKS
#undef FEB_BMS_DISABLE_TEMP_CHECKS
#define FEB_BMS_DISABLE_TEMP_CHECKS 1
#undef FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
#define FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS 1
#undef FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
#define FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS 1
#endif

// With the master override enabled, both pack-voltage getters return this
// value (volts) instead of the (absent) cell-monitor sum, so the precharge
// completion gate (IVT >= 90% of pack) can pass with a bench supply: at the
// default 60 V the IVT must read >= 54 V. Has no effect when the master
// override is 0.
#ifndef FEB_BMS_BENCH_PACK_VOLTAGE_V
#define FEB_BMS_BENCH_PACK_VOLTAGE_V 60.0f
#endif

// ********************************** Accumulator Structure **********************

typedef struct
{
  float voltage_V; // C-code voltage measurement
  float voltage_S; // S-code voltage measurement (redundant)
  float temperature_C;
  uint8_t violations;  // Consecutive violation counter for this cell
  uint8_t discharging; // Cell is being discharged for balancing
} cell_data_t;

typedef struct
{
  cell_data_t cells[FEB_NUM_CELLS_PER_BANK];
  float total_voltage_V;
  float min_voltage_V;
  float max_voltage_V;
  float avg_temp_C;
  float min_temp_C;
  float max_temp_C;
  uint8_t voltRead;                                   // Voltage reading valid flag
  uint8_t tempRead;                                   // Temperature reading valid flag
  uint8_t badReadV;                                   // Bad voltage read counter
  float temp_sensor_readings_V[FEB_NUM_TEMP_SENSORS]; // Temperature sensor readings
  uint8_t temp_violations[FEB_NUM_TEMP_SENSORS];      // Per-sensor violation counters
  uint16_t therm_raw_codes[FEB_NUM_TEMP_SENSORS];     // Raw ADC codes (0xFFFF = PEC failure)
  float therm_raw_voltages_mV[FEB_NUM_TEMP_SENSORS];  // Converted mV (NaN = PEC failure)
} bank_data_t;

typedef struct
{
  bank_data_t banks[FEB_NBANKS];
  float total_voltage_V;
  float min_voltage_V;
  float max_voltage_V;
  float pack_min_voltage_V; // Minimum cell voltage across entire pack
  float pack_max_voltage_V; // Maximum cell voltage across entire pack
  float avg_temp_C;
  float pack_min_temp;     // Minimum temperature across entire pack
  float pack_max_temp;     // Maximum temperature across entire pack
  float average_pack_temp; // Average temperature across entire pack
  uint8_t error_type;
} accumulator_t;

#endif /* INC_FEB_CONST_H_ */
