#include "main.h"
#include "stm32469i_discovery_lcd.h"
#include <lvgl/src/lvgl_private.h>
#include <stdio.h>
#include "cmsis_os.h"

#include "screen_driver.h"

// Screen dimensions (800x480 landscape)
#define LCD_SCREEN_WIDTH  800
#define LCD_SCREEN_HEIGHT 480
#define FRAMEBUFFER_SIZE  (LCD_SCREEN_WIDTH * LCD_SCREEN_HEIGHT)

// Allocate framebuffers in SDRAM for RGB888 (24-bit per pixel = 3 bytes per pixel)
// Total size: 800 * 480 * 3 bytes = 1,152,000 bytes = 1.125 MB
__attribute__((section(".sdram"))) static uint8_t framebuffer_1[LCD_SCREEN_WIDTH * LCD_SCREEN_HEIGHT * 3];

// Buffer for LVGL partial rendering (optimized size for embedded systems)
// 2560 pixels * 3 bytes = 7,680 bytes = 7.5 KB (recommended for partial rendering)
#define DRAW_BUFFER_SIZE  2560
__attribute__((section(".sdram"))) static uint8_t draw_buffer[DRAW_BUFFER_SIZE * 3];

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map);

extern LTDC_HandleTypeDef hltdc;

lv_display_t *create_disp(void *buf1, void *buf2, uint32_t buf_size, uint32_t layer_idx) {
    LTDC_LayerCfgTypeDef *layer_cfg = &hltdc.LayerCfg[layer_idx];
    uint32_t layer_width = layer_cfg->ImageWidth;
    uint32_t layer_height = layer_cfg->ImageHeight;
    lv_color_format_t cf = LV_COLOR_FORMAT_RGB888;

    lv_display_t *disp = lv_display_create(layer_width, layer_height);
    lv_display_set_color_format(disp, cf);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_driver_data(disp, (void *)(uintptr_t)layer_idx);

	lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    return disp;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t layer_idx = (uint32_t)(uintptr_t)lv_display_get_driver_data(disp);
	LTDC_LayerCfgTypeDef *layer_cfg = &hltdc.LayerCfg[layer_idx];

	int32_t disp_width = disp->hor_res;
	int32_t area_width = area->x2 - area->x1 + 1;
	int32_t area_height = area->y2 - area->y1 + 1;

	// RGB888 uses 3 bytes per pixel: [R][G][B]
	uint8_t *fb_p = (uint8_t *)layer_cfg->FBStartAdress + ((area->y1 * disp_width + area->x1) * 3);
	uint8_t *px_map_p = (uint8_t *)px_map;

	if (area_width == disp_width) {
		// Full-width area: copy entire rectangular region in one memcpy
		lv_memcpy(fb_p, px_map_p, area_width * area_height * 3);
	} else {
		// Partial-width area: copy row by row
		for (int i = 0; i < area_height; i++) {
			lv_memcpy(fb_p, px_map_p, area_width * 3);
			fb_p += disp_width * 3;
			px_map_p += area_width * 3;
		}
	}

    lv_display_flush_ready(disp);
}

