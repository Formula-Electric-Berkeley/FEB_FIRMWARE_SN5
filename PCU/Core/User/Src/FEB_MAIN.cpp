#include <FEB_CAN.cpp>
#include <FEB_ADC.cpp>
#include <cstdint>

/*
  Implausiblility:  sensors disagree by >10% pedal travel
                    >100 ms → motor controller cuts motor power

  PCU Specifics:    Brakes on + APPS >25% → cut motor power
                    Stays cut until APPS <5%, with or without brakes
*/

uint16_t APPS1_1 = 0; // Start 0%
uint16_t APPS1_2 = 0; // Max 100%

uint16_t APPS2_1 = 0; // Start 100%
uint16_t APPS2_2 = 0; // Max 0%

bool getAPPS1() {}
bool getAPPS2() {}

while (True)
{
  {
    if (!PC1_CAN_IsR2D())
      continue;
  }

  // Torque validation shi here
  // 4 APPS sensor variables, compare %
}
