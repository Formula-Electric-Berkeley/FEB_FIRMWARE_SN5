#include "FEB_IMU.h"
#include "FEB_MMeter.h"

#include "main.h"

// static void FEB_SystemClock_Config(void);
// void MX_I2C1_Init(void);

void FEB_Init(void)
{
  lsm6dsox_init();
  lis3mdl_init();
}

void FEB_Update(void)
{
  read_Acceleration();
  read_Angular_Rate();
  read_Magnetic_Field_Data();
}
