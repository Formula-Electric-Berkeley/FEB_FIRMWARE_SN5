// ============================================================================
// INCLUDES
// ============================================================================

#include "FEB_CAN_PCU.h"
#include "FEB_CAN_Frame_IDs.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

FEB_CAN_PCU_Message_t FEB_CAN_PCU_Message;

// ============================================================================
// CAN INITIALIZATION
// ============================================================================

/**
 * @brief Initialize PCU CAN message reception
 *
 * Registers callbacks for brake pedal and RMS command messages from PCU.
 * Following PCU's modular pattern: each subsystem registers its own callbacks.
 */
void FEB_CAN_PCU_Init(void)
{
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_BRAKE_FRAME_ID, FEB_CAN_ID_STD, FEB_CAN_PCU_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_RMS_COMMAND_FRAME_ID, FEB_CAN_ID_STD, FEB_CAN_PCU_Callback);

  FEB_CAN_PCU_Message.brake_pedal = 0;
  FEB_CAN_PCU_Message.enabled = 0;
}

// ============================================================================
// CAN CALLBACK (RUNS IN INTERRUPT CONTEXT)
// ============================================================================

/**
 * @brief CAN RX callback for PCU messages
 *
 * Handles brake pedal position and inverter enable state. Runs in interrupt context.
 * @param instance CAN instance
 * @param can_id Received CAN message ID
 * @param id_type Standard or Extended ID
 * @param data Pointer to received data bytes
 * @param length Number of data bytes received
 */
void FEB_CAN_PCU_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data,
                          uint8_t length)
{
  if (can_id == FEB_CAN_PCU_BRAKE_FRAME_ID)
  {
    if (length >= 1)
    {
      FEB_CAN_PCU_Message.brake_pedal = data[0];
    }
  }
  else if (can_id == FEB_CAN_PCU_RMS_COMMAND_FRAME_ID)
  {
    if (length >= 6)
    {
      FEB_CAN_PCU_Message.enabled = data[5];
    }
  }
}

// ============================================================================
// GETTER FUNCTIONS
// ============================================================================

uint8_t FEB_CAN_PCU_Get_Brake_Pos()
{
  return FEB_CAN_PCU_Message.brake_pedal;
}

uint8_t FEB_CAN_PCU_Get_Enabled()
{
  return FEB_CAN_PCU_Message.enabled;
}
