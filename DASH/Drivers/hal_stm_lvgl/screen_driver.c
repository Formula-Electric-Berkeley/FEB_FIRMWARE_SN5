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
#define DMA_XFERS_NEEDED                                                                                               \
  FRAMEBUFFER_SIZE / 2 // We need half as many transfers because the buffer is an array of
                       // 16 bits but the transfers are 32 bits.

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
__attribute__((section(".framebuffer"))) lv_color_t screen_buffer[FRAMEBUFFER_SIZE];
__attribute__((section(".framebuffer"))) lv_color_t framebuffer_1[FRAMEBUFFER_SIZE];

#define FRAMEBUFFER_ADDR ((uint32_t)0xC0000000)

static lv_disp_drv_t *flush_disp_drv;
static lv_disp_t *flush_disp;
static uint32_t flush_src;
static uint32_t flush_dst;
static uint16_t flush_area_idx;

/*
 * Private functions prototypes
 */
void stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void dma2d_start_area_it(lv_area_t area, uint32_t src_buffer, uint32_t dst_buffer);
static void dma2d_flush_next_area(void);
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
  lv_disp_draw_buf_init(&draw_buf, framebuffer_1, screen_buffer, FRAMEBUFFER_SIZE);

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
  lv_disp_t *disp = _lv_refr_get_disp_refreshing();
  if (!lv_disp_flush_is_last(disp_drv))
  {
    lv_disp_flush_ready(disp_drv);
    return;
  }

  // Swap the buffer for the one to display and reload the screen at the next vertical blanking
  HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)color_p, 0);
  HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING); // VSYNC

  // The just-rendered buffer (color_p) is now on screen; sync the dirty areas
  // back into the other buffer so it is coherent for the next frame. The copies
  // run on DMA2D in the background, chained one per transfer-complete IRQ; the
  // final one calls lv_disp_flush_ready() to release LVGL.
  flush_disp_drv = disp_drv;
  flush_disp = disp;
  flush_src = (uint32_t)color_p;
  flush_dst = (color_p == screen_buffer) ? (uint32_t)framebuffer_1 : (uint32_t)screen_buffer;
  flush_area_idx = 0;
  dma2d_flush_next_area();
}

// Start an interrupt-driven DMA2D copy of the next non-joined dirty area. When
// no areas remain, release LVGL. Runs in task context for the first area and in
// DMA2D IRQ context for the rest.
static void dma2d_flush_next_area(void)
{
  while (flush_area_idx < flush_disp->inv_p)
  {
    uint16_t i = flush_area_idx++;
    // Skip areas that were joined into another (and thus already copied)
    if (!flush_disp->inv_area_joined[i])
    {
      dma2d_start_area_it(flush_disp->inv_areas[i], flush_src, flush_dst);
      return; // wait for the transfer-complete IRQ to advance
    }
  }

  // All dirty areas synced into the back buffer
  lv_disp_flush_ready(flush_disp_drv);
}

static void dma2d_start_area_it(lv_area_t area, uint32_t src_buffer, uint32_t dst_buffer)
{
  size_t start_offset =
      (LCD_SCREEN_WIDTH * (area.y1) + (area.x1)) * 2; // address offset (not pixel offset so it is multiplied by 2)
  size_t area_width = 1 + area.x2 - area.x1;
  size_t area_height = 1 + area.y2 - area.y1;
  size_t in_out_offset = LCD_SCREEN_WIDTH - area_width;

  // Set up DMA2D to transfer parts of picture to part of picture
  hdma2d.Init.Mode = DMA2D_M2M; // plain memory to memory
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = in_out_offset; // nb pixels in buffer between end of area line and start of next area line
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputOffset = in_out_offset;
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputAlpha = 0;

  HAL_DMA2D_Init(&hdma2d);
  HAL_DMA2D_ConfigLayer(&hdma2d, DMA2D_FOREGROUND_LAYER);
  HAL_DMA2D_Start_IT(&hdma2d, src_buffer + start_offset, dst_buffer + start_offset, area_width, area_height);
}

// DMA2D transfer-complete IRQ: kick off the next dirty area (or finish).
static void dma2d_xfer_cplt(DMA2D_HandleTypeDef *h)
{
  (void)h;
  dma2d_flush_next_area();
}

// DMA2D error IRQ: give up on the remaining areas and release LVGL so the
// rendering pipeline does not stall waiting for a flush that never completes.
static void dma2d_xfer_error(DMA2D_HandleTypeDef *h)
{
  (void)h;
  lv_disp_flush_ready(flush_disp_drv);
}
