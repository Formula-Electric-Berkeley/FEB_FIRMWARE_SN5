/*
 * screen.c
 *
 *  Created on: Feb 16, 2023
 *      Author: morgan
 */

#include "main.h"
#include "stm32469i_discovery_lcd.h"
#include <lvgl.h>
#include <screen_driver.h>
#include <stdio.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define LCD_SCREEN_WIDTH 800  // Actual screen width in landscape mode
#define LCD_SCREEN_HEIGHT 480 // Actual screen height in landscape mode
#define FRAMEBUFFER_SIZE (uint32_t)(LCD_SCREEN_HEIGHT * LCD_SCREEN_WIDTH)

#define DRAW_BUF_LINES 48
#define DRAW_BUF_SIZE (uint32_t)(LCD_SCREEN_WIDTH * DRAW_BUF_LINES)

/*
 * Handles to peripherals
 */
extern DSI_HandleTypeDef hdsi; // From main.c
extern LTDC_HandleTypeDef hltdc;
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;
extern DMA2D_HandleTypeDef hdma2d;
/*
 * Global variables
 */
lv_disp_drv_t lv_display_driver;
__attribute__((section(".framebuffer"))) lv_color_t screen_buffer[FRAMEBUFFER_SIZE]; // LTDC scans this
__attribute__((section(".framebuffer"))) lv_color_t draw_buf_1[DRAW_BUF_SIZE];       // LVGL render buffer A
__attribute__((section(".framebuffer"))) lv_color_t draw_buf_2[DRAW_BUF_SIZE];       // LVGL render buffer B

#define FRAMEBUFFER_ADDR ((uint32_t)0xC0000000)

// The flush in progress: saved so the DMA2D transfer-complete IRQ can release LVGL.
static lv_disp_drv_t *flush_disp_drv;

/*
 * Private functions prototypes
 */
void stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void dma2d_xfer_cplt(DMA2D_HandleTypeDef *h);
static void dma2d_xfer_error(DMA2D_HandleTypeDef *h);

/*
 * Public functions definitions
 */

void screen_driver_init(void)
{
  BSP_SDRAM_Init();
  BSP_LCD_Init();

  /* Initialize framebuffer_1 as the LTDC layer */
  BSP_LCD_LayerDefaultInit(0, (uint32_t)screen_buffer);
  BSP_LCD_SelectLayer(0);
  BSP_LCD_DisplayOn();

  /* --- FIX: Explicitly ensure the LTDC layer geometry is correct --- */
  LTDC_LayerCfgTypeDef layer_cfg;

  layer_cfg.WindowX0 = 0;
  layer_cfg.WindowY0 = 0;
  layer_cfg.WindowX1 = LCD_SCREEN_WIDTH;  // 800
  layer_cfg.WindowY1 = LCD_SCREEN_HEIGHT; // 480

  layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  layer_cfg.FBStartAdress = (uint32_t)screen_buffer;

  /* Stride defined implicitly by width and pixel format */
  layer_cfg.Alpha = 255;
  layer_cfg.Alpha0 = 0;
  layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

  layer_cfg.ImageWidth = LCD_SCREEN_WIDTH;
  layer_cfg.ImageHeight = LCD_SCREEN_HEIGHT;

  layer_cfg.Backcolor.Red = 0;
  layer_cfg.Backcolor.Green = 0;
  layer_cfg.Backcolor.Blue = 0;

  HAL_LTDC_ConfigLayer(&hltdc, &layer_cfg, 0);
  HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);

  hdma2d.XferCpltCallback = dma2d_xfer_cplt;
  hdma2d.XferErrorCallback = dma2d_xfer_error;

  /* ---- LVGL init ----  */
  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, draw_buf_1, draw_buf_2, DRAW_BUF_SIZE);

  lv_disp_drv_init(&lv_display_driver);
  lv_display_driver.hor_res = LCD_SCREEN_WIDTH;
  lv_display_driver.ver_res = LCD_SCREEN_HEIGHT;
  lv_display_driver.draw_buf = &draw_buf;
  lv_display_driver.full_refresh = false;
  lv_display_driver.flush_cb = stm32_flush_cb;
  lv_disp_drv_register(&lv_display_driver);
}

/*
 * Private functions definitions
 */
void stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  size_t area_width = 1 + area->x2 - area->x1;
  size_t area_height = 1 + area->y2 - area->y1;
  size_t dst_offset =
      (LCD_SCREEN_WIDTH * area->y1 + area->x1) * 2; // byte offset of the area's top-left in screen_buffer

  flush_disp_drv = disp_drv;

  hdma2d.Init.Mode = DMA2D_M2M; // plain memory to memory
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = LCD_SCREEN_WIDTH - area_width; // skip to the start of the next screen row
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputOffset = 0; // compact source: rows are contiguous
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputAlpha = 0;

  HAL_DMA2D_Init(&hdma2d);
  HAL_DMA2D_ConfigLayer(&hdma2d, DMA2D_FOREGROUND_LAYER);
  // Copy runs on DMA2D in the background; dma2d_xfer_cplt() releases LVGL when done,
  // so LVGL can render the next chunk into the other draw buffer meanwhile.
  HAL_DMA2D_Start_IT(&hdma2d, (uint32_t)color_p, (uint32_t)screen_buffer + dst_offset, area_width, area_height);
}

// DMA2D transfer-complete IRQ: the chunk has been blitted, release the draw buffer.
static void dma2d_xfer_cplt(DMA2D_HandleTypeDef *h)
{
  (void)h;
  lv_disp_flush_ready(flush_disp_drv);
}

// DMA2D error IRQ: give up on the remaining areas and release LVGL so the
// rendering pipeline does not stall waiting for a flush that never completes.
static void dma2d_xfer_error(DMA2D_HandleTypeDef *h)
{
  (void)h;
  lv_disp_flush_ready(flush_disp_drv);
}
