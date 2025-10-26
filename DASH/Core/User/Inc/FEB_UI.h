#ifndef __FEB_UI_H
#define __FEB_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "stm32469i_discovery.h"
#include "stm32469i_discovery_lcd.h"
#include "fonts.h"
#include <stdio.h>
#include <stdint.h>

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Main display FreeRTOS task.
 *         Initializes the LCD and renders the dashboard UI.
 * @param  argument: FreeRTOS task argument (unused)
 */
void StartDisplayTask(void *argument);

/**
 * @brief  Draws the static dashboard grid, boxes, and labels.
 */
void DrawDashboardUI(void);

#ifdef __cplusplus
}
#endif

#endif /* __FEB_UI_H */
