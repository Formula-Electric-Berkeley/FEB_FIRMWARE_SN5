#ifndef FEB_MAIN_H
#define FEB_MAIN_H

#include "FEB_CAN.h"

void FEB_Init(void);
void FEB_Update(void);
void FEB_CAN_Rx_Callback(FEB_CAN_Bus_t bus, CAN_RxHeaderTypeDef *rx_header, void *data);
void FEB_Main_Loop(void);
#define LOGC(fmt, ...) printf("[CRITICAL] " fmt "\r", ##__VA_ARGS__)
#define LOGV(fmt, ...) printf("[VERBOSE] " fmt "\r", ##__VA_ARGS__)
#define LOGE(fmt, ...) printf("[ERROR] " fmt "\r", ##__VA_ARGS__)
#define LOGW(fmt, ...) printf("[WARNING] " fmt "\r", ##__VA_ARGS__)
#define LOGI(fmt, ...) printf("[INFO] " fmt "\r", ##__VA_ARGS__)

#endif
