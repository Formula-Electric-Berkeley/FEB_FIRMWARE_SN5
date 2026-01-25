#include "FEB_Main.h"
#include "main.h"
#include <stdio.h>

static CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;

static uint32_t FEB_CAN_Tx_Mailbox;

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim1;
extern UART_HandleTypeDef huart2;

static void FEB_Compose_CAN_Data(void);
static void FEB_Variable_Conversion(void);
static void FEB_Variable_Init(void);

/* Stores TPS2482 configurations
 * The LVPDB has multiple TPS chips on the bus. These are the addresses of
 * each of the TPS chips. The naming conventions is as follows:
 *		LV - Low Voltage Source (scl-sda)
 *		SH - Shutdown Source (sda-sda)
 *		LT - Laptop Branch (gnd-gnd)
 *		BM_L - Braking Servo, Lidar (gnd-scl) 
 *		SM - Steering Motor (gnd-sda) 
 *		AF1_AF2 - Accumulator Fans 1 Branch (gnd-vs)
 *		CP_RF - Coolant Pump + Radiator Fans Branch (vs-scl)
 */
uint8_t tps2482_i2c_addresses[NUM_TPS2482];
uint16_t tps2482_ids[NUM_TPS2482];

TPS2482_Configuration tps2482_configurations[NUM_TPS2482];
TPS2482_Configuration *lv_config = &tps2482_configurations[0];
TPS2482_Configuration *sh_config = &tps2482_configurations[1];
TPS2482_Configuration *lt_config = &tps2482_configurations[2];
TPS2482_Configuration *bm_l_config = &tps2482_configurations[3];
TPS2482_Configuration *sm_config = &tps2482_configurations[4];
TPS2482_Configuration *af1_af2_config = &tps2482_configurations[5];
TPS2482_Configuration *cp_rf_config = &tps2482_configurations[6];

GPIO_TypeDef *tps2482_en_ports[NUM_TPS2482 - 1]; // LV doesn't have an EN pin
uint16_t tps2482_en_pins[NUM_TPS2482 - 1]; // LV doesn't have an EN pin

GPIO_TypeDef *tps2482_pg_ports[NUM_TPS2482];
uint16_t tps2482_pg_pins[NUM_TPS2482];

GPIO_TypeDef *tps2482_alert_ports[NUM_TPS2482];
uint16_t tps2482_alert_pins[NUM_TPS2482];

uint16_t tps2482_current_raw[NUM_TPS2482];
uint16_t tps2482_bus_voltage_raw[NUM_TPS2482];
uint16_t tps2482_shunt_voltage_raw[NUM_TPS2482];

int32_t tps2482_current_filter[NUM_TPS2482];
bool tps2482_current_filter_init[NUM_TPS2482];

int16_t tps2482_current[NUM_TPS2482];
uint16_t tps2482_bus_voltage[NUM_TPS2482];
double tps2482_shunt_voltage[NUM_TPS2482];

FEB_LVPDB_CAN_Data can_data;

bool bus_voltage_healthy = true;

// MARK: Main Loop

