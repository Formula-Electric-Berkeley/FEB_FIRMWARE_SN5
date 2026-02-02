/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bms_tasks.h"
#include "i2c.h"
#include "TPS2482.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osMutexId_t FEB_I2C_MutexHandle;
const osMutexAttr_t FEB_I2C_Mutex_attributes = {
  .name = "FEB_I2C_Mutex"
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ADBMSTask */
osThreadId_t ADBMSTaskHandle;
const osThreadAttr_t ADBMSTask_attributes = {
  .name = "ADBMSTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for bomb_squad_tps */
osThreadId_t bomb_squad_tpsHandle;
const osThreadAttr_t bomb_squad_tps_attributes = {
  .name = "bomb_squad_tps",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ADBMSMutex */
osMutexId_t ADBMSMutexHandle;
const osMutexAttr_t ADBMSMutex_attributes = {
  .name = "ADBMSMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartADBMSTask(void *argument);
void StartTask03(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of ADBMSMutex */
  ADBMSMutexHandle = osMutexNew(&ADBMSMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of ADBMSTask */
  ADBMSTaskHandle = osThreadNew(StartADBMSTask, NULL, &ADBMSTask_attributes);

  /* creation of bomb_squad_tps */
  bomb_squad_tpsHandle = osThreadNew(StartTask03, NULL, &bomb_squad_tps_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for (;;) {
    
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartADBMSTask */
/**
* @brief Function implementing the ADBMSTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartADBMSTask */
__weak void StartADBMSTask(void *argument)
{
  /* USER CODE BEGIN StartADBMSTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(pdMS_TO_TICKS(1));
  }
  /* USER CODE END StartADBMSTask */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the bomb_squad_tps thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */
  #define LV_FUSE_MAX (double)(5)
  #define LV_CURRENT_LSB TPS2482_CURRENT_LSB_EQ(LV_FUSE_MAX)
  #define LV_CAL_VAL TPS2482_CAL_EQ(LV_CURRENT_LSB, R_SHUNT)
  #define LV_ALERT_LIM_VAL TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(LV_FUSE_MAX / LV_CURRENT_LSB), LV_CAL_VAL)
  #define R_SHUNT (double)(.002) // Ohm

  printf("Starting I2C Scanning: \r\n");
  uint8_t i = 0, ret;
  for (i = 1; i < 128; i++)
  {
    ret = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 3, 5);
    if (ret != HAL_OK)
    { /* No ACK Received At That Address */
      printf(" - ");
    }
    else if (ret == HAL_OK)
    {
      printf("0x%X", i);
    }
  }
  printf("Done! \r\n\r\n");

  const uint8_t tps_count = 1;
  uint8_t tps_addresses[1] = { TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND) };
  TPS2482_Configuration tps_cfg[1] = {
    {
      .config = TPS2482_CONFIG_DEFAULT,
      .cal = LV_CAL_VAL,
      .mask = TPS2482_MASK_SOL,
      .alert_lim = LV_ALERT_LIM_VAL
    }
  };
  uint16_t tps_ids[1] = {0};
  bool tps_ok[1] = {false};
  uint16_t vshunt_raw[1] = {0};
  uint16_t vbus_raw[1] = {0};
  uint16_t current_raw[1] = {0};

  printf("[TPS2482] Task start\r\n");
  osDelay(pdMS_TO_TICKS(1000));
  TPS2482_Init(&hi2c1, tps_addresses, tps_cfg, tps_ids, tps_ok, tps_count);
  printf("[TPS2482] Init done ok=%u id=0x%u\r\n",
         (unsigned int)tps_ok[0], tps_ids[0]);

  /* Infinite loop */
  for(;;)
  {
    TPS2482_Poll_Shunt_Voltage(&hi2c1, tps_addresses, vshunt_raw, tps_count);
    TPS2482_Poll_Bus_Voltage(&hi2c1, tps_addresses, vbus_raw, tps_count);
    TPS2482_Poll_Current(&hi2c1, tps_addresses, current_raw, tps_count);   

    printf("[TPS2482] Raw vshunt=0x%04X vbus=0x%04X current=0x%04X \r\n",
           (unsigned int)vshunt_raw[0],
           (unsigned int)vbus_raw[0],
           (unsigned int)current_raw[0]);
    
    printf("[TPS2482] Conv Vshunt=%.3f mV, Vbus=%.3f V, I=%.3f mA\r\n",
           (unsigned int)vshunt_raw[0] * TPS2482_CONV_VSHUNT,
           (unsigned int)vbus_raw[0] * TPS2482_CONV_VBUS,
           TPS2482_CURRENT_LSB_EQ(current_raw[0]));

    osDelay(pdMS_TO_TICKS(1000));
  }
  /* USER CODE END StartTask03 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

