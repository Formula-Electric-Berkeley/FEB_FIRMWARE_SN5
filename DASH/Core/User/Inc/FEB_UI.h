#ifndef __FEB_UI_H
#define __FEB_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "stm32469i_discovery.h"
#include "stm32469i_discovery_lcd.h"
#include "cmsis_os.h"
#include <stdint.h>

/* LCD framebuffer start address in SDRAM */
#define LCD_FB_START_ADDRESS ((uint32_t)0xC0000000)

/* Public UI functions */
void StartDisplayTask(void *argument);
void DrawDashboardUI(voi
