/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Thermocouple3_Pin GPIO_PIN_0
#define Thermocouple3_GPIO_Port GPIOC
#define Thermocouple2_Pin GPIO_PIN_1
#define Thermocouple2_GPIO_Port GPIOC
#define Thermocouple1_Pin GPIO_PIN_2
#define Thermocouple1_GPIO_Port GPIOC
#define LP_Wiper1_Pin GPIO_PIN_3
#define LP_Wiper1_GPIO_Port GPIOC
#define SG1_Pin GPIO_PIN_7
#define SG1_GPIO_Port GPIOA
#define SG2_Pin GPIO_PIN_4
#define SG2_GPIO_Port GPIOC
#define SG3_Pin GPIO_PIN_5
#define SG3_GPIO_Port GPIOC
#define SG4_Pin GPIO_PIN_0
#define SG4_GPIO_Port GPIOB
#define LP_Wiper2_Pin GPIO_PIN_1
#define LP_Wiper2_GPIO_Port GPIOB
#define IMU_INT2_Pin GPIO_PIN_2
#define IMU_INT2_GPIO_Port GPIOB
#define DRDY_Pin GPIO_PIN_14
#define DRDY_GPIO_Port GPIOB
#define INTM_Pin GPIO_PIN_15
#define INTM_GPIO_Port GPIOB
#define IMU_INT1_Pin GPIO_PIN_9
#define IMU_INT1_GPIO_Port GPIOA
#define PGO_Pin GPIO_PIN_12
#define PGO_GPIO_Port GPIOC
#define GPS_EN_Pin GPIO_PIN_2
#define GPS_EN_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
