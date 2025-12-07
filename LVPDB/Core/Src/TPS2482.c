#include <TPS2482.h>

void TPS2482_Init(I2C_HandleTypeDef *hi2c, uint8_t *addresses, TPS2482_Configuration *configurations, uint16_t *ids, bool *res, uint8_t messageCount) {
	uint16_t configs[messageCount];
	uint16_t cals[messageCount];
	uint16_t masks[messageCount];
	uint16_t alert_limits[messageCount];
	uint16_t configs_res[messageCount];
	uint16_t cals_res[messageCount];
	uint16_t masks_res[messageCount];
	uint16_t alert_limits_res[messageCount];

	memset(res, true, messageCount * sizeof(bool));

	// Extract 16-bit values from configurations
	for ( uint8_t i = 0; i < messageCount; i++ ) {
		configs[i] = configurations[i].config;
		cals[i] = configurations[i].cal;
		masks[i] = configurations[i].mask;
		alert_limits[i] = configurations[i].alert_lim;
	}

	// Write configurations to the TPS2482
	TPS2482_Write_Config(hi2c, addresses, configs, messageCount);
	TPS2482_Write_CAL(hi2c, addresses, cals, messageCount);
	TPS2482_Write_Mask(hi2c, addresses, masks, messageCount);
	TPS2482_Write_Alert_Limit(hi2c, addresses, alert_limits, messageCount);

	HAL_Delay(100);

	// Read back configurations
	TPS2482_Get_Config(hi2c, addresses, configs_res, messageCount);
	TPS2482_Get_CAL(hi2c, addresses, cals_res, messageCount);
	TPS2482_Get_Mask(hi2c, addresses, masks_res, messageCount);
	TPS2482_Get_Alert_Limit(hi2c, addresses, alert_limits_res, messageCount);
	TPS2482_Get_ID(hi2c, addresses, ids, messageCount);

	// Validate configurations read back (config reseting wont trigger error)
	for ( uint8_t i = 0; i < messageCount; i++ ) {
		if ( !TPS2482_CONFIG_RST_MASK(configs[i]) ) {
			res[i] &= configs[i] == configs_res[i];
		}
		res[i] &= cals[i] == cals_res[i];

		uint16_t masks_flagless_mask = TPS2482_MASK_SOL | TPS2482_MASK_SUL | TPS2482_MASK_BOL | \
										TPS2482_MASK_BUL | TPS2482_MASK_CNVR | TPS2482_MASK_POL;
		res[i] &= masks[i] == (masks_res[i] & masks_flagless_mask);
		res[i] &= alert_limits[i] == alert_limits_res[i];
	}
}

void TPS2482_Get_Register(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint8_t reg, uint16_t *results, uint8_t messageCount) {
	uint8_t res[2 * messageCount];

	for ( uint8_t i = 0; i < messageCount; i++ ) {
		if ( HAL_I2C_Mem_Read(hi2c, addresses[i] << 1, reg, I2C_MEMADD_SIZE_8BIT, &res[2*i], sizeof(*results), HAL_MAX_DELAY) != HAL_OK ) {
			results[i] = 0; // ERROR
		}
		else {
			results[i] = ((uint16_t)(res[2 * i]) << 8) | (uint16_t)(res[2 * i + 1]);
		}
	}
}

void TPS2482_Get_Config(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_CONFIG, results, messageCount);
}

void TPS2482_Poll_Shunt_Voltage(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_SHUNT_VOLT, results, messageCount);
}

void TPS2482_Poll_Bus_Voltage(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_BUS_VOLT, results, messageCount);
}

void TPS2482_Poll_Power(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_POWER, results, messageCount);
}

void TPS2482_Poll_Current(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_CURRENT, results, messageCount);
}

void TPS2482_Get_CAL(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_CAL, results, messageCount);
}

void TPS2482_Get_Mask(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_MASK, results, messageCount);
}

void TPS2482_Get_Alert_Limit(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_ALERT_LIM, results, messageCount);
}

void TPS2482_Get_ID(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount) {
	TPS2482_Get_Register(hi2c, addresses, TPS2482_ID, results, messageCount);
}

void TPS2482_Write_Register(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint8_t reg, uint16_t *transmit, uint8_t messageCount) {
	uint8_t trans[2 * messageCount];

	for ( uint8_t i = 0; i < messageCount; i++ ) {
		trans[2 * i] = (uint8_t)((transmit[i] >> 8) & 0xFF);
		trans[2 * i + 1] = (uint8_t)(transmit[i] & 0xFF);

		if ( HAL_I2C_Mem_Write(hi2c, addresses[i] << 1, reg, I2C_MEMADD_SIZE_8BIT, &trans[2 * i], sizeof(uint16_t), HAL_MAX_DELAY) != HAL_OK ) {
			// Todo failure state
		}
	}
}

