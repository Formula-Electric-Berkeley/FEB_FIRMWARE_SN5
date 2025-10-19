#include "FEB_UI.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* External LTDC handle */
extern LTDC_HandleTypeDef hltdc;

/* Frame buffer - adjust size based on your display */
// static uint16_t frameBuffer[LCD_WIDTH * LCD_HEIGHT] __attribute__((section(".sdram")));
static uint16_t frameBuffer[LCD_WIDTH * LCD_HEIGHT];

/* Basic 5x7 font data (ASCII 32-127) */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

/* Initialize LCD */
void LCD_Init(LTDC_HandleTypeDef *hltdc)
{
    /* Configure LTDC Layer 1 */
    LTDC_LayerCfgTypeDef pLayerCfg = {0};

    pLayerCfg.WindowX0 = 0;
    pLayerCfg.WindowX1 = LCD_WIDTH;
    pLayerCfg.WindowY0 = 0;
    pLayerCfg.WindowY1 = LCD_HEIGHT;
    pLayerCfg.PixelFormat = LCD_PIXEL_FORMAT;
    pLayerCfg.Alpha = 255;
    pLayerCfg.Alpha0 = 0;
    pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    pLayerCfg.FBStartAdress = (uint32_t)frameBuffer;
    pLayerCfg.ImageWidth = LCD_WIDTH;
    pLayerCfg.ImageHeight = LCD_HEIGHT;
    pLayerCfg.Backcolor.Blue = 0;
    pLayerCfg.Backcolor.Green = 0;
    pLayerCfg.Backcolor.Red = 0;

    HAL_LTDC_ConfigLayer(hltdc, &pLayerCfg, 0);

    /* Clear display */
    LCD_Clear(LCD_COLOR_BLACK);
}

/* Clear entire display with specified color */
void LCD_Clear(uint16_t color)
{
    uint32_t i;
    for (i = 0; i < (LCD_WIDTH * LCD_HEIGHT); i++)
    {
        frameBuffer[i] = color;
    }
}

/* Draw a single pixel */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
    {
        return;
    }
    frameBuffer[y * LCD_WIDTH + x] = color;
}

/* Draw a line using Bresenham's algorithm */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (1)
    {
        LCD_DrawPixel(x1, y1, color);

        if (x1 == x2 && y1 == y2)
            break;

        int16_t e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

/* Draw a rectangle outline */
void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    LCD_DrawLine(x, y, x + width - 1, y, color);
    LCD_DrawLine(x, y, x, y + height - 1, color);
    LCD_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
    LCD_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
}

/* Fill a rectangle with color */
void LCD_FillRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    uint16_t i, j;
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            LCD_DrawPixel(x + j, y + i, color);
        }
    }
}

/* Draw a circle outline */
void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y)
    {
        LCD_DrawPixel(x0 + x, y0 + y, color);
        LCD_DrawPixel(x0 + y, y0 + x, color);
        LCD_DrawPixel(x0 - y, y0 + x, color);
        LCD_DrawPixel(x0 - x, y0 + y, color);
        LCD_DrawPixel(x0 - x, y0 - y, color);
        LCD_DrawPixel(x0 - y, y0 - x, color);
        LCD_DrawPixel(x0 + y, y0 - x, color);
        LCD_DrawPixel(x0 + x, y0 - y, color);

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

/* Fill a circle with color */
void LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y)
    {
        LCD_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        LCD_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        LCD_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        LCD_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

/* Draw a single character */
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, uint8_t font_size)
{
    uint8_t i, j, line;
    uint16_t x0 = x, y0 = y;

    if (ch < 32 || ch > 126)
    {
        ch = '?';
    }

    for (i = 0; i < 5; i++)
    {
        line = font5x7[ch - 32][i];
        for (j = 0; j < 7; j++)
        {
            if (line & 0x01)
            {
                if (font_size == 1)
                {
                    LCD_DrawPixel(x0, y0, color);
                }
                else
                {
                    LCD_FillRectangle(x0, y0, font_size, font_size, color);
                }
            }
            else if (bg_color != LCD_COLOR_BLACK)
            {
                if (font_size == 1)
                {
                    LCD_DrawPixel(x0, y0, bg_color);
                }
                else
                {
                    LCD_FillRectangle(x0, y0, font_size, font_size, bg_color);
                }
            }
            line >>= 1;
            y0 += font_size;
        }
        y0 = y;
        x0 += font_size;
    }
}

/* Draw a string of text */
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color, uint8_t font_size)
{
    uint16_t x0 = x;
    while (*str)
    {
        if (*str == '\n')
        {
            y += 7 * font_size + 2;
            x0 = x;
        }
        else
        {
            LCD_DrawChar(x0, y, *str, color, bg_color, font_size);
            x0 += 6 * font_size;
        }
        str++;
    }
}

