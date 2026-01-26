// ********************************** Includes & Externs *************************

#include "FEB_ADBMS6830B_Driver.h"
#include "FEB_HW.h"
#include "FEB_CMDCODES.h"

// ********************************** Functions **********************************

uint16_t SetOverVoltageThreshold(float voltage)
{
  uint16_t vov_value;
  uint8_t rbits = 12;
  voltage = (voltage - 1.5);
  voltage = voltage / (16 * 0.000150);
  vov_value = (uint16_t)(voltage + 2 * (1 << (rbits - 1)));
  vov_value &= 0xFFF;
  return vov_value;
}
uint16_t SetUnderVoltageThreshold(float voltage)
{
  uint16_t vuv_value;
  uint8_t rbits = 12;
  voltage = (voltage - 1.5);
  voltage = voltage / (16 * 0.000150);
  vuv_value = (uint16_t)(voltage + 2 * (1 << (rbits - 1)));
  vuv_value &= 0xFFF;
  return vuv_value;
}
/* Helper function to initialize CFG variables */
void ADBMS6830B_init_cfg(uint8_t total_ic, // Number of ICs in the system
                         cell_asic *ic     // A two dimensional array that stores the data
)
{
  for (uint8_t cic = 0; cic < total_ic; cic++)
  {
    /* Init config A */
    ic[cic].configa.tx_data[0] = 0x03; // REFON | | CTH[3]
    ic[cic].configa.tx_data[1] = 0x00; // FLAGS
    ic[cic].configa.tx_data[2] = 0x00;
    ic[cic].configa.tx_data[3] = 0xFF; // GPIO
    ic[cic].configa.tx_data[4] = 0x03;
    ic[cic].configa.tx_data[5] = 0x00;
    uint16_t VOVCode = SetOverVoltageThreshold(5.0);
    uint16_t VUVCode = SetUnderVoltageThreshold(2.0);
    ic[cic].configb.tx_data[0] = VUVCode & 0xFFFF;
    ic[cic].configb.tx_data[1] = (((VOVCode & 0x000F) << 4) | ((VUVCode) >> 8));
    ic[cic].configb.tx_data[2] = ((VOVCode >> 4) & 0x0FF);
    ic[cic].configb.tx_data[3] = 0xFF;
    ic[cic].configb.tx_data[4] = 0x00;
    ic[cic].configb.tx_data[5] = 0x00;
    for (int i = 0; i < 6; i++)
    {
      ic[cic].pwm.tx_data[i] = 0x00;
      ic[cic].pwmb.tx_data[i] = 0x00;
    }
  }
}

/* Helper Function to reset PEC counters */
void ADBMS6830B_reset_crc_count(uint8_t total_ic, // Number of ICs in the system
                                cell_asic *ic     // A two dimensional array that stores the data
)
{
  for (int current_ic = 0; current_ic < total_ic; current_ic++)
  {
    ic[current_ic].crc_count.pec_count = 0;
    ic[current_ic].crc_count.cfgr_pec = 0;
    for (int i = 0; i < 6; i++)
    {
      ic[current_ic].crc_count.cell_pec[i] = 0;
    }
    for (int i = 0; i < 4; i++)
    {
      ic[current_ic].crc_count.aux_pec[i] = 0;
    }
    for (int i = 0; i < 2; i++)
    {
      ic[current_ic].crc_count.stat_pec[i] = 0;
    }
  }
}

/* Helper function to set CFGR variable */
void ADBMS6830B_set_cfgr(uint8_t nIC,   // Current IC
                         cell_asic *ic, // A two dimensional array that stores the data
                         bool refon,    // The REFON bit
                         bool cth[3],   // The CTH bits
                         bool gpio[10], // The GPIO bits
                         uint16_t dcc,  // The DCC bits
                         bool dcto[6],  // The Dcto bits
                         uint16_t uv,   // The UV value
                         uint16_t ov    // The OV value
)
{
  ADBMS6830B_set_cfgr_refon(nIC, ic, refon);
  ADBMS6830B_set_cfgr_cth(nIC, ic, cth);
  ADBMS6830B_set_cfgr_gpio(nIC, ic, gpio);
  ADBMS6830B_set_cfgr_dis(nIC, ic, dcc);
  ADBMS6830B_set_cfgr_dcto(nIC, ic, dcto);
  ADBMS6830B_set_cfgr_uv(nIC, ic, uv);
  ADBMS6830B_set_cfgr_ov(nIC, ic, ov);
  // ADBMS6830B_set_cfgr_flags(nIC, ic, 255);
}

