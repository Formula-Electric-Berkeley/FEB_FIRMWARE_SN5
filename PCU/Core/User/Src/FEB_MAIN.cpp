#include <FEB_CAN.cpp>

while (True)
{
  if (!PCU_CAN_IsR2D())
  {
    continue;
  }

  // Torque validation shi here
}
