/**
 * @file ADBMS6830B_Cmd.c
 * @brief ADBMS6830B Low-Level Command Implementation
 * @author Formula Electric @ Berkeley
 *
 * @details
 * Implements the low-level command interface for the ADBMS6830B battery
 * monitor IC. Each function corresponds to a command from the ADBMS6830B
 * datasheet (Table 50).
 *
 * This module wraps the legacy FEB_ADBMS6830B_Driver functions with a
 * cleaner, more type-safe API while maintaining backward compatibility.
 *
 * @par Thread Safety
 * Functions in this module are NOT thread-safe. Callers must acquire
 * ADBMSMutexHandle before calling any function in this module.
 *
 * @see ADBMS6830B Datasheet Rev. A
 */

#include "ADBMS6830B_Cmd.h"
#include "ADBMS6830B_Commands.h"
#include "FEB_AD68xx_Interface.h"
#include "FEB_HW.h"
#include "FEB_Const.h"
#include <string.h>

/*============================================================================
 * Private Helper Functions
 *============================================================================*/

/**
 * @brief Convert voltage to UV/OV threshold code
 */
static uint16_t voltage_to_threshold_code(float voltage_mV)
{
    /* Threshold = (Code + 1) * 16 * 150uV = (Code + 1) * 2.4mV */
    /* Code = (Threshold / 2.4) - 1 */
    return (uint16_t)(voltage_mV / 2.4f) - 1;
}

/*============================================================================
 * Initialization Functions
 *============================================================================*/

void ADBMS_InitDefaults(uint8_t total_ic, ADBMS_IC_t ic[])
{
    for (uint8_t i = 0; i < total_ic; i++)
    {
        /* Clear all data */
        memset(&ic[i], 0, sizeof(ADBMS_IC_t));

        /* Configuration A defaults */
        ic[i].cfgA.bits.REFON = 1;        /* Reference always on */
        ic[i].cfgA.bits.CTH = 0x07;       /* All comparison thresholds enabled */
        ic[i].cfgA.bits.GPO_1_8 = 0xFF;   /* GPIO 1-8 pull-downs enabled */
        ic[i].cfgA.bits.GPO_9_10 = 0x03;  /* GPIO 9-10 pull-downs enabled */

        /* Configuration B defaults */
        uint16_t vuv_code = voltage_to_threshold_code(2000.0f);  /* 2.0V UV */
        uint16_t vov_code = voltage_to_threshold_code(5000.0f);  /* 5.0V OV */

        ic[i].cfgB.bits.VUV_LO = vuv_code & 0xFF;
        ic[i].cfgB.bits.VUV_HI = (vuv_code >> 8) & 0x0F;
        ic[i].cfgB.bits.VOV_LO = vov_code & 0x0F;
        ic[i].cfgB.bits.VOV_HI = (vov_code >> 4) & 0xFF;
        ic[i].cfgB.bits.DCTO = 0x0F;      /* Discharge timeout enabled */
        ic[i].cfgB.bits.DCC_LO = 0x00;    /* No discharge */
        ic[i].cfgB.bits.DCC_HI = 0x00;
    }
}

int ADBMS_WriteConfig(uint8_t total_ic, const ADBMS_IC_t ic[])
{
    uint8_t write_buffer[total_ic * 6];

    /* Write CFGA */
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(&write_buffer[i * 6], ic[i].cfgA.raw, 6);
    }
    transmitCMDW(WRCFGA, write_buffer);

    /* Write CFGB */
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(&write_buffer[i * 6], ic[i].cfgB.raw, 6);
    }
    transmitCMDW(WRCFGB, write_buffer);

    return 0;
}

int ADBMS_ReadConfig(uint8_t total_ic, ADBMS_IC_t ic[])
{
    uint8_t read_buffer[total_ic * 8];
    int pec_errors = 0;

    /* Read CFGA */
    transmitCMDR(RDCFGA, read_buffer, 8 * total_ic);
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(ic[i].cfgA.raw, &read_buffer[i * 8], 6);
        /* PEC check would go here - simplified for now */
    }

    /* Read CFGB */
    transmitCMDR(RDCFGB, read_buffer, 8 * total_ic);
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(ic[i].cfgB.raw, &read_buffer[i * 8], 6);
    }

    return pec_errors;
}

/*============================================================================
 * ADC Conversion Commands
 *============================================================================*/