/* Helper function to set the REFON bit */
void ADBMS6830B_set_cfgr_refon(uint8_t nIC, cell_asic *ic, bool refon)
{
  if (refon)
    ic[nIC].configa.tx_data[0] = ic[nIC].configa.tx_data[0] | 0x80;
  else
    ic[nIC].configa.tx_data[0] = ic[nIC].configa.tx_data[0] & 0x7F;
}

void ADBMS6830B_set_cfgr_flags(uint8_t nIC, cell_asic *ic, uint8_t flags)
{
  ic[nIC].configa.tx_data[1] = flags;
}

/* Helper function to set CTH bits */
void ADBMS6830B_set_cfgr_cth(uint8_t nIC, cell_asic *ic, bool cth[3])
{
  for (int i = 0; i < 3; i++)
  {
    if (cth[i])
    {
      ic[nIC].configa.tx_data[0] = ic[nIC].configa.tx_data[0] | (0b01 << i);
    }
    else
    {
      ic[nIC].configa.tx_data[0] = ic[nIC].configa.tx_data[0] & ~(0b01 << i);
    }
  }
}

/* Helper function to set GPIO bits */
void ADBMS6830B_set_cfgr_gpio(uint8_t nIC, cell_asic *ic, bool gpio[10])
{
  for (int i = 0; i < 8; i++)
  {
    if (gpio[i])
    {
      ic[nIC].configa.tx_data[3] = ic[nIC].configa.tx_data[3] | (0b01 << i);
    }
    else
    {
      ic[nIC].configa.tx_data[3] = ic[nIC].configa.tx_data[3] & ~(0b01 << i);
    }
  }

  for (int i = 0; i < 2; i++)
  {
    if (gpio[i + 8])
    {
      ic[nIC].configa.tx_data[4] = ic[nIC].configa.tx_data[4] | (0b01 << i);
    }
    else
    {
      ic[nIC].configa.tx_data[4] = ic[nIC].configa.tx_data[4] & ~(0b01 << i);
    }
  }
}

/* Helper function to control discharge */
void ADBMS6830B_set_cfgr_dis(uint8_t nIC, cell_asic *ic, uint16_t dcc)
{
  ic[nIC].configb.tx_data[4] = (uint8_t)(dcc & 0xFF);
  ic[nIC].configb.tx_data[5] = (uint8_t)((dcc >> 8) & 0xFF);
}

/* Helper function to control discharge time value */
void ADBMS6830B_set_cfgr_dcto(uint8_t nIC, cell_asic *ic, bool dcto[6])
{
  for (int i = 0; i < 6; i++)
  {
    if (dcto[i])
    {
      ic[nIC].configb.tx_data[3] = ic[nIC].configb.tx_data[3] | (0b01 << i);
    }
    else
    {
      ic[nIC].configb.tx_data[3] = ic[nIC].configb.tx_data[3] & ~(0b01 << i);
    }
  }
}

/* Helper Function to set UV value in CFG register */
void ADBMS6830B_set_cfgr_uv(uint8_t nIC, cell_asic *ic, uint16_t uv)
{
  uint16_t tmp = (uv / 16);
  ic[nIC].configb.tx_data[0] = 0x00FF & tmp;
  ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & 0xF0;
  ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | ((0x0F00 & tmp) >> 8);
}

/* Helper function to set OV value in CFG register */
void ADBMS6830B_set_cfgr_ov(uint8_t nIC, cell_asic *ic, uint16_t ov)
{
  uint16_t tmp = (ov / 16);
  ic[nIC].configb.tx_data[2] = 0x00FF & (tmp >> 4);
  ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & 0x0F;
  ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | ((0x000F & tmp) << 4);
}

/* Initialize the Register limits */
void ADBMS6830B_init_reg_limits(uint8_t total_ic, // The number of ICs in the system
                                cell_asic *ic     // A two dimensional array where data will be written
)
{
  for (uint8_t cic = 0; cic < total_ic; cic++)
  {
    ic[cic].ic_reg.cell_channels = 20;
    ic[cic].ic_reg.stat_channels = 4; // Doesn't matter?
    ic[cic].ic_reg.aux_channels = 6;  // Doesn't matter?
    ic[cic].ic_reg.num_cv_reg = 6;
    ic[cic].ic_reg.num_gpio_reg = 4;
    ic[cic].ic_reg.num_stat_reg = 5;
  }
}

