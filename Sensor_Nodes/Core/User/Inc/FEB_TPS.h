#ifndef FEB_TPS_H
#define FEB_TPS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"
#include "TPS2482.h"

// Same shunt resistor as LVPDB: WSR52L000FEA .002 ohm
#define FEB_TPS_R_SHUNT (double)(0.002)
#define FEB_TPS_I_MAX (double)(10.0) // adjust to your fuse rating
#define FEB_TPS_CURRENT_LSB TPS2482_CURRENT_LSB_EQ(FEB_TPS_I_MAX)
#define FEB_TPS_CAL_VAL TPS2482_CAL_EQ(FEB_TPS_CURRENT_LSB, FEB_TPS_R_SHUNT)
#define FEB_TPS_ALERT_LIM                                                                                              \
  TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(FEB_TPS_I_MAX / FEB_TPS_CURRENT_LSB), FEB_TPS_CAL_VAL)

// A1=GND, A0=GND → 0x40, confirm with I2C scanner
#define FEB_TPS_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND)

#define SIGN_MAGNITUDE(n) (int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF))
#define FLOAT_TO_INT16_T(n) ((int16_t)(n * 1000))

  void tps_init(void);
  void read_TPS(void);

  extern float tps_bus_voltage;   // V
  extern float tps_shunt_voltage; // mV
  extern float tps_current;       // A

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_H */
