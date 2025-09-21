#ifndef __FEB_UI_H
#define __FEB_UI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Display Configuration */
#define LCD_WIDTH  240
#define LCD_HEIGHT 320
#define LCD_PIXEL_FORMAT LTDC_PIXEL_FORMAT_RGB565

/* Color Definitions (RGB565) */
#define LCD_COLOR_BLACK   0x0000
#define LCD_COLOR_WHITE   0xFFFF
#define LCD_COLOR_RED     0xF800
#define LCD_COLOR_GREEN   0x07E0
#define LCD_COLOR_BLUE    0x001F
#define LCD_COLOR_YELLOW  0xFFE0
#define LCD_COLOR_CYAN    0x07FF
#define LCD_COLOR_MAGENTA 0xF81F
#define LCD_COLOR_GRAY    0x8410
#define LCD_COLOR_ORANGE  0xFD20

/* Font Configuration */
typedef struct {
    const uint8_t width;
    const uint8_t height;
    const uint16_t *data;
} LCD_Font_t;

/* Display Functions */
void LCD_Init(LTDC_HandleTypeDef *hltdc);
void LCD_Clear(uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void LCD_FillRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, uint8_t font_size);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color, uint8_t font_size);
void LCD_DrawBitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *bitmap);
void LCD_ScrollVertical(int16_t pixels);
void LCD_SetBacklight(uint8_t brightness);

/* Task Functions */
void StartDisplayTask(void *argument);
void StartBtnTxLoop(void *argument);
void DrawSquareUI(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __FEB_UI_H */