// ******************************** Voltage ********************************

/* Starts ADC conversion for cell voltage */
void ADBMS6830B_adcv(uint8_t RD,   // ADC Mode
                     uint8_t DCP,  // Discharge Permit
                     uint8_t CONT, // Cell Channels to be measured
                     uint8_t RSTF, // Reset Filter
                     uint8_t OW    // Open-wire bits
)
{
  uint8_t cmd[2];

  cmd[0] = 0x02 + RD;
  cmd[1] = 0x60 | (CONT << 7) | (DCP << 4) | (RSTF << 2) | OW;
  cmd_68(cmd);
}

/* This function will block operation until the ADC has finished it's conversion */
uint32_t ADBMS6830B_pollAdc()
{
  uint32_t counter = 0;
  uint8_t finished = 0;
  uint8_t current_time = 0;
  uint8_t temp[1] = {0x00};
  //		uint8_t cmd[4];
  //		uint16_t cmd_pec;

  //		cmd[0] = 0x07;
  //		cmd[1] = 0x18;
  //		cmd_pec = pec15_calc(2, cmd);
  //		cmd[2] = (uint8_t) (cmd_pec >> 8);
  //		cmd[3] = (uint8_t) (cmd_pec);

  //		FEB_cs_low();
  //		FEB_spi_write_array(4, cmd);

  transmitCMDR(ADSV, temp, 1);

  while ((counter < 200000) && (finished == 0))
  {
    current_time = FEB_spi_read_byte(0xff);
    if (current_time > 0)
    {
      finished = 1;
    }
    else
    {
      counter = counter + 10;
    }
  }

  //		FEB_cs_high();

  return (counter);
}

/*
Reads and parses the ADBMS6830B cell voltage registers.
The function is used to read the parsed Cell voltages codes of the LTC681x.
This function will send the requested read commands parse the data
and store the cell voltages in c_codes variable.
*/

uint8_t ADBMS6830B_rdcv(uint8_t total_ic, // The number of ICs in the system
                        cell_asic *ic     // Array of the parsed cell codes
)
{
#ifdef READALL
  uint8_t TxSize = 34;
  uint8_t cell_data[TxSize * total_ic];
  transmitCMDR(RDCVALL, cell_data, TxSize * total_ic);
  for (int icn = 0; icn < total_ic; icn++)
  {
    for (int byte = 0; byte < 34; byte++)
    {
      if (byte % 2 == 0)
      {
        ic[icn].cells.c_codes[byte / 2] = 0;
        ic[icn].cells.c_codes[byte / 2] |= cell_data[byte + TxSize * icn];
      }
      else
      {
        ic[icn].cells.c_codes[byte / 2] |= cell_data[byte + TxSize * icn] << 8;
      }
    }
  }
  // memcpy(&(ic[icn].cells.c_codes),cell_data+icn*TxSize,(size_t)TxSize);

  int16_t c_data_pec = pec10_calc(TxSize - 2, cell_data);
  int16_t c_rx_pec = *(uint16_t *)(cell_data + TxSize - 2);

  return (c_data_pec != c_rx_pec);
#else
  uint8_t errorCount = 0;
  uint8_t TxSize = 8;
  uint8_t cell_data[TxSize * total_ic];
  uint16_t codes[6] = {RDCVA, RDCVB, RDCVC, RDCVD, RDCVE, RDCVF};
  for (int REGGRP = 0; REGGRP < 6; REGGRP++)
  {
    wakeup_sleep(FEB_NUM_IC);
    transmitCMDR(codes[REGGRP], cell_data, TxSize * total_ic);
    uint8_t bytesInGroup = (REGGRP == 5) ? 2 : 6;
    for (int icn = 0; icn < total_ic; icn++)
    {
      for (int byte = 0; byte < bytesInGroup; byte++)
      {
        if (byte % 2 == 0)
        {
          ic[icn].cells.c_codes[byte / 2 + 3 * REGGRP] = 0;
          ic[icn].cells.c_codes[byte / 2 + 3 * REGGRP] |= cell_data[byte + TxSize * icn];
        }
        else
        {
          ic[icn].cells.c_codes[byte / 2 + 3 * REGGRP] |= cell_data[byte + TxSize * icn] << 8;
        }
      }
    }
    int16_t c_data_pec = pec10_calc(TxSize - 2, cell_data);
    int16_t c_rx_pec = *(uint16_t *)(cell_data + TxSize - 2);
    if (c_data_pec != c_rx_pec)
      errorCount++;
  }
  return errorCount;
#endif
}

