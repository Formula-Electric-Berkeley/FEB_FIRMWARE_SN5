// ============================================================================
// INCLUDES
// ============================================================================

#include "FEB_CAN_BMS.h"
#include "FEB_CAN_Frame_IDs.h"
#include "FEB_CAN_Heartbeat.h"

// ============================================================================
// DATA STRUCTURES & ENUMS
// ============================================================================

typedef enum {
	FEB_HB_NULL,
	FEB_HB_DASH,
	FEB_HB_PCU,
	FEB_HB_LVPDB,
	FEB_HB_DCU,
	FEB_HB_FSN,
	FEB_HB_RSN
} FEB_HB_t;

typedef struct BMS_MESSAGE_TYPE {
	FEB_SM_ST_t state;
	FEB_HB_t ping_ack; // ping message
    uint32_t last_message_time;
    float ivt_voltage;
	float max_acc_temp;
	bool bms_fault;
	bool imd_fault;
} BMS_MESSAGE_TYPE;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

BMS_MESSAGE_TYPE bms_message;

#define BMS_TIMEOUT 1200

// ============================================================================
// GETTER FUNCTIONS
// ============================================================================

FEB_SM_ST_t FEB_CAN_BMS_Get_State(){
	return bms_message.state;
}

bool FEB_CAN_BMS_GET_FAULTS(){
	return (bms_message.bms_fault || bms_message.imd_fault);
}

bool FEB_CAN_GET_IMD_FAULT(){
	return bms_message.imd_fault;
}

// ============================================================================
// CAN INITIALIZATION
// ============================================================================

/**
 * @brief Initialize BMS CAN message reception
 * 
 * Registers callback for BMS state and fault messages.
 * Following PCU's modular pattern: each subsystem registers its own callbacks.
 */
void FEB_CAN_BMS_Init(void) {
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_STATE_FRAME_ID, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ACCUMULATOR_FAULTS_FRAME_ID, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
	
	bms_message.state = FEB_SM_ST_BOOT;
	bms_message.last_message_time = 0;
}

// ============================================================================
// CAN CALLBACK (RUNS IN INTERRUPT CONTEXT)
// ============================================================================

/**
 * @brief CAN RX callback for BMS messages
 * 
 * Handles BMS state and fault messages. Runs in interrupt context.
 * @param instance CAN instance (CAN1 or CAN2)
 * @param can_id Received CAN message ID
 * @param id_type Standard or Extended ID
 * @param data Pointer to received data bytes
 * @param length Number of data bytes received
 */
void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length) {
	if (can_id == FEB_CAN_BMS_STATE_FRAME_ID) {
		bms_message.state = data[0] & 0x1F;
		bms_message.ping_ack = (data[0] & 0xE0) >> 5;
		
		if (bms_message.state == FEB_SM_ST_HEALTH_CHECK || bms_message.ping_ack == FEB_HB_DASH) {
			FEB_CAN_HEARTBEAT_Transmit();
		}
		
		bms_message.last_message_time = HAL_GetTick();
	} else if (can_id == FEB_CAN_ACCUMULATOR_FAULTS_FRAME_ID) {
		bms_message.bms_fault = (data[0] & 0x01) == 0x01;
		bms_message.imd_fault = (data[0] & 0x02) == 0x02;
	}
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool FEB_CAN_BMS_is_stale(){
	if(HAL_GetTick() - bms_message.last_message_time >= BMS_TIMEOUT ){
		return true;
	}
	return false;
}



