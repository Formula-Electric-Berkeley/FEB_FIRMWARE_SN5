#include "FEB_CAN_BMS.h"
#include "FEB_CAN_IDs.h"
#include "FEB_Debug.h"

/* Global BMS message data */
BMS_MESSAGE_TYPE BMS_MESSAGE;

uint16_t FEB_CAN_BMS_getTemp()
{
  return BMS_MESSAGE.temperature;
}

uint16_t FEB_CAN_BMS_getVoltage()
{
  return BMS_MESSAGE.voltage;
}

uint8_t FEB_CAN_BMS_getDeviceSelect()
{
  return BMS_MESSAGE.ping_ack;
}

FEB_SM_ST_t FEB_CAN_BMS_getState()
{
  return BMS_MESSAGE.state;
}

void FEB_CAN_BMS_Init(void)
{
  LOG_I(TAG_BMS, "Initializing BMS CAN communication");

  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BMS_STATE, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
  LOG_I(TAG_BMS, "Registered BMS CAN callbacks (Temp: 0x%03lX, State: 0x%03lX, Voltage: 0x%03lX)",
        FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE, FEB_CAN_ID_BMS_STATE, FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE);

  BMS_MESSAGE.temperature = 0;
  BMS_MESSAGE.voltage = 0;
  BMS_MESSAGE.state = FEB_SM_ST_DEFAULT;
  BMS_MESSAGE.ping_ack = FEB_HB_NULL;

  LOG_I(TAG_BMS, "BMS CAN initialization complete");
}

void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, uint8_t *data,
                          uint8_t length)
{
  LOG_D(TAG_BMS, "BMS Callback: ID=0x%03lX, Len=%d, Payload: %02X %02X %02X %02X %02X %02X %02X %02X", can_id, length,
        data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

  if (can_id == FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE)
  {
    BMS_MESSAGE.temperature = data[2] << 8 | data[3];
    LOG_D(TAG_BMS, "BMS temperature: %d", BMS_MESSAGE.temperature);
  }
  else if (can_id == FEB_CAN_ID_BMS_STATE)
  {
    FEB_SM_ST_t old_state = BMS_MESSAGE.state;
    BMS_MESSAGE.state = data[0] & 0x1F;
    BMS_MESSAGE.ping_ack = (data[0] & 0xE0) >> 5;

    if (old_state != BMS_MESSAGE.state)
    {
      LOG_I(TAG_BMS, "BMS state changed: %d -> %d", old_state, BMS_MESSAGE.state);
    }

    if (BMS_MESSAGE.state == FEB_SM_ST_HEALTH_CHECK || BMS_MESSAGE.ping_ack == FEB_HB_PCU)
    {
      LOG_D(TAG_BMS, "Sending heartbeat (state=%d, ping_ack=%d)", BMS_MESSAGE.state, BMS_MESSAGE.ping_ack);
      FEB_CAN_HEARTBEAT_Transmit();
    }

    LOG_D(TAG_BMS, "BMS state: %d, ping_ack: %d", BMS_MESSAGE.state, BMS_MESSAGE.ping_ack);
  }
  else if (can_id == FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE)
  {
    BMS_MESSAGE.voltage = (data[0] << 8) | (data[1]);
    LOG_D(TAG_BMS, "BMS voltage: %d", BMS_MESSAGE.voltage);
  }
  else
  {
    LOG_W(TAG_BMS, "Unknown BMS CAN ID: 0x%03lX", can_id);
  }
}

// TODO: NEED TO ADD DATA THAT IS BEING TRANSMITTED
void FEB_CAN_HEARTBEAT_Transmit(void)
{
  uint8_t data[8];
  data[0] = 1;

  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_HEARTBEAT_FRAME_ID, data, 1);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_BMS, "Failed to transmit heartbeat: %d", status);
  }
  else
  {
    LOG_D(TAG_BMS, "Heartbeat transmitted");
  }
}