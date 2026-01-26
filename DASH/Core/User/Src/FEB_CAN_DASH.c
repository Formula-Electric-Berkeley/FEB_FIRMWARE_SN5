// ============================================================================
// INCLUDES
// ============================================================================

#include "FEB_CAN_DASH.h"
#include "FEB_CAN_Frame_IDs.h"

// ============================================================================
// EXTERNAL VARIABLES
// ============================================================================

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart3;
extern CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
extern uint8_t FEB_CAN_Tx_Data[8];
extern uint32_t FEB_CAN_Tx_Mailbox;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

DASH_UI_Values_t DASH_UI_Values;
int16_t lv_voltage;

// ============================================================================
// CAN INITIALIZATION
// ============================================================================

/**
 * @brief Initialize DASH CAN message reception
 *
 * Registers callbacks for BMS voltage, temperature, motor speed, and LVPDB messages.
 * Following PCU's modular pattern: each subsystem registers its own callbacks.
 */
void FEB_CAN_DASH_Init(void)
{
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID, FEB_CAN_ID_STD,
                      FEB_CAN_DASH_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID, FEB_CAN_ID_STD,
                      FEB_CAN_DASH_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_RMS_MOTOR_SPEED_FRAME_ID, FEB_CAN_ID_STD, FEB_CAN_DASH_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID, FEB_CAN_ID_STD,
                      FEB_CAN_DASH_Callback);

  DASH_UI_Values.pack_voltage = 0;
  DASH_UI_Values.min_voltage = 0;
  DASH_UI_Values.max_acc_temp = 0;
  DASH_UI_Values.motor_speed = 0;
  lv_voltage = 0;
}

// ============================================================================
// CAN CALLBACK (RUNS IN INTERRUPT CONTEXT)
// ============================================================================

/**
 * @brief CAN RX callback for DASH UI messages
 *
 * Handles BMS voltage/temp, motor speed, and LVPDB voltage. Runs in interrupt context.
 * @param instance CAN instance
 * @param can_id Received CAN message ID
 * @param id_type Standard or Extended ID
 * @param data Pointer to received data bytes
 * @param length Number of data bytes received
 */
void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data,
                           uint8_t length)
{
  if (can_id == FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID)
  {
    if (length >= 4)
    {
      DASH_UI_Values.pack_voltage = (data[1] << 8) | data[0];
      DASH_UI_Values.min_voltage = (data[3] << 8) | data[2];
    }
  }
  else if (can_id == FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID)
  {
    if (length >= 6)
    {
      DASH_UI_Values.max_acc_temp = (data[5] << 8) | data[4];
    }
  }
  else if (can_id == FEB_CAN_RMS_MOTOR_SPEED_FRAME_ID)
  {
    if (length >= 4)
    {
      if (data[3] == 0xFF)
      {
        DASH_UI_Values.motor_speed = 0;
      }
      else
      {
        DASH_UI_Values.motor_speed = (data[2] << 8) + data[3];
      }
    }
  }
  else if (can_id == FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID)
  {
    if (length >= 6)
    {
      lv_voltage = (data[5] << 8) | data[4];
    }
  }
}

// ============================================================================
// CAN TRANSMIT FUNCTIONS
// ============================================================================

void FEB_CAN_DASH_Transmit_Button_State(uint8_t transmit_button_state)
{
  FEB_CAN_Tx_Header.DLC = 1;
  FEB_CAN_Tx_Header.StdId = FEB_CAN_DASH_IO_FRAME_ID;
  FEB_CAN_Tx_Header.IDE = CAN_ID_STD;
  FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
  FEB_CAN_Tx_Header.TransmitGlobalTime = DISABLE;

  // Copy data to Tx buffer
  FEB_CAN_Tx_Data[0] = transmit_button_state;

  // Delay until mailbox available
  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
  {
  }

  // Add Tx data to mailbox
  if (HAL_CAN_AddTxMessage(&hcan1, &FEB_CAN_Tx_Header, FEB_CAN_Tx_Data, &FEB_CAN_Tx_Mailbox) != HAL_OK)
  {
    //
  }
}