void ADBMS_StartCellADC(bool continuous, bool discharge_permit, ADBMS_OpenWire_t ow_mode)
{
    uint16_t cmd = ADCV;

    if (continuous)
    {
        cmd |= ADCV_CONT;
    }
    if (discharge_permit)
    {
        cmd |= ADCV_DCP;
    }

    switch (ow_mode)
    {
    case ADBMS_OW_PULLUP:
        cmd |= ADCV_OW_PUP;
        break;
    case ADBMS_OW_PULLDOWN:
        cmd |= ADCV_OW_PDN;
        break;
    default:
        break;
    }

    transmitCMD(cmd);
}

void ADBMS_StartSADC(void)
{
    transmitCMD(ADSV);
}

void ADBMS_StartAuxADC(ADBMS_AuxChannel_t channel, bool open_wire, bool pullup)
{
    uint16_t cmd = ADAX;

    /* Channel selection (bits 4:0) */
    cmd |= (channel & 0x1F);

    if (open_wire)
    {
        cmd |= ADAX_OW;
    }
    if (pullup)
    {
        cmd |= ADAX_PUP;
    }

    transmitCMD(cmd);
}

int ADBMS_PollADC(uint32_t timeout_ms)
{
    /* Note: In RTOS environments, prefer osDelay() over polling */
    uint32_t counter = 0;
    uint8_t status = 0;

    while (counter < timeout_ms * 1000)
    {
        status = FEB_spi_read_byte(0xFF);
        if (status > 0)
        {
            return 0;  /* Conversion complete */
        }
        counter += 10;
    }

    return 1;  /* Timeout - still converting */
}

/*============================================================================
 * Read Functions
 *============================================================================*/

int ADBMS_ReadCellVoltages(uint8_t total_ic, ADBMS_IC_t ic[])
{
    uint8_t read_buffer[total_ic * 8];
    uint16_t codes[6] = {RDCVA, RDCVB, RDCVC, RDCVD, RDCVE, RDCVF};
    int pec_errors = 0;

    for (int reg = 0; reg < 6; reg++)
    {
        wakeup_sleep(total_ic);
        transmitCMDR(codes[reg], read_buffer, 8 * total_ic);

        uint8_t bytes_in_group = (reg == 5) ? 2 : 6;
        for (uint8_t i = 0; i < total_ic; i++)
        {
            uint8_t *data = &read_buffer[i * 8];
            for (uint8_t b = 0; b < bytes_in_group; b += 2)
            {
                uint8_t cell_idx = b / 2 + 3 * reg;
                ic[i].c_codes[cell_idx] = data[b] | ((uint16_t)data[b + 1] << 8);
            }
        }

        /* PEC verification - simplified */
        uint16_t calc_pec = pec10_calc(6, read_buffer);
        uint16_t rx_pec = read_buffer[6] | ((uint16_t)read_buffer[7] << 8);
        if (calc_pec != rx_pec)
        {
            pec_errors++;
        }
    }

    return pec_errors;
}

int ADBMS_ReadSVoltages(uint8_t total_ic, ADBMS_IC_t ic[])
{
    uint8_t read_buffer[total_ic * 8];
    uint16_t codes[6] = {RDSVA, RDSVB, RDSVC, RDSVD, RDSVE, RDSVF};
    int pec_errors = 0;

    for (int reg = 0; reg < 6; reg++)
    {
        wakeup_sleep(total_ic);
        transmitCMDR(codes[reg], read_buffer, 8 * total_ic);

        uint8_t bytes_in_group = (reg == 5) ? 2 : 6;
        for (uint8_t i = 0; i < total_ic; i++)
        {
            uint8_t *data = &read_buffer[i * 8];
            for (uint8_t b = 0; b < bytes_in_group; b += 2)
            {
                uint8_t cell_idx = b / 2 + 3 * reg;
                ic[i].s_codes[cell_idx] = data[b] | ((uint16_t)data[b + 1] << 8);
            }
        }

        /* PEC verification - simplified */
        uint16_t calc_pec = pec10_calc(6, read_buffer);
        uint16_t rx_pec = read_buffer[6] | ((uint16_t)read_buffer[7] << 8);
        if (calc_pec != rx_pec)
        {
            pec_errors++;
        }
    }

    return pec_errors;
}

int ADBMS_ReadAux(uint8_t total_ic, ADBMS_IC_t ic[])
{
    uint8_t read_buffer[total_ic * 8];
    int pec_errors = 0;

    /* Read AUXA (GPIO 1-3) */
    transmitCMDR(RDAUXA, read_buffer, 8 * total_ic);
    for (uint8_t i = 0; i < total_ic; i++)
    {
        uint8_t *data = &read_buffer[i * 8];
        ic[i].aux_codes[0] = data[0] | ((uint16_t)data[1] << 8);
        ic[i].aux_codes[1] = data[2] | ((uint16_t)data[3] << 8);
        ic[i].aux_codes[2] = data[4] | ((uint16_t)data[5] << 8);
    }

    return pec_errors;
}

