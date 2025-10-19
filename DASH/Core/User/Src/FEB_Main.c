// **************************************** Includes ****************************************

#include "FEB_Main.h"

uint64_t bms_errors;
extern UART_HandleTypeDef huart2;
extern CAN_HandleTypeDef hcan1;
extern I2C_HandleTypeDef hi2c1;

// ********************************** Variables **********************************
char buf[128];
uint8_t buf_len; //stolen from Main_Setup (SN2)


// **************************************** Functions ****************************************

// static void FEB_Variable_Init(void) {
    
// }

// void FEB_Main_Setup(void) {
    
// }

void FEB_Main_Loop(void) {

    StartDisplayTask(NULL);
    
	HAL_Delay(10);

}