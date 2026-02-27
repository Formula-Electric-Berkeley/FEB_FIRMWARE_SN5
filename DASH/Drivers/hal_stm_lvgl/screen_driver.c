/*
 * screen.c
 *
 *  Created on: Feb 16, 2023
 *      Author: morgan
 */

#include "main.h"
#include "src/display/lv_display.h"
#include "src/display/lv_display_private.h"
#include "stm32469i_discovery_lcd.h"
#include <lvgl.h>
#include <screen_driver.h>
#include <stdio.h>
#include <../lvgl/src/drivers/display/st_ltdc/lv_st_ltdc.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define LCD_SCREEN_WIDTH 800  // Actual screen width in landscape mode
#define LCD_SCREEN_HEIGHT 480  // Actual screen height in landscape mode
#define FRAMEBUFFER_SIZE (uint32_t)(LCD_SCREEN_HEIGHT * LCD_SCREEN_WIDTH)
#define DMA_XFERS_NEEDED FRAMEBUFFER_SIZE/2 // We need half as many transfers because the buffer is an array of
											// 16 bits but the transfers are 32 bits.

lv_display_t * display1;

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
// lv_disp_drv_t lv_display_driver;
__attribute__((section(".framebuffer"))) lv_color_t framebuffer_1[FRAMEBUFFER_SIZE];
__attribute__((section(".framebuffer"))) lv_color_t framebuffer_2[FRAMEBUFFER_SIZE];
__attribute__((section(".framebuffer"))) lv_color_t framebuffer_3[FRAMEBUFFER_SIZE];

#define FRAMEBUFFER_ADDR ((uint32_t)0xC0000000)

/*
 * Private functions prototypes
 */
// void stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void dma2d_copy_area(lv_area_t area, uint32_t src_buffer, uint32_t dst_buffer);
// void my_stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void display_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map);

/*
 * Public functions definitions
 */

void screen_driver_init(void)
{
    BSP_SDRAM_Init();
    BSP_LCD_Init();

    /* Initialize framebuffer_1 as the LTDC layer */
    BSP_LCD_LayerDefaultInit(0, (uint32_t)framebuffer_1);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();

    /* --- FIX: Explicitly ensure the LTDC layer geometry is correct --- */
    LTDC_LayerCfgTypeDef layer_cfg;

    layer_cfg.WindowX0 = 0;
    layer_cfg.WindowY0 = 0;
    layer_cfg.WindowX1 = LCD_SCREEN_WIDTH;   // 800
    layer_cfg.WindowY1 = LCD_SCREEN_HEIGHT;  // 480

    layer_cfg.PixelFormat    = LTDC_PIXEL_FORMAT_RGB565;
    layer_cfg.FBStartAdress  = (uint32_t)framebuffer_1;

    /* Stride defined implicitly by width and pixel format */
    layer_cfg.Alpha = 255;
    layer_cfg.Alpha0 = 0;
    layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

    layer_cfg.ImageWidth  = LCD_SCREEN_WIDTH;
    layer_cfg.ImageHeight = LCD_SCREEN_HEIGHT;

    layer_cfg.Backcolor.Red   = 0;
    layer_cfg.Backcolor.Green = 0;
    layer_cfg.Backcolor.Blue  = 0;

    HAL_LTDC_ConfigLayer(&hltdc, &layer_cfg, 0);
    HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);

    /* ---- LVGL init ---- */
    display1 = lv_display_create(LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT);
    lv_display_set_buffers(display1, framebuffer_2, framebuffer_3, FRAMEBUFFER_SIZE, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display1, display_flush_cb);

    // static lv_disp_draw_buf_t draw_buf;
    // lv_disp_draw_buf_init(&draw_buf, framebuffer_1, framebuffer_2, FRAMEBUFFER_SIZE);

    // lv_disp_drv_init(&lv_display_driver);
    // lv_display_driver.hor_res = LCD_SCREEN_WIDTH;
    // lv_display_driver.ver_res = LCD_SCREEN_HEIGHT;
    // lv_display_driver.draw_buf = &draw_buf;
    // lv_display_driver.full_refresh = true;
    // lv_display_driver.flush_cb = my_stm32_flush_cb;
    // lv_disp_drv_register(&lv_display_driver);
}



// void my_stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
//     /* Use the active framebuffer that LTDC is displaying */
//     uint16_t *fb = (uint16_t *)framebuffer_1;

//     /* Copy the LVGL draw area into the framebuffer */
//     for(int y = area->y1; y <= area->y2; y++) {
//         uint32_t fb_index = y * LCD_SCREEN_WIDTH + area->x1;
//         uint32_t copy_pixels = (area->x2 - area->x1 + 1);
//         memcpy(&fb[fb_index], color_p, copy_pixels * sizeof(lv_color_t));
//         color_p += copy_pixels;
//     }

