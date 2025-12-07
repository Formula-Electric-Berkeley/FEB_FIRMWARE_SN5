#include "FEB_Main.h"
#include "main.h"

#define TESTBENCH 0
#define BMS_TESTING 0

#if TESTBENCH

#if BMS_TESTING

#define BMS_ID FEB_CAN_BMS_STATE_FRAME_ID
#define DASH_ID FEB_CAN_DASH_MESSAGE_FRAME_ID

// *********************** States ***********************
typedef enum {
	FEB_SM_ST_BOOT,
	FEB_SM_ST_LV,
	FEB_SM_ST_ESC,
	FEB_SM_ST_PRECHARGE,
	FEB_SM_ST_ENERGIZED,
	FEB_SM_ST_DRIVE,
	FEB_SM_ST_FREE,
	FEB_SM_ST_CHARGING,
	FEB_SM_ST_BALANCE,
	FEB_SM_ST_FAULT_BMS,
	FEB_SM_ST_FAULT_BSPD,
	FEB_SM_ST_FAULT_IMD,
	FEB_SM_ST_FAULT_CHARGING,
	FEB_SM_ST_DEFAULT
} FEB_SM_ST_t;

static uint64_t ten_sec_timer = 0;

static FEB_SM_ST_t bms_state = FEB_SM_ST_PRECHARGE;

#else

#define BOARD 4

#if BOARD == 1
#define THIS_ID FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID
#define OTHER_ID FEB_CAN_FEB_PING_PONG_COUNTER2_FRAME_ID
#elif BOARD == 2
#define THIS_ID FEB_CAN_FEB_PING_PONG_COUNTER2_FRAME_ID
#define OTHER_ID FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID
#elif BOARD == 3
#define THIS_ID FEB_CAN_FEB_PING_PONG_COUNTER3_FRAME_ID
#define OTHER_ID FEB_CAN_FEB_PING_PONG_COUNTER4_FRAME_ID
#elif BOARD == 4
#define THIS_ID FEB_CAN_FEB_PING_PONG_COUNTER4_FRAME_ID
#define OTHER_ID FEB_CAN_FEB_PING_PONG_COUNTER3_FRAME_ID
#endif

static uint64_t rx_tx_data = (BOARD > 2) ? 0xFFFFFFFF : 0;

#endif

static CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;

static uint32_t FEB_CAN_Tx_Mailbox;

#endif

extern I2C_HandleTypeDef hi2c1;
extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim1;

extern FEB_CAN_APPS_Message_t FEB_CAN_APPS_Message;

static void FEB_Compose_CAN_Data(void);
static void FEB_Variable_Conversion(void);
static void FEB_Variable_Init(void);

// Stores TPS2482 configurations
/* 		LV - Low voltage bus
* 		CP - Coolant Pump
* 		AF - Accumulator Fans
* 		RF - Radiator Fans
* 		SH - Shutdown Source
* 		L - LIDAR
* 		AS - Autonomous Steering
* 		AB - Autonomous Braking
*/
uint8_t tps2482_i2c_addresses[NUM_TPS2482];
uint16_t tps2482_ids[NUM_TPS2482];

TPS2482_Configuration tps2482_configurations[NUM_TPS2482];
TPS2482_Configuration *lv_config = &tps2482_configurations[0];
TPS2482_Configuration *cp_config = &tps2482_configurations[1];
TPS2482_Configuration *af_config = &tps2482_configurations[2];
TPS2482_Configuration *rf_config = &tps2482_configurations[3];
TPS2482_Configuration *sh_config = &tps2482_configurations[4];
TPS2482_Configuration *l_config = &tps2482_configurations[5];
TPS2482_Configuration *as_config = &tps2482_configurations[6];
TPS2482_Configuration *ab_config = &tps2482_configurations[7];

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