void FEB_Main_Setup(void) {
	printf("Beginning Setup");

	FEB_Variable_Init();
	// FEB_CAN_HEARTBEAT_Init();

	bool tps2482_init_res[NUM_TPS2482];
	bool tps2482_init_success = false;
	int maxiter = 0; // Safety in case of infinite while

	while ( !tps2482_init_success ) {
		if ( maxiter > 100 ) {
			break; // Todo add failure case
		}

		printf("Initializing... %d. Status: %d", maxiter, tps2482_init_success);

		// Assume successful init
		bool b = 0x01;

		TPS2482_Init(&hi2c1, tps2482_i2c_addresses, tps2482_configurations, tps2482_ids, tps2482_init_res, NUM_TPS2482);

		for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
			// If any don't enable properly b will be false and thus loop continues
			b &= tps2482_init_res[i];
		}

		tps2482_init_success = b;
		maxiter += 1;
	}

	printf("tps2482_init_success: %d\r\n", tps2482_init_success);

	bool tps2482_en_res[NUM_TPS2482 - 1]; // LVPDB is always enabled so num TPS - 1
	bool tps2482_en_success = false;
	GPIO_PinState tps2482_pg_res[NUM_TPS2482];
	bool tps2482_pg_success = false;
	maxiter = 0; // Safety in case of infinite while

	uint8_t start_en[NUM_TPS2482 - 1] = {1, 1, 1, 1, 1, 1};

	while ( !tps2482_en_success || !tps2482_pg_success ) {
		if ( maxiter > 100 ) {
			break; // Todo add failure case
		}
		// Assume successful enable
		bool b1 = true;
		bool b2 = true;

		TPS2482_Enable(tps2482_en_ports, tps2482_en_pins, start_en, tps2482_en_res, NUM_TPS2482 - 1);
		TPS2482_GPIO_Read(tps2482_pg_ports, tps2482_pg_pins, tps2482_pg_res, NUM_TPS2482);

		printf("%d tps2482_en_res: %d, %d, %d, %d, %d, %d\r\n", maxiter, tps2482_en_res[0], tps2482_en_res[1], tps2482_en_res[2], tps2482_en_res[3], tps2482_en_res[4], tps2482_en_res[5]);
		printf("%d tps2482_pg_res: %d, %d, %d, %d, %d, %d, %d\r\n", maxiter, tps2482_pg_res[0], tps2482_pg_res[1], tps2482_pg_res[2], tps2482_pg_res[3], tps2482_pg_res[4], tps2482_pg_res[5], tps2482_pg_res[6]);

		for ( uint8_t i = 0; i < NUM_TPS2482 - 1; i++ ) {
			// If any don't enable properly b will be false and thus loop continues
			b1 &= (tps2482_en_res[i] == start_en[i]);
		}

		for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
			// If any don't power up properly b will be false and thus loop continues

			if ( i == 0 ) {
				b2 &= tps2482_pg_res[i];
			}
			else {
				b2 &= (tps2482_pg_res[i] ==  start_en[i - 1]);
			}
		}

		tps2482_en_success = b1;
		tps2482_pg_success = b2;
		maxiter += 1;
	}

	// Initialize brake light to be off
	HAL_GPIO_WritePin(BL_Switch_GPIO_Port, BL_Switch_Pin, GPIO_PIN_RESET);

	FEB_CAN_Init(FEB_CAN1_Rx_Callback);

	HAL_TIM_Base_Start_IT(&htim1);
}

void FEB_Main_Loop(void) {

}

void FEB_1ms_Callback(void) {
	TPS2482_Poll_Current(&hi2c1, tps2482_i2c_addresses, tps2482_current_raw, NUM_TPS2482);
	TPS2482_Poll_Bus_Voltage(&hi2c1, tps2482_i2c_addresses, tps2482_bus_voltage_raw, NUM_TPS2482);
	TPS2482_Poll_Shunt_Voltage(&hi2c1, tps2482_i2c_addresses, tps2482_shunt_voltage_raw, NUM_TPS2482);

	FEB_Variable_Conversion();

	// FEB_Compose_CAN_Data();

	// for ( uint8_t i = 0; i < 3; i++ ) {
	// 	can_data.flags &= 0xF0FFFFFF;
	// 	can_data.flags |= ((uint32_t)i) << 24;
	// 	FEB_CAN_Transmit(&hcan1, &can_data);
	// }
}

