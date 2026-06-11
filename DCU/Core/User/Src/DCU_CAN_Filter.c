/**
 ******************************************************************************
 * @file           : DCU_CAN_Filter.c
 * @brief          : Allow-list deciding which CAN frames are forwarded to radio
 * @author         : Formula Electric @ Berkeley
 *
 * The CSV logger calls DCU_CAN_Filter_ShouldForwardToRadio() for every frame it
 * captures on CAN1/CAN2. Returning true enqueues that frame for CAN-over-radio
 * transmission (see FEB_Task_Radio_ForwardCanFrame). To keep the radio link
 * (slow: a few hundred bytes/sec) from being flooded, only the IDs explicitly
 * listed in k_radio_allow[] below are forwarded.
 *
 *  >>> EDIT k_radio_allow[] TO CHOOSE WHICH CAN IDS GO OVER THE RADIO. <<<
 *
 * The list starts empty, so the default behaviour is "forward nothing" — radio
 * streaming is a no-op until you add entries here (and enable it at runtime
 * with `dcu|radio|stream on`).
 ******************************************************************************
 */

#include "DCU_CAN_Filter.h"
#include "DCU_CAN_Log.h" /* full DCU_CAN_Frame_t definition (bus, can_id, ...) */

#include <stddef.h>
#include <stdint.h>

/* Match any bus for an entry by setting .bus = DCU_CAN_BUS_ANY. */
#define DCU_CAN_BUS_ANY 0U

typedef struct
{
  uint8_t bus;     /**< 1 = CAN1, 2 = CAN2, DCU_CAN_BUS_ANY = either */
  uint32_t can_id; /**< 11-bit or 29-bit identifier to forward */
} DCU_CAN_AllowEntry_t;

/* ----------------------------------------------------------------------------
 * Radio forward allow-list — ADD YOUR IDS HERE.
 *
 * Examples (delete or replace with the real IDs you want on the radio link):
 *   { .bus = 1,               .can_id = 0x0A0 },   // a CAN1 message
 *   { .bus = 2,               .can_id = 0x123 },   // a CAN2 message
 *   { .bus = DCU_CAN_BUS_ANY, .can_id = 0x300 },   // 0x300 from either bus
 * -------------------------------------------------------------------------- */
static const DCU_CAN_AllowEntry_t k_radio_allow[] = {
    /* (empty — nothing forwarded until you add entries) */
};

#define DCU_CAN_ALLOW_COUNT (sizeof(k_radio_allow) / sizeof(k_radio_allow[0]))

bool DCU_CAN_Filter_ShouldForwardToRadio(const DCU_CAN_Frame_t *frame)
{
  if (frame == NULL)
  {
    return false;
  }

  for (size_t i = 0; i < DCU_CAN_ALLOW_COUNT; i++)
  {
    const bool bus_ok = (k_radio_allow[i].bus == DCU_CAN_BUS_ANY) || (k_radio_allow[i].bus == frame->bus);
    if (bus_ok && k_radio_allow[i].can_id == frame->can_id)
    {
      return true;
    }
  }
  return false;
}
