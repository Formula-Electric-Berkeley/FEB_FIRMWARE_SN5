#ifndef INC_FEB_CMDCODES_H_
#define INC_FEB_CMDCODES_H_

// ********************************** ADBMS6830B Command Codes *******************
// Based on ADBMS6830 datasheet - fully verbose naming scheme

// ****************** Wake-up and Sleep Commands ******************
#define CMD_WAKEUP_FROM_IDLE            0x0000
#define CMD_WAKEUP_FROM_SLEEP           0x0000

// ****************** ADC Conversion Start Commands ******************
#define CMD_START_CELL_VOLTAGE_ADC_NORMAL       0x0370
#define CMD_START_CELL_VOLTAGE_ADC_FILTERED     0x0360
#define CMD_START_S_VOLTAGE_ADC                 0x01E0  // S-voltage ADC conversion
#define CMD_START_AUX_ADC                       0x0570  // GPIO/Auxiliary inputs
#define CMD_START_STATUS_ADC                    0x0570  // Status group measurements
#define CMD_POLL_ADC_STATUS                     0x0718  // Check if ADC conversion complete

// ****************** Clear Register Commands ******************
#define CMD_CLEAR_CELL_VOLTAGE_REGISTERS        0x0711
#define CMD_CLEAR_AUX_REGISTERS                 0x0712
#define CMD_CLEAR_STATUS_REGISTERS              0x0713
#define CMD_CLEAR_ALL_ADC_FLAGS                 0x0717

// ****************** Configuration Register Commands ******************
#define CMD_READ_CONFIG_REG_GROUP_A             0x0002
#define CMD_READ_CONFIG_REG_GROUP_B             0x0026
#define CMD_WRITE_CONFIG_REG_GROUP_A            0x0001
#define CMD_WRITE_CONFIG_REG_GROUP_B            0x0024

// ****************** Cell Voltage Register Read Commands ******************
#define CMD_READ_CELL_VOLTAGE_REG_A             0x0004  // Cells 1-3
#define CMD_READ_CELL_VOLTAGE_REG_B             0x0006  // Cells 4-6
#define CMD_READ_CELL_VOLTAGE_REG_C             0x0008  // Cells 7-9
#define CMD_READ_CELL_VOLTAGE_REG_D             0x000A  // Cells 10-12
#define CMD_READ_CELL_VOLTAGE_REG_E             0x0009  // Cells 13-15
#define CMD_READ_CELL_VOLTAGE_REG_F             0x000B  // Cells 16-18
#define CMD_READ_CELL_VOLTAGE_ALL               0x0010  // Read all cell voltages

// ****************** S Voltage Register Read Commands ******************
#define CMD_READ_S_VOLTAGE_REG_A                0x0003  // S1-S3
#define CMD_READ_S_VOLTAGE_REG_B                0x0007  // S4-S6
#define CMD_READ_S_VOLTAGE_REG_C                0x000D  // S7-S9
#define CMD_READ_S_VOLTAGE_REG_D                0x000F  // S10-S12
#define CMD_READ_S_VOLTAGE_REG_E                0x000C  // S13-S15
#define CMD_READ_S_VOLTAGE_REG_F                0x000E  // S16-S18
#define CMD_READ_S_VOLTAGE_ALL                  0x0011  // Read all S voltages

// ****************** Auxiliary Register Read Commands ******************
#define CMD_READ_AUX_REG_GROUP_A                0x0016  // GPIO1-3, VREF2
#define CMD_READ_AUX_REG_GROUP_B                0x0017  // GPIO4-6, VD
#define CMD_READ_AUX_REG_GROUP_C                0x0018  // GPIO7-9, VA
#define CMD_READ_AUX_REG_GROUP_D                0x0019  // GPIO10, ITEMP, etc.

// ****************** Status Register Read Commands ******************
#define CMD_READ_STATUS_REG_GROUP_A             0x0030
#define CMD_READ_STATUS_REG_GROUP_B             0x0031