void FEB_Main_Setup(void) {
	FEB_Variable_Init();
	FEB_CAN_HEARTBEAT_Init();

#if !TESTBENCH

	bool tps2482_init_res[NUM_TPS2482];
	bool tps2482_init_success = false;
	int maxiter = 0; // Safety in case of infinite while

	while ( !tps2482_init_success ) {
		if ( maxiter > 100 ) {
			break; // Todo add failure case
		}

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

	bool tps2482_en_res[NUM_TPS2482 - 1];
	bool tps2482_en_success = false;
	GPIO_PinState tps2482_pg_res[NUM_TPS2482];
	bool tps2482_pg_success = false;
	maxiter = 0; // Safety in case of infinite while

	uint8_t start_en[NUM_TPS2482 - 1] = {0, 0, 0, 1, 0, 0, 0};

	while ( !tps2482_en_success || !tps2482_pg_success ) {
		if ( maxiter > 100 ) {
			break; // Todo add failure case
		}

		// Assume successful enable
		bool b1 = true;
		bool b2 = true;

		TPS2482_Enable(tps2482_en_ports, tps2482_en_pins, start_en, tps2482_en_res, NUM_TPS2482 - 1);
		TPS2482_GPIO_Read(tps2482_pg_ports, tps2482_pg_pins, tps2482_pg_res, NUM_TPS2482);

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
	HAL_GPIO_WritePin(BL_SWITCH_GPIO_Port, BL_SWITCH_Pin, GPIO_PIN_RESET);

#endif

	FEB_CAN_Init(FEB_CAN1_Rx_Callback);

	HAL_TIM_Base_Start_IT(&htim1);
}

void FEB_Main_Loop(void) {

}

void FEB_1ms_Callback(void) {
#if !TESTBENCH

	TPS2482_Poll_Current(&hi2c1, tps2482_i2c_addresses, tps2482_current_raw, NUM_TPS2482);
	TPS2482_Poll_Bus_Voltage(&hi2c1, tps2482_i2c_addresses, tps2482_bus_voltage_raw, NUM_TPS2482);
//	TPS2482_Poll_Shunt_Voltage(&hi2c1, tps2482_i2c_addresses, tps2482_shunt_voltage_raw, NUM_TPS2482);

	FEB_Variable_Conversion();

	FEB_Compose_CAN_Data();

	for ( uint8_t i = 0; i < 3; i++ ) {
		can_data.flags &= 0xF0FFFFFF;
		can_data.flags |= ((uint32_t)i) << 24;
		FEB_CAN_Transmit(&hcan1, &can_data);
	}
#endif

#if TESTBENCH
	#if BMS_TESTING
		++ten_sec_timer;

		if (ten_sec_timer == 10) {
			bms_state = FEB_SM_ST_ENERGIZED;
		}

		// Initialize Transmission Header
		FEB_CAN_Tx_Header.StdId = BMS_ID;
		FEB_CAN_Tx_Header.IDE = CAN_ID_STD;
		FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
		FEB_CAN_Tx_Header.DLC = 2;

		// Delay until mailbox available
		while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {}

		// Add Tx data to mailbox
		if (HAL_CAN_AddTxMessage(&hcan1, &FEB_CAN_Tx_Header, (uint8_t *)&bms_state, &FEB_CAN_Tx_Mailbox) != HAL_OK) {
			// Code Error - Shutdown
		}

		char buf[1024];

		size_t len = sprintf(buf, "BOARD: %d, STATE: %u\n\r", BMS_ID, bms_state);

		HAL_UART_Transmit(&huart2, (const uint8_t *)buf, len, HAL_MAX_DELAY);
	#else

		#if BOARD == 1 || BOARD == 3

		// Initialize Transmission Header
		FEB_CAN_Tx_Header.StdId = THIS_ID;
		FEB_CAN_Tx_Header.IDE = CAN_ID_STD;
		FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
		FEB_CAN_Tx_Header.DLC = 8;

		// Delay until mailbox available
		while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {}

		// Add Tx data to mailbox
		if (HAL_CAN_AddTxMessage(&hcan1, &FEB_CAN_Tx_Header, (uint8_t *) &rx_tx_data, &FEB_CAN_Tx_Mailbox) != HAL_OK) {
			// Code Error - Shutdown
		}

		char buf[1024];

		size_t len = sprintf(buf, "BOARD: %d, DATA: %lu\n\r", THIS_ID, (uint32_t)rx_tx_data);

		HAL_UART_Transmit(&huart2, (const uint8_t *)buf, len, HAL_MAX_DELAY);

		#endif
	#endif

#endif
}

void FEB_CAN1_Rx_Callback(CAN_RxHeaderTypeDef *rx_header, void *data) {
	data = (char *)data;

	#if TESTBENCH && !BMS_TESTING

	if ( rx_header->StdId == OTHER_ID ) {
		rx_tx_data = *((uint64_t *) data);

		#if BOARD == 2 || BOARD == 4

		rx_tx_data = (BOARD > 2) ? rx_tx_data - 1 : rx_tx_data + 1;

		// Initialize Transmission Header
		FEB_CAN_Tx_Header.StdId = THIS_ID;
		FEB_CAN_Tx_Header.IDE = CAN_ID_STD;
		FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
		FEB_CAN_Tx_Header.DLC = 8;

		// Delay until mailbox available
		while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {}

		// Add Tx data to mailbox
		if (HAL_CAN_AddTxMessage(&hcan1, &FEB_CAN_Tx_Header, (uint8_t *) &rx_tx_data, &FEB_CAN_Tx_Mailbox) != HAL_OK) {
			// Code Error - Shutdown
		}

		#endif
	}

	#elif BMS_TESTING

	if ( rx_header->StdId == DASH_ID ) {
		bool drive_request = (((uint8_t *)data)[0] >> 1) & 0x01;

		if ( bms_state == FEB_SM_ST_ENERGIZED && drive_request ) {
			bms_state = FEB_SM_ST_DRIVE;
		}
		else if ( bms_state == FEB_SM_ST_DRIVE && drive_request ) {
			bms_state = FEB_SM_ST_ENERGIZED;
		}
	}

	#else

	if ( rx_header->StdId == FEB_CAN_BRAKE_FRAME_ID ) {
		uint8_t brake_pressure = *((uint8_t *)data);

		if ( brake_pressure > FEB_BREAK_THRESHOLD ) {
			HAL_GPIO_WritePin(BL_SWITCH_GPIO_Port, BL_SWITCH_Pin, GPIO_PIN_SET);
		}
		else {
			HAL_GPIO_WritePin(BL_SWITCH_GPIO_Port, BL_SWITCH_Pin, GPIO_PIN_RESET);
		}
	}

	if ( rx_header->StdId == FEB_CAN_DASH_IO_FRAME_ID ) {
		uint8_t dash_data = *((uint8_t *)data);
		bool cp_en = (dash_data >> 5) & 0x01;
		bool rf_en = (dash_data >> 6) & 0x01;
		bool af_en = (dash_data >> 7) & 0x01;
		bool as_en = af_en;

		// Read LV bus voltage
		float lv_voltage = tps2482_bus_voltage[0] / 1000.0f;

		if (lv_voltage < 21.0f) {
			bus_voltage_healthy = false;
		}

		// Only enable Accumulator Fans if accum over certain temp threshold and bus voltage is over 23. 
		af_en = (af_en && (bms_message.max_acc_temp > 40.0f) && bus_voltage_healthy); // back turns on when 35*C
		as_en = (as_en && (bms_message.max_acc_temp > 50.0f) && bus_voltage_healthy); // front turns on when 45*C

		// Only enable Coolant Pump and Rad Fans if bus voltage is over 23. 
		cp_en = cp_en && bus_voltage_healthy;
		rf_en = rf_en && bus_voltage_healthy;

		bool tps2482_en_res[NUM_TPS2482 - 1];
		GPIO_PinState tps2482_en_initial[NUM_TPS2482 - 1];
		bool tps2482_en_success = true; // Assume successful enable
		GPIO_PinState tps2482_pg_res[NUM_TPS2482];
		bool tps2482_pg_success = true; // Assume successful enable

		TPS2482_GPIO_Read(tps2482_pg_ports, tps2482_pg_pins, tps2482_pg_res, NUM_TPS2482);
		TPS2482_GPIO_Read(tps2482_en_ports, tps2482_en_pins, tps2482_en_initial, NUM_TPS2482 - 1);

		for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
			// If any don't enable properly b will be false and thus loop continues
			tps2482_pg_success &= (tps2482_pg_res[i] == (bool)tps2482_en_initial[i]);
		}

		if ( !tps2482_pg_success ) {
			// Todo: Error State
		}

		// Todo: have this check that nothing has changed since last send
		tps2482_en_initial[0] = (GPIO_PinState)cp_en;
		tps2482_en_initial[1] = (GPIO_PinState)af_en;
		tps2482_en_initial[2] = (GPIO_PinState)rf_en;
		tps2482_en_initial[5] = (GPIO_PinState)as_en;

		TPS2482_Enable(tps2482_en_ports, tps2482_en_pins, tps2482_en_initial, tps2482_en_res, NUM_TPS2482 - 1);

		for ( uint8_t i = 0; i < NUM_TPS2482 - 1; i++ ) {
			// If any don't enable properly b will be false and thus loop continues
			tps2482_en_success &= (tps2482_en_res[i] == tps2482_en_initial[i]);
		}

		if ( !tps2482_en_success ) {
			// Todo: Error State
		}
	}

	if ( rx_header->StdId == FEB_CAN_BMS_STATE_FRAME_ID ) {
		uint8_t rx_data[rx_header->DLC];

			memcpy(rx_data, data, rx_header->DLC);

			if ( ((rx_data[0] & 0x1F) == FEB_SM_ST_HEALTH_CHECK) || (((rx_data[0] & 0xE0) >> 5) == FEB_HB_LVPDB) ) {
				// FEB_CAN_HEARTBEAT_Transmit();
			}
	}

	if (rx_header->StdId == FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID ) { 
		uint8_t rx_data[rx_header->DLC];
		bms_message.max_acc_temp = (rx_data[5] << 8) | rx_data[4];
	}

	#endif
}

static void FEB_Compose_CAN_Data(void) {
	memset(&can_data, 0, sizeof(FEB_LVPDB_CAN_Data));

	can_data.ids[0] = FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID;
	can_data.ids[1] = FEB_CAN_LVPDB_COOLANT_FANS_SHUTDOWN_FRAME_ID;
	can_data.ids[2] = FEB_CAN_LVPDB_AUTONOMOUS_FRAME_ID;

#if !TESTBENCH
	can_data.bus_voltage = tps2482_bus_voltage[0];

	memcpy(&can_data.lv_current, tps2482_current, NUM_TPS2482 * sizeof(uint16_t));
#endif
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
//		tps2482_shunt_voltage[i] = (SIGN_MAGNITUDE(tps2482_shunt_voltage_raw[i]) * TPS2482_CONV_VSHUNT);
	}

	tps2482_current[0] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[0]) * LV_CURRENT_LSB);
	tps2482_current[1] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[1]) * CP_CURRENT_LSB);
	tps2482_current[2] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[2]) * AF_CURRENT_LSB);
	tps2482_current[3] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[3]) * RF_CURRENT_LSB);
	tps2482_current[4] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[4]) * SH_CURRENT_LSB);
	tps2482_current[5] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[5]) * L_CURRENT_LSB);
	tps2482_current[6] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[6]) * AS_CURRENT_LSB);
	tps2482_current[7] = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(tps2482_current_raw[7]) * AB_CURRENT_LSB);

	FEB_Current_IIR(tps2482_current, tps2482_current, tps2482_current_filter, NUM_TPS2482, tps2482_current_filter_init);
}