/* Draw a bitmap image */
void LCD_DrawBitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *bitmap)
{
    uint16_t i, j;
    uint32_t index = 0;

    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            LCD_DrawPixel(x + j, y + i, bitmap[index++]);
        }
    }
}

/* Scroll display vertically */
void LCD_ScrollVertical(int16_t pixels)
{
    uint16_t *tempBuffer = (uint16_t *)malloc(LCD_WIDTH * abs(pixels) * sizeof(uint16_t));
    if (tempBuffer == NULL)
        return;

    if (pixels > 0)
    {
        /* Scroll up */
        memcpy(tempBuffer, frameBuffer, LCD_WIDTH * pixels * sizeof(uint16_t));
        memmove(frameBuffer, &frameBuffer[LCD_WIDTH * pixels],
                LCD_WIDTH * (LCD_HEIGHT - pixels) * sizeof(uint16_t));
        memcpy(&frameBuffer[LCD_WIDTH * (LCD_HEIGHT - pixels)], tempBuffer,
               LCD_WIDTH * pixels * sizeof(uint16_t));
    }
    else if (pixels < 0)
    {
        /* Scroll down */
        pixels = -pixels;
        memcpy(tempBuffer, &frameBuffer[LCD_WIDTH * (LCD_HEIGHT - pixels)],
               LCD_WIDTH * pixels * sizeof(uint16_t));
        memmove(&frameBuffer[LCD_WIDTH * pixels], frameBuffer,
                LCD_WIDTH * (LCD_HEIGHT - pixels) * sizeof(uint16_t));
        memcpy(frameBuffer, tempBuffer, LCD_WIDTH * pixels * sizeof(uint16_t));
    }

    free(tempBuffer);
}

/* Set backlight brightness (0-100) */
void LCD_SetBacklight(uint8_t brightness)
{
    /* This depends on your hardware configuration */
    /* Usually controlled via PWM on a GPIO pin */
    /* Example implementation:
    TIM_HandleTypeDef *htim = &htim3;
    uint32_t pulse = (brightness * htim->Init.Period) / 100;
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, pulse);
    */
}

/* Display Task - Example usage */
void StartDisplayTask(void *argument)
{
    /* Initialize LCD */
    LCD_Init(&hltdc);

    /* Example: Draw welcome screen */
    LCD_Clear(LCD_COLOR_BLACK);
    LCD_DrawString(10, 10, "STM32 LCD Demo", LCD_COLOR_WHITE, LCD_COLOR_BLACK, 2);
    LCD_DrawRectangle(5, 5, LCD_WIDTH - 10, 40, LCD_COLOR_GREEN);

    /* Draw some shapes */
    LCD_FillRectangle(20, 60, 60, 40, LCD_COLOR_RED);
    LCD_DrawCircle(120, 80, 30, LCD_COLOR_BLUE);
    LCD_FillCircle(200, 80, 20, LCD_COLOR_YELLOW);

    /* Draw lines */
    LCD_DrawLine(10, 120, 230, 120, LCD_COLOR_CYAN);
    LCD_DrawLine(10, 130, 230, 150, LCD_COLOR_MAGENTA);

    uint32_t counter = 0;
    char buffer[32];

    for (;;)
    {
        HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);
        /* Update counter display */
        sprintf(buffer, "Count: %lu", counter++);
        LCD_FillRectangle(10, 200, 150, 20, LCD_COLOR_RED);
        LCD_DrawString(10, 200, buffer, LCD_COLOR_WHITE, LCD_COLOR_BLACK, 1);

        osDelay(1000);
    }


}

void DrawSquareUI(void *argument)
{
    /* Example: Draw a dashboard-style UI */
    LCD_Clear(LCD_COLOR_BLACK);

    /* Draw grid of squares */
    uint16_t squareSize = 50;
    uint16_t spacing = 10;
    uint16_t startX = 20;
    uint16_t startY = 20;

    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            uint16_t x = startX + col * (squareSize + spacing);
            uint16_t y = startY + row * (squareSize + spacing);

            /* Draw square with different colors */
            uint16_t colors[] = {LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_BLUE, LCD_COLOR_YELLOW};
            LCD_FillRectangle(x, y, squareSize, squareSize, colors[(row * 4 + col) % 4]);
            LCD_DrawRectangle(x, y, squareSize, squareSize, LCD_COLOR_WHITE);

            /* Add text labels */
            char label[16];
            sprintf(label, "S%d", row * 4 + col + 1);
            LCD_DrawString(x + 10, y + 20, label, LCD_COLOR_WHITE, colors[(row * 4 + col) % 4], 1);
        }
    }

    /* Add title */
    LCD_DrawString(50, 200, "Dashboard UI", LCD_COLOR_WHITE, LCD_COLOR_BLACK, 2);
}