uint8_t ADBMS6830B_rdsv(uint8_t total_ic, // The number of ICs in the system
                        cell_asic *ic     // Array of the parsed cell codes
)
{
#ifdef READALL
  uint8_t TxSize = 34;
  uint8_t cell_data[TxSize * total_ic];
  transmitCMDR(RDSALL, cell_data, TxSize * total_ic);
  for (int icn = 0; icn < total_ic; icn++)
  {
    for (int byte = 0; byte < 34; byte++)
    {
      if (byte % 2 == 0)
      {
        ic[icn].cells.s_codes[byte / 2] = 0;
        ic[icn].cells.s_codes[byte / 2] |= cell_data[byte + TxSize * icn];
      }
      else
      {
        ic[icn].cells.s_codes[byte / 2] |= cell_data[byte + TxSize * icn] << 8;
      }
    }
  }
  // memcpy(&(ic[icn].cells.s_codes),cell_data+icn*TxSize,(size_t)34);
  uint16_t data_pec = pec10_calc(32, cell_data);
  uint16_t rx_pec = *(uint16_t *)(cell_data + 32);

  return (data_pec != rx_pec);
#else
  uint8_t errorCount = 0;
  uint8_t TxSize = 8;
  uint8_t cell_data[TxSize * total_ic];
  uint16_t codes[6] = {RDSVA, RDSVB, RDSVC, RDSVD, RDSVE, RDSVF};
  for (int REGGRP = 0; REGGRP < 6; REGGRP++)
  {
    wakeup_sleep(FEB_NUM_IC);
    transmitCMDR(codes[REGGRP], cell_data, TxSize * total_ic);
    uint8_t bytesInGroup = (REGGRP == 5) ? 2 : 6;
    for (int icn = 0; icn < total_ic; icn++)
    {
      for (int byte = 0; byte < bytesInGroup; byte++)
      {
        if (byte % 2 == 0)
        {
          ic[icn].cells.s_codes[byte / 2 + 3 * REGGRP] = 0;
          ic[icn].cells.s_codes[byte / 2 + 3 * REGGRP] |= cell_data[byte + TxSize * icn];
        }
        else
        {
          ic[icn].cells.s_codes[byte / 2 + 3 * REGGRP] |= cell_data[byte + TxSize * icn] << 8;
        }
      }
    }
    int16_t c_data_pec = pec10_calc(TxSize - 2, cell_data);
    int16_t c_rx_pec = *(uint16_t *)(cell_data + TxSize - 2);
    if (c_data_pec != c_rx_pec)
      errorCount++;
  }
  return errorCount;
#endif
}
// ******************************** Temperature ********************************
void ADBMS6830B_wrALL(uint8_t total_ic, // The number of ICs being written to
                      cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  ADBMS6830B_wrcfga(total_ic, ic);
  ADBMS6830B_wrcfgb(total_ic, ic);
  // ADBMS6830B_wrpwmga(total_ic, ic);
  // ADBMS6830B_wrpwmgb(total_ic, ic);
}
void ADBMS6830B_rdALL(uint8_t total_ic, // The number of ICs being written to
                      cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  // wakeup_sleep(total_ic);
  ADBMS6830B_rdcfga(total_ic, ic);
  // wakeup_sleep(total_ic);
  ADBMS6830B_rdcfgb(total_ic, ic);
  // wakeup_sleep(total_ic);
  ADBMS6830B_rdpwmga(total_ic, ic);
  // wakeup_sleep(total_ic);
  ADBMS6830B_rdpwmgb(total_ic, ic);
}
/* Write the ADBMS6830B CFGRA */
void ADBMS6830B_wrcfga(uint8_t total_ic, // The number of ICs being written to
                       cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t write_buffer[total_ic * 6];
  uint8_t write_count = 0;
  uint8_t c_ic = 0;
  for (uint8_t current_ic = 0; current_ic < total_ic; current_ic++)
  {
    c_ic = current_ic;
    for (uint8_t data = 0; data < 6; data++)
    {
      write_buffer[write_count] = ic[c_ic].configa.tx_data[data];
      write_count++;
    }
  }

  transmitCMDW(WRCFGA, write_buffer);
}

