#ifndef INC_FEB_CAN_H_
#define INC_FEB_CAN_H_

// **************************************** Includes ****************************************

#include "stm32f4xx_hal.h"
#include <stdint.h>

// Include generated pack/unpack functions from SN4 CAN library
#include "FEB_CAN_Library_SN4/gen/feb_can.h"

// **************************************** Types ****************************************

typedef struct __attribute__((packed))
{
  uint32_t tim_ms; // rollover handled by logging device
  /*
   *  Flags:
   ~  Bit 31: New current reading ready
   X  Bit 30: New bus voltage reading ready
   *  Bit 29: Bus undervoltage (Todo: define metric for undervoltage)
   *  Bit 28: Bus overvoltage (Todo: define metric for overvoltage)
   X  Bits [24:27] LVPDB first word ID (should be 0)
   *  Bits [16:23] Bus shutdown (V_bus = 0 -> fuse blown)
   *  Bits [8:15]: Power good
   *  Bits [0:7]: Alert pins for overcurrent
   */
  uint32_t flags;
  uint16_t bus_voltage; // They all run off of the same v_bus
  uint16_t lv_current;
  uint16_t cp_current;
  uint16_t af_current;
  uint16_t rf_current;
  uint16_t sh_current;
  uint16_t l_current;
  uint16_t as_current;
  uint16_t ab_current;
  uint16_t zero;   // byte stuffing
  uint32_t ids[5]; // 9 messages total so 9 id's sent out
} FEB_LVPDB_CAN_Data;

#endif /* INC_FEB_CAN_H_ */