//     /* Tell LVGL we're done */
//     lv_disp_flush_ready(disp_drv);

//     /* Make LTDC refresh this buffer NOW */
//     HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);
// }

static inline void put_px(int32_t x, int32_t y, lv_color_t color)
{
    /* Bounds check — optional but safe during bring-up */
    if (x < 0 || x >= LCD_SCREEN_WIDTH || y < 0 || y >= LCD_SCREEN_HEIGHT) return;

    framebuffer_1[y * LCD_SCREEN_WIDTH + x] = color;
}

void display_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{
    /* The most simple case (also the slowest) to send all rendered pixels to the
     * screen one-by-one.  `put_px` is just an example.  It needs to be implemented by you. */
    /* Let's say it's a 16 bit (RGB565) display */
    lv_color_t * buf16 = (lv_color_t *)px_map;

    // /* if using a monochrome display (LV_COLOR_DEPTH 1), make sure to skip the palette! (https://docs.lvgl.io/master/main-modules/display/color_format.html#monochrome-displays) */
    // int32_t x, y;
    // for(y = area->y1; y <= area->y2; y++) {
    //     for(x = area->x1; x <= area->x2; x++) {
    //         // put_px(x, y, *buf16);
    //         framebuffer_1[y * LCD_SCREEN_WIDTH + x] =  *buf16;
    //         buf16++;
    //     }
    // }

    /* Copy the LVGL draw area into the framebuffer */
    for(int y = area->y1; y <= area->y2; y++) {
        uint32_t fb_index = y * LCD_SCREEN_WIDTH + area->x1;
        uint32_t copy_pixels = (area->x2 - area->x1 + 1);
        memcpy(&framebuffer_1[fb_index], buf16, copy_pixels * sizeof(lv_color_t));
        buf16 += copy_pixels;
    }

    /* IMPORTANT!!!
     * Inform LVGL that flushing is complete so buffer can be modified again. */
    HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);
    lv_display_flush_ready(display);
}

/*
 * Private functions definitions
 */
// void stm32_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p){
// 	lv_disp_t *disp = _lv_refr_get_disp_refreshing();
// 	uint16_t *dma_xfer_src, *dma_xfer_dst;
// 	if(!lv_disp_flush_is_last(disp_drv)){
// 		lv_disp_flush_ready(disp_drv);
// 		return;
// 	}

// 	// Swap the buffer for the one to display and reload the screen at the next vertical blanking
// 	HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)color_p, 0);
// 	HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING); // VSYNC

// 	// Determine source and destination of transfer
// 	dma_xfer_src = (uint16_t *)color_p;
// 	if(color_p == framebuffer_1){
// 		dma_xfer_dst = (uint16_t *)framebuffer_2;
// 	}else{
// 		dma_xfer_dst = (uint16_t *)framebuffer_1;
// 	}

// 	for(size_t i = 0; i < disp->inv_p; i++){
// 		// If the area was not joined (and thus should not be ignored)
// 		if(!disp->inv_area_joined[i]){
// 			dma2d_copy_area(disp->inv_areas[i], (uint32_t)dma_xfer_src, (uint32_t)dma_xfer_dst);
// 		}
// 	}

// 	lv_disp_flush_ready(disp_drv);
// }

void dma2d_copy_area(lv_area_t area, uint32_t src_buffer, uint32_t dst_buffer){
	size_t start_offset = (LCD_SCREEN_WIDTH*(area.y1) + (area.x1))*2; // address offset (not pixel offset so it is multiplied by 2)
	size_t area_width = 1 + area.x2 - area.x1;
	size_t area_height = 1 +  area.y2 - area.y1;
	size_t in_out_offset = LCD_SCREEN_WIDTH - area_width;

	// Set up DMA2D to transfer parts of picture to part of picture
	hdma2d.Init.Mode = DMA2D_M2M;													// plain memory to memory
	hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
	hdma2d.Init.OutputOffset = in_out_offset;										// nb pixels in buffer between end of area line and start of next area line
	hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputColorMode = DMA2D_INPUT_RGB565;
	hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputOffset = in_out_offset;
	hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].AlphaMode = DMA2D_NO_MODIF_ALPHA;
	hdma2d.LayerCfg[DMA2D_FOREGROUND_LAYER].InputAlpha = 0;

	HAL_DMA2D_Init(&hdma2d);
	HAL_DMA2D_ConfigLayer(&hdma2d, DMA2D_FOREGROUND_LAYER);
	HAL_DMA2D_Start(&hdma2d, src_buffer + start_offset, dst_buffer + start_offset, area_width, area_height);	// Start transfer
	HAL_DMA2D_PollForTransfer(&hdma2d, 10000);	// Wait for transfer to be over
}
