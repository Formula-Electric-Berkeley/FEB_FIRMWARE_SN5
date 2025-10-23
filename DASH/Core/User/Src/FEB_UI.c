#include "FEB_UI.h"
#include "cmsis_os.h"
#include "main.h"
#include "stm32469i_discovery.h"
#include "stm32469i_discovery_lcd.h"
#include <stdio.h>

/* Display Task Example */
void StartDisplayTask(void *argument)
{
    /* Initialize the LCD */
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);  // Use SDRAM framebuffer
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();

    /* Optional: Set backlight ON if controlled via GPIO */
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);

    /* Clear screen and set colors */
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetFont(&Font24);

    BSP_LCD_DisplayStringAt(0, 10, (uint8_t *)"STM32F469I Dashboard", CENTER_MODE);
    BSP_LCD_SetFont(&Font16);
    BSP_LCD_DisplayStringAt(0, 40, (uint8_t *)"Initializing...", CENTER_MODE);

    /* Draw dashboard UI */
    DrawDashboardUI();

    uint32_t counter = 0;
    char buffer[64];

    for (;;)
    {
        HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);

        /* Update counter text */
        sprintf(buffer, "Counter: %lu", counter++);
        BSP_LCD_SetTextColor(LCD_COLOR_YELLOW);
        BSP_LCD_FillRect(20, 260, 200, 30); // Clear area
        BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
        BSP_LCD_DisplayStringAt(20, 260, (uint8_t *)buffer, LEFT_MODE);

        osDelay(1000);
    }
}

/* Draw a simple dashboard grid */
void DrawDashboardUI(void)
{
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_DrawRect(10, 70, 460, 180);

    uint16_t squareSize = 100;
    uint16_t spacing = 20;
    uint16_t startX = 40;
    uint16_t startY = 90;

    uint32_t colors[] = {LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_BLUE, LCD_COLOR_CYAN};

    for (int i = 0; i < 4; i++)
    {
        BSP_LCD_SetTextColor(colors[i]);
        BSP_LCD_FillRect(startX + i * (squareSize + spacing), startY, squareSize, squareSize);
        BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
        char label[10];
        sprintf(label, "S%d", i + 1);
        BSP_LCD_DisplayStringAt(startX + i * (squareSize + spacing) + 30, startY + 40, (uint8_t *)label, LEFT_MODE);
    }

    BSP_LCD_SetFont(&Font20);
    BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
    BSP_LCD_DisplayStringAt(0, 230, (uint8_t *)"Dashboard Active", CENTER_MODE);
}