static void FEB_Variable_Init(void) {
	tps2482_i2c_addresses[0] = LV_ADDR;
	tps2482_i2c_addresses[1] = CP_ADDR;
	tps2482_i2c_addresses[2] = AF_ADDR;
	tps2482_i2c_addresses[3] = RF_ADDR;
	tps2482_i2c_addresses[4] = SH_ADDR;
	tps2482_i2c_addresses[5] = L_ADDR;
	tps2482_i2c_addresses[6] = AS_ADDR;
	tps2482_i2c_addresses[7] = AB_ADDR;

	for ( uint8_t i = 0; i < NUM_TPS2482; i++ ) {
		tps2482_configurations[i].config = TPS2482_CONFIG_DEFAULT;
		tps2482_configurations[i].mask = TPS2482_MASK_SOL;
	}

	lv_config->cal = LV_CAL_VAL;
	cp_config->cal = CP_CAL_VAL;
	af_config->cal = AF_CAL_VAL;
	rf_config->cal = RF_CAL_VAL;
	sh_config->cal = SH_CAL_VAL;
	l_config->cal = L_CAL_VAL;
	as_config->cal = AS_CAL_VAL;
	ab_config->cal = AB_CAL_VAL;

	lv_config->alert_lim = LV_ALERT_LIM_VAL;
	cp_config->alert_lim = CP_ALERT_LIM_VAL;
	af_config->alert_lim = AF_ALERT_LIM_VAL;
	rf_config->alert_lim = RF_ALERT_LIM_VAL;
	sh_config->alert_lim = SH_ALERT_LIM_VAL;
	l_config->alert_lim = L_ALERT_LIM_VAL;
	as_config->alert_lim = AS_ALERT_LIM_VAL;
	ab_config->alert_lim = AB_ALERT_LIM_VAL;

	tps2482_en_ports[0] = CP_EN_GPIO_Port;
	tps2482_en_ports[1] = AF_EN_GPIO_Port;
	tps2482_en_ports[2] = RF_EN_GPIO_Port;
	tps2482_en_ports[3] = SH_EN_GPIO_Port;
	tps2482_en_ports[4] = L_EN_GPIO_Port;
	tps2482_en_ports[5] = AS_EN_GPIO_Port;
	tps2482_en_ports[6] = AB_EN_GPIO_Port;

	tps2482_en_pins[0] = CP_EN_Pin;
	tps2482_en_pins[1] = AF_EN_Pin;
	tps2482_en_pins[2] = RF_EN_Pin;
	tps2482_en_pins[3] = SH_EN_Pin;
	tps2482_en_pins[4] = L_EN_Pin;
	tps2482_en_pins[5] = AS_EN_Pin;
	tps2482_en_pins[6] = AB_EN_Pin;

	tps2482_pg_ports[0] = LV_PG_GPIO_Port;
	tps2482_pg_ports[1] = CP_PG_GPIO_Port;
	tps2482_pg_ports[2] = AF_PG_GPIO_Port;
	tps2482_pg_ports[3] = RF_PG_GPIO_Port;
	tps2482_pg_ports[4] = SH_PG_GPIO_Port;
	tps2482_pg_ports[5] = L_PG_GPIO_Port;
	tps2482_pg_ports[6] = AS_PG_GPIO_Port;
	tps2482_pg_ports[7] = AB_PG_GPIO_Port;

	tps2482_pg_pins[0] = LV_PG_Pin;
	tps2482_pg_pins[1] = CP_PG_Pin;
	tps2482_pg_pins[2] = AF_PG_Pin;
	tps2482_pg_pins[3] = RF_PG_Pin;
	tps2482_pg_pins[4] = SH_PG_Pin;
	tps2482_pg_pins[5] = L_PG_Pin;
	tps2482_pg_pins[6] = AS_PG_Pin;
	tps2482_pg_pins[7] = AB_PG_Pin;

	tps2482_alert_ports[0] = LV_ALERT_GPIO_Port;
	tps2482_alert_ports[1] = CP_ALERT_GPIO_Port;
	tps2482_alert_ports[2] = AF_ALERT_GPIO_Port;
	tps2482_alert_ports[3] = RF_ALERT_GPIO_Port;
	tps2482_alert_ports[4] = SH_ALERT_GPIO_Port;
	tps2482_alert_ports[5] = L_ALERT_GPIO_Port;
	tps2482_alert_ports[6] = AS_ALERT_GPIO_Port;
	tps2482_alert_ports[7] = AB_ALERT_GPIO_Port;

	tps2482_alert_pins[0] = LV_ALERT_Pin;
	tps2482_alert_pins[1] = CP_ALERT_Pin;
	tps2482_alert_pins[2] = AF_ALERT_Pin;
	tps2482_alert_pins[3] = RF_ALERT_Pin;
	tps2482_alert_pins[4] = SH_ALERT_Pin;
	tps2482_alert_pins[5] = L_ALERT_Pin;
	tps2482_alert_pins[6] = AS_ALERT_Pin;
	tps2482_alert_pins[7] = AB_ALERT_Pin;

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