void FEB_CAN1_Rx_Callback(CAN_RxHeaderTypeDef *rx_header, void *data) {
	data = (char *)data;

	// if ( rx_header->StdId == FEB_CAN_BRAKE_FRAME_ID ) {
	// 	uint8_t brake_pressure = *((uint8_t *)data);

	// 	if ( brake_pressure > FEB_BREAK_THRESHOLD ) {
	// 		HAL_GPIO_WritePin(BL_SWITCH_GPIO_Port, BL_SWITCH_Pin, GPIO_PIN_SET);
	// 	}
	// 	else {
	// 		HAL_GPIO_WritePin(BL_SWITCH_GPIO_Port, BL_SWITCH_Pin, GPIO_PIN_RESET);
	// 	}
	// }

	// if ( rx_header->StdId == FEB_CAN_DASH_IO_FRAME_ID ) {
	// 	uint8_t dash_data = *((uint8_t *)data);
	// 	bool cp_en = (dash_data >> 5) & 0x01;
	// 	bool rf_en = (dash_data >> 6) & 0x01;
	// 	bool af_en = (dash_data >> 7) & 0x01;
	// 	bool as_en = af_en;

	// 	// Read LV bus voltage
	// 	float lv_voltage = tps2482_bus_voltage[0] / 1000.0f;

	// 	if (lv_voltage < 21.0f) {
	// 		bus_voltage_healthy = false;
	// 	}

	// 	// Only enable Accumulator Fans if accum over certain temp threshold and bus voltage is over 23. 
	// 	af_en = (af_en && (bms_message.max_acc_temp > 40.0f) && bus_voltage_healthy); // back turns on when 35*C
	// 	as_en = (as_en && (bms_message.max_acc_temp > 50.0f) && bus_voltage_healthy); // front turns on when 45*C

	// 	// Only enable Coolant Pump and Rad Fans if bus voltage is over 23. 
	// 	cp_en = cp_en && bus_voltage_healthy;
	// 	rf_en = rf_en && bus_voltage_healthy;

	// 	bool tps2482_en_res[NUM_TPS2482 - 1];
	// 	GPIO_PinState tps2482_en_initial[NUM_TPS2482 - 1];
	// 	bool tps2482_en_success = true; // Assume successful enable
	// 	GPIO_PinState tps2482_pg_res[NUM_TPS2482];
	// 	bool tps2482_pg_success = true; // Assume successful enable

	// 	TPS2482_GPIO_Read(tps2482_pg_ports, tps2482_pg_pins, tps2482_pg_res, NUM_TPS2482);
	// 	TPS2482_GPIO_Read(tps2482_en_ports, tps2482_en_pins, tps2482_en_initial, NUM_TPS2482 - 1);

	// 	for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
	// 		// If any don't enable properly b will be false and thus loop continues
	// 		tps2482_pg_success &= (tps2482_pg_res[i] == (bool)tps2482_en_initial[i]);
	// 	}

	// 	if ( !tps2482_pg_success ) {
	// 		// Todo: Error State
	// 	}

	// 	// Todo: have this check that nothing has changed since last send
	// 	tps2482_en_initial[0] = (GPIO_PinState)cp_en;
	// 	tps2482_en_initial[1] = (GPIO_PinState)af_en;
	// 	tps2482_en_initial[2] = (GPIO_PinState)rf_en;
	// 	tps2482_en_initial[5] = (GPIO_PinState)as_en;

	// 	TPS2482_Enable(tps2482_en_ports, tps2482_en_pins, tps2482_en_initial, tps2482_en_res, NUM_TPS2482 - 1);

	// 	for ( uint8_t i = 0; i < NUM_TPS2482 - 1; i++ ) {
	// 		// If any don't enable properly b will be false and thus loop continues
	// 		tps2482_en_success &= (tps2482_en_res[i] == tps2482_en_initial[i]);
	// 	}

	// 	if ( !tps2482_en_success ) {
	// 		// Todo: Error State
	// 	}
	// }

	// if ( rx_header->StdId == FEB_CAN_BMS_STATE_FRAME_ID ) {
	// 	uint8_t rx_data[rx_header->DLC];

	// 		memcpy(rx_data, data, rx_header->DLC);

	// 		if ( ((rx_data[0] & 0x1F) == FEB_SM_ST_HEALTH_CHECK) || (((rx_data[0] & 0xE0) >> 5) == FEB_HB_LVPDB) ) {
	// 			// FEB_CAN_HEARTBEAT_Transmit();
	// 		}
	// }

	// if (rx_header->StdId == FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID ) { 
	// 	uint8_t rx_data[rx_header->DLC];
	// 	bms_message.max_acc_temp = (rx_data[5] << 8) | rx_data[4];
	// }
}