void TPS2482_Write_Config(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount) {
	TPS2482_Write_Register(hi2c, addresses, TPS2482_CONFIG, transmit, messageCount);
}

void TPS2482_Write_CAL(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount) {
	TPS2482_Write_Register(hi2c, addresses, TPS2482_CAL, transmit, messageCount);
}

void TPS2482_Write_Mask(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount) {
	TPS2482_Write_Register(hi2c, addresses, TPS2482_MASK, transmit, messageCount);
}

void TPS2482_Write_Alert_Limit(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount) {
	TPS2482_Write_Register(hi2c, addresses, TPS2482_ALERT_LIM, transmit, messageCount);
}

void TPS2482_GPIO_Write(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, uint8_t *state, uint8_t messageCount) {
	for ( uint8_t i = 0; i < messageCount; i++ ) {
		HAL_GPIO_WritePin(GPIOx[i], GPIO_Pin[i], state[i]);
	}
}

void TPS2482_GPIO_Read(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, GPIO_PinState *result, uint8_t messageCount) {
	for ( uint8_t i = 0; i < messageCount; i++ ) {
		result[i] = HAL_GPIO_ReadPin(GPIOx[i], GPIO_Pin[i]);
	}
}

void TPS2482_Enable(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, uint8_t *en_dis, bool *result, uint8_t messageCount) {
	TPS2482_GPIO_Write(GPIOx, GPIO_Pin, en_dis, messageCount);

//	HAL_Delay(100);

	TPS2482_GPIO_Read(GPIOx, GPIO_Pin, (GPIO_PinState *)result, messageCount);
}

//void FEB_TPS2482_shutdownIfError(I2C_HandleTypeDef * hi2c, uint8_t DEV_ADDR, GPIO_TypeDef * EN, uint16_t EN_NUM, GPIO_TypeDef * AL, uint16_t AL_NUM,
//		GPIO_TypeDef * PG, uint16_t PG_NUM, float VMAX, float VMIN, float IMAX, float IMIN, float PMAX, float PMIN) {
//
//	// pull EN low if
//		// 1. alert pin pulled low
//		// 2. PG goes low while EN is high
//		// 3. shunt voltage, bus voltage, power, or current out of bounds
//
//	// 1.
//	if (HAL_GPIO_ReadPin(AL, AL_NUM) == GPIO_PIN_RESET) {
//		HAL_GPIO_WritePin(EN, EN_NUM, GPIO_PIN_RESET);
//	}
//	// 2.
//	if ((HAL_GPIO_ReadPin(EN, EN_NUM) == GPIO_PIN_SET) && (HAL_GPIO_ReadPin(PG, PG_NUM) == GPIO_PIN_RESET)) {
//		HAL_GPIO_WritePin(EN, EN_NUM, GPIO_PIN_RESET);
//	}
//	// 3.
//	FEB_TPS2482_pullLowIfOutOfBounds(hi2c, DEV_ADDR, EN, EN_NUM, VMAX, VMIN, 0x01); // shunt voltage
//	FEB_TPS2482_pullLowIfOutOfBounds(hi2c, DEV_ADDR, EN, EN_NUM, VMAX, VMIN, 0x02); // bus voltage
//	FEB_TPS2482_pullLowIfOutOfBounds(hi2c, DEV_ADDR, EN, EN_NUM, PMAX, PMIN, 0x03); // power
//	FEB_TPS2482_pullLowIfOutOfBounds(hi2c, DEV_ADDR, EN, EN_NUM, IMAX, IMIN, 0x04); // current
//
//}
//
////TODO: Modify to accomodate other sensing types, right now its only good for shunt voltage
//void FEB_TPS2482_pullLowIfOutOfBounds(I2C_HandleTypeDef * hi2c, uint8_t DEV_ADDR, GPIO_TypeDef * EN, uint16_t EN_NUM, float MAX, float MIN,
//		uint8_t REG) {
//	uint8_t buf[12];
//	buf[0] = REG;
//	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hi2c, DEV_ADDR, buf, 1, 100);
//	if (ret != HAL_OK) {
//		HAL_GPIO_WritePin(EN, EN_NUM, GPIO_PIN_RESET); // pull EN low
//	} else {
//		ret = HAL_I2C_Master_Receive(hi2c, DEV_ADDR, buf, 2, 100);
//		if (ret != HAL_OK) {
//			HAL_GPIO_WritePin(EN, EN_NUM, GPIO_PIN_RESET); // pull EN low
//		} else {
//			int val = ((int16_t)buf[0] << 4) | (buf[1] >> 4); // combine the 2 bytes
//			val = val - 1;
//			val |= 0xF000; // subtract 1 and take complement
//			float parsed = val * 0.0000025;// convert to decimal and multiply by 2.5uV
//			if (parsed > MAX || parsed < MIN) {
//				HAL_GPIO_WritePin(EN, EN_NUM, GPIO_PIN_RESET);
//			}
//		}
//	}
//}
//