void ADBMS6830B_rdcfga(uint8_t total_ic, // The number of ICs being written to
                       cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t TxSize = 8;
  uint8_t *cell_data;
  cell_data = (uint8_t *)malloc(TxSize * total_ic * sizeof(uint8_t));

  transmitCMDR(RDCFGA, cell_data, 8 * total_ic);
  for (int bank = 0; bank < total_ic; bank++)
  {
    memcpy(&(ic[bank].configa.rx_data), cell_data + bank * TxSize, (size_t)8);
  }
  int16_t c_data_pec = pec10_calc(TxSize - 2, cell_data);
  int16_t c_rx_pec = *(uint16_t *)(cell_data + TxSize - 2);
  for (int bank = 0; bank < total_ic; bank++)
    ic[bank].configa.rx_pec_match = c_data_pec - c_rx_pec;
  free(cell_data);
}
/* Write the ADBMS6830B CFGRB */
void ADBMS6830B_wrcfgb(uint8_t total_ic, // The number of ICs being written to
                       cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t write_buffer[256];
  uint8_t write_count = 0;
  uint8_t c_ic = 0;

  for (uint8_t current_ic = 0; current_ic < total_ic; current_ic++)
  {
    c_ic = current_ic;
    for (uint8_t data = 0; data < 6; data++)
    {
      write_buffer[write_count] = ic[c_ic].configb.tx_data[data];
      write_count++;
    }
  }
  transmitCMDW(WRCFGB, write_buffer);
}

void ADBMS6830B_rdcfgb(uint8_t total_ic, // The number of ICs being written to
                       cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t TxSize = 8;
  uint8_t *cell_data;
  cell_data = (uint8_t *)malloc(TxSize * total_ic * sizeof(uint8_t));
  transmitCMDR(RDCFGB, cell_data, 8 * total_ic);

  for (int bank = 0; bank < total_ic; bank++)
  {
    memcpy(&(ic[bank].configb.rx_data), cell_data + bank * TxSize, (size_t)8);
  }
  free(cell_data);
}

/* Start ADC Conversion for GPIO and Vref2  */
void ADBMS6830B_adax(uint8_t OW,  // Open Wire Detection
                     uint8_t PUP, // Pull up/pull down current sources during measurement
                     uint8_t CH   // GPIO Channels to be measured
)
{
  uint8_t cmd[4];

  cmd[0] = OW + 0x04;
  cmd[1] = (PUP << 7) + ((CH & 0x10) << 2) + (CH & 0xF) + 0x10;

  cmd_68(cmd);
}
void ADBMS6830B_wrpwmga(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t write_buffer[256];
  uint8_t write_count = 0;
  uint8_t c_ic = 0;
  for (uint8_t current_ic = 0; current_ic < total_ic; current_ic++)
  {
    c_ic = current_ic;
    for (uint8_t data = 0; data < 6; data++)
    {
      write_buffer[write_count] = ic[c_ic].pwm.tx_data[data];
      write_count++;
    }
  }
  transmitCMDW(WRPWMA, write_buffer);
}
void ADBMS6830B_rdpwmga(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t TxSize = 8;
  uint8_t *cell_data;
  cell_data = (uint8_t *)malloc(TxSize * total_ic * sizeof(uint8_t));
  transmitCMDR(RDPWMA, cell_data, 8 * total_ic);
  for (int bank = 0; bank < total_ic; bank++)
  {
    memcpy(&(ic[bank].pwm.rx_data), cell_data + bank * TxSize, (size_t)8);
  }
  free(cell_data);
}
void ADBMS6830B_wrpwmgb(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t write_buffer[256];
  uint8_t write_count = 0;
  uint8_t c_ic = 0;
  for (uint8_t current_ic = 0; current_ic < total_ic; current_ic++)
  {
    c_ic = current_ic;
    for (uint8_t data = 0; data < 6; data++)
    {
      write_buffer[write_count] = ic[c_ic].pwmb.tx_data[data];
      write_count++;
    }
  }
  transmitCMDW(WRPWMB, write_buffer);
}
void ADBMS6830B_rdpwmgb(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
)
{
  uint8_t TxSize = 8;
  uint8_t *cell_data;
  cell_data = (uint8_t *)malloc(TxSize * total_ic * sizeof(uint8_t));
  transmitCMDR(RDPWMB, cell_data, 8 * total_ic);
  for (int bank = 0; bank < total_ic; bank++)
  {
    memcpy(&(ic[bank].pwmb.rx_data), cell_data + bank * TxSize, (size_t)8);
  }
  free(cell_data);
}
/*
The function is used to read the  parsed GPIO codes of the ADBMS6830B.
This function will send the requested read commands parse the data
and store the gpio voltages in a_codes variable.
*/