// ****************** PWM Register Commands ******************
#define CMD_WRITE_PWM_REG_GROUP_A               0x0020
#define CMD_WRITE_PWM_REG_GROUP_B               0x0021
#define CMD_READ_PWM_REG_GROUP_A                0x0022
#define CMD_READ_PWM_REG_GROUP_B                0x0023

// ****************** ADC Mode Bit Flags ******************
#define ADCV                    0x0370  // Cell voltage ADC base command
#define AD_CONT                 0x0080  // Continuous mode bit
#define AD_RD                   0x0010  // Redundancy bit
#define AD_DCP                  0x0010  // Discharge permitted bit
#define OWVR                    0x0003  // Open wire voltage reading mode

// ****************** Serial ID Register Commands ******************
#define CMD_READ_SERIAL_ID                        0x002C
#define RDSID                                     CMD_READ_SERIAL_ID

// ****************** Legacy Compatibility Aliases ******************
// Short names for backward compatibility with existing code
#define WAKEUP_IDLE             CMD_WAKEUP_FROM_IDLE
#define WAKEUP_SLEEP            CMD_WAKEUP_FROM_SLEEP
#define ADSV                    CMD_START_S_VOLTAGE_ADC
#define RDCVALL                 CMD_READ_CELL_VOLTAGE_ALL
#define RDSALL                  CMD_READ_S_VOLTAGE_ALL
#define ADCV_NORMAL             CMD_START_CELL_VOLTAGE_ADC_NORMAL
#define ADCV_FILTERED           CMD_START_CELL_VOLTAGE_ADC_FILTERED
#define ADAX                    CMD_START_AUX_ADC
#define ADSTAT                  CMD_START_STATUS_ADC
#define PLADC                   CMD_POLL_ADC_STATUS
#define CLRCELL                 CMD_CLEAR_CELL_VOLTAGE_REGISTERS
#define CLRAUX                  CMD_CLEAR_AUX_REGISTERS
#define CLRSTAT                 CMD_CLEAR_STATUS_REGISTERS
#define CLRFLAG                 CMD_CLEAR_ALL_ADC_FLAGS
#define RDCFGA                  CMD_READ_CONFIG_REG_GROUP_A
#define RDCFGB                  CMD_READ_CONFIG_REG_GROUP_B
#define WRCFGA                  CMD_WRITE_CONFIG_REG_GROUP_A
#define WRCFGB                  CMD_WRITE_CONFIG_REG_GROUP_B
#define RDCVA                   CMD_READ_CELL_VOLTAGE_REG_A
#define RDCVB                   CMD_READ_CELL_VOLTAGE_REG_B
#define RDCVC                   CMD_READ_CELL_VOLTAGE_REG_C
#define RDCVD                   CMD_READ_CELL_VOLTAGE_REG_D
#define RDCVE                   CMD_READ_CELL_VOLTAGE_REG_E
#define RDCVF                   CMD_READ_CELL_VOLTAGE_REG_F
#define RDSVA                   CMD_READ_S_VOLTAGE_REG_A
#define RDSVB                   CMD_READ_S_VOLTAGE_REG_B
#define RDSVC                   CMD_READ_S_VOLTAGE_REG_C
#define RDSVD                   CMD_READ_S_VOLTAGE_REG_D
#define RDSVE                   CMD_READ_S_VOLTAGE_REG_E
#define RDSVF                   CMD_READ_S_VOLTAGE_REG_F
#define RDAUXA                  CMD_READ_AUX_REG_GROUP_A
#define RDAUXB                  CMD_READ_AUX_REG_GROUP_B
#define RDAUXC                  CMD_READ_AUX_REG_GROUP_C
#define RDAUXD                  CMD_READ_AUX_REG_GROUP_D
#define RDSTATA                 CMD_READ_STATUS_REG_GROUP_A
#define RDSTATB                 CMD_READ_STATUS_REG_GROUP_B
#define WRPWMA                  CMD_WRITE_PWM_REG_GROUP_A
#define WRPWMB                  CMD_WRITE_PWM_REG_GROUP_B
#define RDPWMA                  CMD_READ_PWM_REG_GROUP_A
#define RDPWMB                  CMD_READ_PWM_REG_GROUP_B

#endif /* INC_FEB_CMDCODES_H_ */
