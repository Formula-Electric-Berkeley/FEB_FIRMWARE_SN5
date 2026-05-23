/**
 ******************************************************************************
 * @file           : DCU_CAN_Filter.c
 * @brief          : Stub: no frames forwarded to radio yet
 * @author         : Formula Electric @ Berkeley
 *
 * Replace the body of DCU_CAN_Filter_ShouldForwardToRadio() with the project's
 * selection policy when the radio TX path is wired up. The logger already
 * calls this for every frame, so adding policy here lights up forwarding
 * without other code changes.
 ******************************************************************************
 */

#include "DCU_CAN_Filter.h"

bool DCU_CAN_Filter_ShouldForwardToRadio(const DCU_CAN_Frame_t *frame)
{
  (void)frame;
  return false;
}
