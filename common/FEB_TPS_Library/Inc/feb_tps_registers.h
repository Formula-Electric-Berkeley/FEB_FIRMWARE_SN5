/**
 ******************************************************************************
 * @file           : feb_tps_registers.h
 * @brief          : TPS2482 register definitions
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * TPS2482 I2C register addresses, bit definitions, and conversion constants.
 * Reference: TPS2482 Datasheet (https://www.ti.com/lit/ds/symlink/tps2482.pdf)
 *
 ******************************************************************************
 */

#ifndef FEB_TPS_REGISTERS_H
#define FEB_TPS_REGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================================
 * I2C Address Calculation (Table 8)
 * ============================================================================ */

/** Address pin options for A0/A1 */
#define FEB_TPS_PIN_GND   0x00
#define FEB_TPS_PIN_VS    0x01
#define FEB_TPS_PIN_SDA   0x02
#define FEB_TPS_PIN_SCL   0x03

/**
 * @brief Calculate 7-bit I2C address from A1 and A0 pin settings
 * @param a1 A1 pin setting (FEB_TPS_PIN_GND, FEB_TPS_PIN_VS, FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL)
 * @param a0 A0 pin setting
 * @return 7-bit I2C address
 */
#define FEB_TPS_ADDR(a1, a0)  ((uint8_t)(0x40 | (((a1) << 2) | (a0))))

/* ============================================================================
 * Register Addresses (Table 3)
 * ============================================================================ */

#define FEB_TPS_REG_CONFIG      0x00    /**< R/W - Configuration */
#define FEB_TPS_REG_SHUNT_VOLT  0x01    /**< R   - Shunt Voltage */
#define FEB_TPS_REG_BUS_VOLT    0x02    /**< R   - Bus Voltage */
#define FEB_TPS_REG_POWER       0x03    /**< R   - Power */
#define FEB_TPS_REG_CURRENT     0x04    /**< R   - Current */
#define FEB_TPS_REG_CAL         0x05    /**< R/W - Calibration */
#define FEB_TPS_REG_MASK        0x06    /**< R/W - Mask/Enable */
#define FEB_TPS_REG_ALERT_LIM   0x07    /**< R/W - Alert Limit */
#define FEB_TPS_REG_ID          0xFF    /**< R   - Unique ID */

/* ============================================================================
 * Register Default Values (Table 3)
 * ============================================================================ */

#define FEB_TPS_CONFIG_DEFAULT  0x4127  /**< Default CONFIG register value */

/* ============================================================================
 * Configuration Register Bit Definitions (Tables 4-7)
 * ============================================================================ */

#define FEB_TPS_CFG_RST         (1U << 15)    /**< Reset bit */
#define FEB_TPS_CFG_AVG_MASK    (7U << 9)     /**< Averaging mode mask */
#define FEB_TPS_CFG_VBUS_CT_MASK (7U << 6)    /**< Bus voltage conversion time mask */
#define FEB_TPS_CFG_VSH_CT_MASK  (7U << 3)    /**< Shunt voltage conversion time mask */
#define FEB_TPS_CFG_MODE_MASK   (7U << 0)     /**< Operating mode mask */

/** Averaging mode values (Table 4) */
#define FEB_TPS_AVG_1           (0U << 9)
#define FEB_TPS_AVG_4           (1U << 9)
#define FEB_TPS_AVG_16          (2U << 9)
#define FEB_TPS_AVG_64          (3U << 9)
#define FEB_TPS_AVG_128         (4U << 9)
#define FEB_TPS_AVG_256         (5U << 9)
#define FEB_TPS_AVG_512         (6U << 9)
#define FEB_TPS_AVG_1024        (7U << 9)

/** Conversion time values (Tables 5, 6) - applies to both VBUS_CT and VSH_CT */
#define FEB_TPS_CT_140US        0U
#define FEB_TPS_CT_204US        1U
#define FEB_TPS_CT_332US        2U
#define FEB_TPS_CT_588US        3U
#define FEB_TPS_CT_1100US       4U
#define FEB_TPS_CT_2116US       5U
#define FEB_TPS_CT_4156US       6U
#define FEB_TPS_CT_8244US       7U

/** Operating mode values (Table 7) */
#define FEB_TPS_MODE_POWERDOWN  0U
#define FEB_TPS_MODE_SHUNT_TRIG 1U
#define FEB_TPS_MODE_BUS_TRIG   2U
#define FEB_TPS_MODE_BOTH_TRIG  3U
#define FEB_TPS_MODE_POWERDOWN2 4U
#define FEB_TPS_MODE_SHUNT_CONT 5U
#define FEB_TPS_MODE_BUS_CONT   6U
#define FEB_TPS_MODE_BOTH_CONT  7U

/* ============================================================================
 * Mask/Enable Register Bit Definitions
 * ============================================================================ */

#define FEB_TPS_MASK_SOL        (1U << 15)    /**< Shunt Over-Voltage */
#define FEB_TPS_MASK_SUL        (1U << 14)    /**< Shunt Under-Voltage */
#define FEB_TPS_MASK_BOL        (1U << 13)    /**< Bus Over-Voltage */
#define FEB_TPS_MASK_BUL        (1U << 12)    /**< Bus Under-Voltage */
#define FEB_TPS_MASK_POL        (1U << 11)    /**< Power Over-Limit */
#define FEB_TPS_MASK_CNVR       (1U << 10)    /**< Conversion Ready */
#define FEB_TPS_MASK_AFF        (1U << 4)     /**< Alert Function Flag */
#define FEB_TPS_MASK_CVRF       (1U << 3)     /**< Conversion Ready Flag */
#define FEB_TPS_MASK_OVF        (1U << 2)     /**< Math Overflow Flag */
#define FEB_TPS_MASK_APOL       (1U << 1)     /**< Alert Polarity */
#define FEB_TPS_MASK_LEN        (1U << 0)     /**< Alert Latch Enable */

/* ============================================================================
 * Conversion Constants (from Datasheet)
 * ============================================================================ */

/** Bus voltage: 1.25 mV/LSB */
#define FEB_TPS_CONV_VBUS_V_PER_LSB     0.00125f

/** Shunt voltage: 2.5 uV/LSB (0.0025 mV/LSB) */
#define FEB_TPS_CONV_VSHUNT_MV_PER_LSB  0.0025f

/* ============================================================================
 * Calibration Calculation Macros (Equations 20-24)
 * ============================================================================ */

/**
 * @brief Calculate Current LSB from max current (Eq. 20)
 * @param i_max Maximum expected current in Amps
 * @return Current LSB in A/bit
 */
#define FEB_TPS_CALC_CURRENT_LSB(i_max) ((float)(i_max) / 32768.0f)

/**
 * @brief Calculate CAL register value (Eq. 21)
 * @param current_lsb Current LSB in A/bit (from FEB_TPS_CALC_CURRENT_LSB)
 * @param r_shunt Shunt resistor value in Ohms
 * @return CAL register value (uint16_t)
 */
#define FEB_TPS_CALC_CAL(current_lsb, r_shunt) \
    ((uint16_t)(0.00512f / ((current_lsb) * (r_shunt))))

/**
 * @brief Calculate Power LSB from Current LSB (Eq. 24)
 * @param current_lsb Current LSB in A/bit
 * @return Power LSB in W/bit
 */
#define FEB_TPS_CALC_POWER_LSB(current_lsb) ((current_lsb) * 25.0f)

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_REGISTERS_H */