static void FEB_Compose_CAN_Data(void) {
	memset(&can_data, 0, sizeof(FEB_LVPDB_CAN_Data));

	// can_data.ids[0] = FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID;
	// can_data.ids[1] = FEB_CAN_LVPDB_COOLANT_FANS_SHUTDOWN_FRAME_ID;
	// can_data.ids[2] = FEB_CAN_LVPDB_AUTONOMOUS_FRAME_ID;

	// can_data.bus_voltage = tps2482_bus_voltage[0];

	// memcpy(&can_data.lv_current, tps2482_current, NUM_TPS2482 * sizeof(uint16_t));
}

#define ADC_FILTER_EXPONENT 2

static void FEB_Current_IIR(int16_t *data_in, int16_t *data_out, int32_t *filters, \
											uint8_t length, bool *filter_initialized) {
	int16_t *dest = data_out;
	int32_t *dest_filters = filters;

	for ( uint8_t i = 0; i < length; i++ ) {
		if ( !filter_initialized[i] ) {
			dest_filters[i] = data_in[i] << ADC_FILTER_EXPONENT;
			dest[i] = data_in[i];
			filter_initialized[i] = true;
		}
		else {
			dest_filters[i] += data_in[i] - (dest_filters[i] >>  ADC_FILTER_EXPONENT);
			dest[i] = dest_filters[i] >> ADC_FILTER_EXPONENT;
		}
	}
}

static void FEB_Variable_Conversion(void) {
	for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
		tps2482_bus_voltage[i] = FLOAT_TO_UINT16_T(tps2482_bus_voltage_raw[i] * TPS2482_CONV_VBUS);
		tps2482_shunt_voltage[i] = (SIGN_MAGNITUDE(tps2482_shunt_voltage_raw[i]) * TPS2482_CONV_VSHUNT);
	}

	tps2482_current[0] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[0]) * LV_CURRENT_LSB);
	tps2482_current[1] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[1]) * SH_CURRENT_LSB);
	tps2482_current[2] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[2]) * LT_CURRENT_LSB);
	tps2482_current[3] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[3]) * BM_L_CURRENT_LSB);
	tps2482_current[4] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[4]) * SM_CURRENT_LSB);
	tps2482_current[5] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[5]) * AF1_AF2_CURRENT_LSB);
	tps2482_current[6] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[6]) * CP_RF_CURRENT_LSB);

	FEB_Current_IIR(tps2482_current, tps2482_current, tps2482_current_filter, NUM_TPS2482, tps2482_current_filter_init);
}