lv_display_t *screen_driver_init(void) {
    printf("[LCD] Starting screen driver initialization...\r\n");

    // Initialize LCD hardware
    printf("[LCD] Initializing LCD hardware (DSI, OTM8009A)...\r\n");
    if (BSP_LCD_Init() != LCD_OK) {
        printf("[LCD] ERROR: BSP_LCD_Init() failed!\r\n");
        Error_Handler();
    }
    printf("[LCD] BSP_LCD_Init() successful\r\n");

    // Allow LCD controller to stabilize after initialization
    osDelay(100);  // Use osDelay since RTOS kernel is already running
    printf("[LCD] LCD stabilization delay complete\r\n");

    // Enable LCD display output (send display-on command to OTM8009A)
    printf("[LCD] Enabling LCD display output...\r\n");
    BSP_LCD_DisplayOn();
    printf("[LCD] LCD display enabled\r\n");

    // Enable backlight (critical for visibility)
    printf("[LCD] Turning on backlight (PA3)...\r\n");
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);
    printf("[LCD] Backlight enabled\r\n");

    // Configure LTDC Layer 0 for RGB888 with framebuffer
    // NOTE: We do NOT call BSP_LCD_LayerDefaultInit() because it hardcodes RGB565!
    // Instead, we configure the layer directly with RGB888
    printf("[LCD] Configuring LTDC Layer 0 (RGB888, framebuffer @ 0x%08lX)...\r\n", (uint32_t)framebuffer_1);

    LTDC_LayerCfgTypeDef layer_cfg;
    layer_cfg.WindowX0 = 0;
    layer_cfg.WindowX1 = LCD_SCREEN_WIDTH;
    layer_cfg.WindowY0 = 0;
    layer_cfg.WindowY1 = LCD_SCREEN_HEIGHT;
    layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB888;
    layer_cfg.Alpha = 255;
    layer_cfg.Alpha0 = 0;
    layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    layer_cfg.FBStartAdress = (uint32_t)framebuffer_1;
    layer_cfg.ImageWidth = LCD_SCREEN_WIDTH;
    layer_cfg.ImageHeight = LCD_SCREEN_HEIGHT;
    layer_cfg.Backcolor.Blue = 0;
    layer_cfg.Backcolor.Green = 0;
    layer_cfg.Backcolor.Red = 0;

    if (HAL_LTDC_ConfigLayer(&hltdc, &layer_cfg, 0) != HAL_OK) {
        printf("[LCD] ERROR: HAL_LTDC_ConfigLayer() failed!\r\n");
        Error_Handler();
    }
    printf("[LCD] LTDC Layer 0 configured successfully\r\n");

    // Enable LTDC Layer 0 (critical - layer is configured but disabled by default)
    __HAL_LTDC_LAYER_ENABLE(&hltdc, 0);

    // Reload LTDC configuration to transfer shadow registers to active registers
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    printf("[LCD] LTDC Layer 0 enabled and configuration reloaded\r\n");

    // DIAGNOSTIC TEST: Verify LTDC can display before LVGL initialization
    // This tests the hardware path: CPU -> SDRAM -> LTDC -> DSI -> Panel
    printf("[LCD] Running diagnostic framebuffer test...\r\n");

    // Fill framebuffer with red (RGB888: R=0xFF, G=0x00, B=0x00)
    for (uint32_t i = 0; i < FRAMEBUFFER_SIZE; i++) {
        framebuffer_1[i * 3 + 0] = 0xFF;  // Red channel
        framebuffer_1[i * 3 + 1] = 0x00;  // Green channel
        framebuffer_1[i * 3 + 2] = 0x00;  // Blue channel
    }
    printf("[LCD] Framebuffer filled with RED - screen should show red for 500ms...\r\n");
    osDelay(500);  // Use osDelay since RTOS kernel is already running

    // Clear framebuffer (fill with black)
    for (uint32_t i = 0; i < FRAMEBUFFER_SIZE; i++) {
        framebuffer_1[i * 3 + 0] = 0x00;  // Red channel
        framebuffer_1[i * 3 + 1] = 0x00;  // Green channel
        framebuffer_1[i * 3 + 2] = 0x00;  // Blue channel
    }
    printf("[LCD] Framebuffer cleared - diagnostic test complete\r\n");

    // Create LVGL display with partial rendering buffer
    // Buffer size is in bytes, LVGL needs it in pixels (1 pixel = 3 bytes for RGB888)
    uint32_t buf_size = sizeof(draw_buffer) / 3;
    printf("[LCD] Creating LVGL display (draw buffer size: %lu pixels)...\r\n", buf_size);
    lv_display_t *disp = create_disp(draw_buffer, NULL, buf_size, 0);

    if (disp == NULL) {
        printf("[LCD] ERROR: Failed to create LVGL display!\r\n");
        Error_Handler();
    }

    printf("[LCD] Screen driver initialization complete!\r\n");
    printf("[LCD] Display: %dx%d RGB888, Framebuffer: %.2f KB, Draw buffer: %.2f KB\r\n",
           LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT,
           (float)(sizeof(framebuffer_1)) / 1024.0f,
           (float)(sizeof(draw_buffer)) / 1024.0f);

    return disp;
}