uint8_t ADBMS6830B_rdaux(uint8_t total_ic, // The number of ICs in the system
                         cell_asic *ic     // Array of the parsed cell codes
)
{
  const uint8_t REG_LEN = 8; // Number of bytes in each ICs register + 2 bytes for the PEC

  int8_t pec_error = 0;
  uint8_t *cell_data;
  uint8_t c_ic = 0;
  cell_data = (uint8_t *)malloc((NUM_RX_BYT * total_ic) * sizeof(uint8_t));
  transmitCMDR(RDAUXA, cell_data, NUM_RX_BYT * total_ic);
  for (int i = 0; i < total_ic; i++)
    memcpy(&(ic[i].aux.a_codes), cell_data + i * NUM_RX_BYT, NUM_RX_BYT);
  // ADBMS6830B_check_pec(total_ic, CELL, ic);
  free(cell_data);
  return (pec_error);
}
void ADBMS6830B_CLRFLAG(uint8_t total_ic)
{
  int8_t flagData[256];
  uint8_t write_count = 0;
  uint8_t c_ic = 0;
  for (uint8_t current_ic = 0; current_ic < total_ic; current_ic++)
  {
    c_ic = current_ic;
    for (uint8_t data = 0; data < 6; data++)
    {
      flagData[write_count] = 0xFF;
      write_count++;
    }
  }
  transmitCMDW(CLRFLAG, flagData);
}
/* Generic wakeup command to wake the ADBMS6830B from sleep state */
void wakeup_sleep(uint8_t total_ic) // Number of ICs in the system
{
  for (int i = 0; i < 1; i++)
  {
    FEB_cs_low();
    HAL_Delay(1);
    FEB_cs_high();
    HAL_Delay(1);
  }
}

void ADBMS6830B_check_pec(uint8_t total_ic, // Number of ICs in the system
                          uint8_t reg,      // Type of Register
                          cell_asic *ic     // A two dimensional array that stores the data
)
{
  switch (reg)
  {
  case CFGRA:
    for (int current_ic = 0; current_ic < total_ic; current_ic++)
    {
      ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].configa.rx_pec_match;
      ic[current_ic].crc_count.cfgr_pec = ic[current_ic].crc_count.cfgr_pec + ic[current_ic].configa.rx_pec_match;
    }
    break;

  case CFGRB:
    for (int current_ic = 0; current_ic < total_ic; current_ic++)
    {
      ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].configb.rx_pec_match;
      ic[current_ic].crc_count.cfgr_pec = ic[current_ic].crc_count.cfgr_pec + ic[current_ic].configb.rx_pec_match;
    }
    break;
  case CELL:
    for (int current_ic = 0; current_ic < total_ic; current_ic++)
    {
      for (int i = 0; i < ic[0].ic_reg.num_cv_reg; i++)
      {
        ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].cells.pec_match[i];
        ic[current_ic].crc_count.cell_pec[i] = ic[current_ic].crc_count.cell_pec[i] + ic[current_ic].cells.pec_match[i];
      }
    }
    break;
  case AUX:
    for (int current_ic = 0; current_ic < total_ic; current_ic++)
    {
      for (int i = 0; i < ic[0].ic_reg.num_gpio_reg; i++)
      {
        ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + (ic[current_ic].aux.pec_match[i]);
        ic[current_ic].crc_count.aux_pec[i] = ic[current_ic].crc_count.aux_pec[i] + (ic[current_ic].aux.pec_match[i]);
      }
    }

    break;
  case STAT:
    for (int current_ic = 0; current_ic < total_ic; current_ic++)
    {

      for (int i = 0; i < ic[0].ic_reg.num_stat_reg - 1; i++)
      {
        ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].stat.pec_match[i];
        ic[current_ic].crc_count.stat_pec[i] = ic[current_ic].crc_count.stat_pec[i] + ic[current_ic].stat.pec_match[i];
      }
    }
    break;
  default:
    break;
  }
}