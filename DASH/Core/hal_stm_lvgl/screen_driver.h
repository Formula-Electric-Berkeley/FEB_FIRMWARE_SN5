#ifndef SCREEN_DRIVER_H
#define SCREEN_DRIVER_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize LCD hardware and create LVGL display
lv_display_t *screen_driver_init(void);

// Create LVGL display with custom buffers (advanced usage)
lv_display_t *create_disp(void *buf1, void *buf2, uint32_t buf_size, uint32_t layer_idx);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SCREEN_DRIVER_H*/