static void FEB_Variable_Init(void) {
	tps2482_i2c_addresses[0] = LV_ADDR;
	tps2482_i2c_addresses[1] = SH_ADDR;
	tps2482_i2c_addresses[2] = LT_ADDR;
	tps2482_i2c_addresses[3] = BM_L_ADDR;
	tps2482_i2c_addresses[4] = SM_ADDR;
	tps2482_i2c_addresses[5] = AF1_AF2_ADDR;
	tps2482_i2c_addresses[6] = CP_RF_CURRENT_LSB;

	for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
		tps2482_configurations[i].config = TPS2482_CONFIG_DEFAULT;
		tps2482_configurations[i].mask = TPS2482_MASK_SOL;
	}

	lv_config->cal = LV_CAL_VAL;
	sh_config->cal = SH_CAL_VAL;
	lt_config->cal = LT_CAL_VAL;
	bm_l_config->cal = BM_L_CAL_VAL;
	sm_config->cal = SM_CAL_VAL;
	af1_af2_config->cal = AF1_AF2_CAL_VAL;
	cp_rf_config->cal = CP_RF_CAL_VAL;

	lv_config->alert_lim = LV_ALERT_LIM_VAL;
	sh_config->alert_lim = SH_ALERT_LIM_VAL;
	lt_config->alert_lim = LT_ALERT_LIM_VAL;
	bm_l_config->alert_lim = BM_L_ALERT_LIM_VAL;
	sm_config->alert_lim = SM_ALERT_LIM_VAL;
	af1_af2_config->alert_lim = AF1_AF2_ALERT_LIM_VAL;
	cp_rf_config->alert_lim = CP_RF_ALERT_LIM_VAL;

	tps2482_en_ports[0] = SH_EN_GPIO_Port;
	tps2482_en_ports[1] = LT_EN_GPIO_Port;
	tps2482_en_ports[2] = BM_L_EN_GPIO_Port;
	tps2482_en_ports[3] = SM_EN_GPIO_Port;
	tps2482_en_ports[4] = AF1_AF2_EN_GPIO_Port;
	tps2482_en_ports[5] = CP_RF_EN_GPIO_Port;

	tps2482_en_pins[0] = SH_EN_Pin;
	tps2482_en_pins[1] = LT_EN_Pin;
	tps2482_en_pins[2] = BM_L_EN_Pin;
	tps2482_en_pins[3] = SM_EN_Pin;
	tps2482_en_pins[4] = AF1_AF2_EN_Pin;
	tps2482_en_pins[5] = CP_RF_EN_Pin;

	tps2482_pg_ports[0] = LV_PG_GPIO_Port;
	tps2482_pg_ports[1] = SH_PG_GPIO_Port;
	tps2482_pg_ports[2] = LT_PG_GPIO_Port;
	tps2482_pg_ports[3] = BM_L_PG_GPIO_Port;
	tps2482_pg_ports[4] = SM_PG_GPIO_Port;
	tps2482_pg_ports[5] = AF1_AF2_PG_GPIO_Port;
	tps2482_pg_ports[6] = CP_RF_PG_GPIO_Port;

	tps2482_pg_pins[0] = LV_PG_Pin;
	tps2482_pg_pins[1] = SH_PG_Pin;
	tps2482_pg_pins[2] = LT_PG_Pin;
	tps2482_pg_pins[3] = BM_L_PG_Pin;
	tps2482_pg_pins[4] = SM_PG_Pin;
	tps2482_pg_pins[5] = AF1_AF2_PG_Pin;
	tps2482_pg_pins[6] = CP_RF_PG_Pin;

	tps2482_alert_ports[0] = LV_Alert_GPIO_Port;
	tps2482_alert_ports[1] = SH_Alert_GPIO_Port;
	tps2482_alert_ports[2] = LT_Alert_GPIO_Port;
	tps2482_alert_ports[3] = BM_L_Alert_GPIO_Port;
	tps2482_alert_ports[4] = SM_Alert_GPIO_Port;
	tps2482_alert_ports[5] = AF1_AF2_Alert_GPIO_Port;
	tps2482_alert_ports[6] = CP_RF_Alert_GPIO_Port;

	tps2482_alert_pins[0] = LV_Alert_Pin;
	tps2482_alert_pins[1] = SH_Alert_Pin;
	tps2482_alert_pins[2] = LT_Alert_Pin;
	tps2482_alert_pins[3] = BM_L_Alert_Pin;
	tps2482_alert_pins[4] = SM_Alert_Pin;
	tps2482_alert_pins[5] = AF1_AF2_Alert_Pin;
	tps2482_alert_pins[6] = CP_RF_Alert_Pin;

	can_data.ids[0] = FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID;
	can_data.ids[1] = FEB_CAN_LVPDB_COOLANT_FANS_SHUTDOWN_FRAME_ID;
	can_data.ids[2] = FEB_CAN_LVPDB_AUTONOMOUS_FRAME_ID;

	memset(tps2482_current_raw, 0, NUM_TPS2482 * sizeof(uint16_t));
	memset(tps2482_bus_voltage_raw, 0, NUM_TPS2482 * sizeof(uint16_t));
	memset(tps2482_shunt_voltage_raw, 0, NUM_TPS2482 * sizeof(uint16_t));
	memset(tps2482_current, 0, NUM_TPS2482 * sizeof(uint16_t));
	memset(tps2482_bus_voltage, 0, NUM_TPS2482 * sizeof(uint16_t));
	memset(tps2482_shunt_voltage, 0, NUM_TPS2482 * sizeof(uint16_t));

	memset(tps2482_current_filter, 0, NUM_TPS2482 * sizeof(int32_t));
	memset(tps2482_current_filter_init, false, NUM_TPS2482 * sizeof(bool));
}
