/**
 ******************************************************************************
 * @file           : FEB_CAN_LVPDB.h
 * @brief          : CAN LVPDB Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

static void rx_callback_lv_temperature(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       const uint8_t *data, uint8_t length, void *user_data);