int ADBMS_ReadStatus(uint8_t total_ic, ADBMS_IC_t ic[])
{
    uint8_t read_buffer[total_ic * 8];
    int pec_errors = 0;

    /* Read STATA */
    transmitCMDR(RDSTATA, read_buffer, 8 * total_ic);
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(ic[i].statA.raw, &read_buffer[i * 8], 6);
    }

    /* Read STATB */
    transmitCMDR(RDSTATB, read_buffer, 8 * total_ic);
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(ic[i].statB.raw, &read_buffer[i * 8], 6);
    }

    return pec_errors;
}

int ADBMS_ReadSerialID(uint8_t total_ic, ADBMS_IC_t ic[])
{
    uint8_t read_buffer[total_ic * 8];
    int pec_errors = 0;

    wakeup_sleep(total_ic);
    transmitCMDR(RDSID, read_buffer, 8 * total_ic);

    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(ic[i].serial_id, &read_buffer[i * 8], 6);

        /* PEC verification */
        uint8_t *data = &read_buffer[i * 8];
        uint16_t calc_pec = Pec10_calc(false, 6, data);
        uint16_t rx_pec = ((uint16_t)data[6] << 8) | data[7];
        if (calc_pec != rx_pec)
        {
            pec_errors++;
            ic[i].last_pec_status = 1;
        }
        else
        {
            ic[i].last_pec_status = 0;
        }
    }

    return pec_errors;
}

/*============================================================================
 * Write Functions
 *============================================================================*/

int ADBMS_WritePWM(uint8_t total_ic, const ADBMS_IC_t ic[])
{
    uint8_t write_buffer[total_ic * 6];

    /* Write PWMA */
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(&write_buffer[i * 6], ic[i].pwmA.raw, 6);
    }
    transmitCMDW(WRPWMA, write_buffer);

    /* Write PWMB */
    for (uint8_t i = 0; i < total_ic; i++)
    {
        memcpy(&write_buffer[i * 6], ic[i].pwmB.raw, 6);
    }
    transmitCMDW(WRPWMB, write_buffer);

    return 0;
}

/*============================================================================
 * Control Commands
 *============================================================================*/

void ADBMS_ClearCells(void)
{
    transmitCMD(CLRCELL);
}

void ADBMS_ClearAux(void)
{
    transmitCMD(CLRAUX);
}

void ADBMS_ClearFlags(void)
{
    transmitCMD(CLRFLAG);
}

void ADBMS_Mute(void)
{
    transmitCMD(MUTE);
}

void ADBMS_Unmute(void)
{
    transmitCMD(UNMUTE);
}

void ADBMS_SoftReset(void)
{
    transmitCMD(SRST);
}

/*============================================================================
 * Configuration Helpers
 *============================================================================*/

void ADBMS_SetDischarge(ADBMS_IC_t *ic, uint16_t dcc_mask)
{
    ic->cfgB.bits.DCC_LO = dcc_mask & 0xFF;
    ic->cfgB.bits.DCC_HI = (dcc_mask >> 8) & 0xFF;
}

void ADBMS_SetUVThreshold(ADBMS_IC_t *ic, uint16_t voltage_mV)
{
    uint16_t code = voltage_to_threshold_code((float)voltage_mV);
    ic->cfgB.bits.VUV_LO = code & 0xFF;
    ic->cfgB.bits.VUV_HI = (code >> 8) & 0x0F;
}

void ADBMS_SetOVThreshold(ADBMS_IC_t *ic, uint16_t voltage_mV)
{
    uint16_t code = voltage_to_threshold_code((float)voltage_mV);
    ic->cfgB.bits.VOV_LO = code & 0x0F;
    ic->cfgB.bits.VOV_HI = (code >> 4) & 0xFF;
}

void ADBMS_SetGPIO(ADBMS_IC_t *ic, uint16_t gpio_mask)
{
    ic->cfgA.bits.GPO_1_8 = gpio_mask & 0xFF;
    ic->cfgA.bits.GPO_9_10 = (gpio_mask >> 8) & 0x03;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

bool ADBMS_IsSerialIDValid(const uint8_t sid[6])
{
    bool all_zero = true;
    bool all_ff = true;

    for (uint8_t i = 0; i < 6; i++)
    {
        if (sid[i] != 0x00)
        {
            all_zero = false;
        }
        if (sid[i] != 0xFF)
        {
            all_ff = false;
        }
    }

    return !all_zero && !all_ff;
}
