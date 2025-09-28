#include "FEB_IO.h"
#include "FEB_CAN.h"
#include "ui.h"

/* ------------------- External handles ------------------- */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart3;

/* ------------------- State variables ------------------- */
static uint32_t rtd_press_start_time;
static uint32_t rtd_buzzer_start_time = 0;
static uint8_t set_rtd_buzzer = 1;
static uint8_t IO_state = 0xFF;
static uint8_t r2d = 0;

static FEB_SM_ST_t bms_state;
static FEB_SM_ST_t prev_state;

static uint8_t entered_drive = 0;
static uint8_t exited_drive = 0;
static uint32_t exit_buzzer_start_time = 0;

static uint32_t datalog_press_start_time = 0;
static uint8_t datalog_active = 0;

static uint8_t tssi_startup = 0;
static uint8_t imd_startup = 0;


/* ------------------- Utilities ------------------- */
uint8_t set_n_bit(uint8_t x, uint8_t n, uint8_t bit_value) {
    return (x & (~(1 << n))) | (bit_value << n);
}

void enable_r2d(void) { r2d = 1; }
void disable_r2d(void){ r2d = 0; }
bool is_r2d(void)     { return r2d == 1; }


/* ------------------- Initialization ------------------- */
void FEB_IO_Init(void) {
    uint8_t init_val = 0xF;
    HAL_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, &init_val, 1, HAL_MAX_DELAY);
    bms_state = FEB_CAN_BMS_Get_State();
}

/* ------------------- Reset ------------------- */
void FEB_IO_Reset_All(void) {
    rtd_press_start_time = 0;
    rtd_buzzer_start_time = 0;
    set_rtd_buzzer = 1;
    r2d = 0;

    entered_drive = 0;
    exited_drive = 0;
    exit_buzzer_start_time = 0;

    datalog_press_start_time = 0;
    datalog_active = 0;
}


/* ------------------- TSSI & IMD ------------------- */
void FEB_IO_HandleTSSI(void)
{
    if (FEB_CAN_BMS_GET_FAULTS()) {
        if (tssi_startup)
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_RESET);
        else
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
    } else {
        tssi_startup = 1;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
    }
}

void FEB_IO_HandleIMD(void)
{
    if (FEB_CAN_GET_IMD_FAULT()) {
        if (imd_startup)
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
    } else {
        imd_startup = 1;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
    }
}



/* ------------------- RTD Button ------------------- */
void FEB_IO_HandleRTDButton(void)
{
    uint8_t received_data = 0x00;
    HAL_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

    uint8_t brake_pressure = FEB_CAN_APPS_Get_Brake_Pos();
    uint8_t inv_enabled = FEB_CAN_APPS_Get_Enabled();

    prev_state = bms_state;
    bms_state = FEB_CAN_BMS_Get_State();

    /* Reset all if LV */
    if (bms_state == FEB_SM_ST_LV)
        FEB_IO_Reset_All();

    /* Detect transitions for buzzer */
    if (prev_state == FEB_SM_ST_ENERGIZED && bms_state == FEB_SM_ST_DRIVE)
        entered_drive = 1;
    else if (prev_state == FEB_SM_ST_DRIVE && bms_state == FEB_SM_ST_ENERGIZED)
        exited_drive = 1;

    /* UI colors */
    if (bms_state == FEB_SM_ST_DRIVE) {
        if (inv_enabled)
            lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0x019F02), LV_PART_MAIN);
        else
            lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    } else if (bms_state == FEB_SM_ST_ENERGIZED) {
        lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0xFE0000), LV_PART_MAIN);
        r2d = 0;
    }

    /* RTD button logic */
    if (received_data & (1 << 1)) {
        if ((HAL_GetTick() - rtd_press_start_time) >= BTN_HOLD_TIME &&
            brake_pressure >= 20 &&
            (bms_state == FEB_SM_ST_ENERGIZED || bms_state == FEB_SM_ST_DRIVE))
        {
            // Attempt to enter or exit drive
	        if (bms_state == FEB_SM_ST_ENERGIZED) {
	            r2d = 1; // Try entering Drive
	        } else if (bms_state == FEB_SM_ST_DRIVE) {
	            r2d = 0; // Try exiting Drive
	        }

            // Send R2D over CAN
	        IO_state = (uint8_t) set_n_bit(IO_state, 1, r2d);
	        FEB_CAN_ICS_Transmit_Button_State(IO_state);
	        rtd_press_start_time = HAL_GetTick(); // reset timer
        } else {
            IO_state = set_n_bit(IO_state, 1, r2d);
        }
    } else {
        rtd_press_start_time = HAL_GetTick();
        IO_state = set_n_bit(IO_state, 1, r2d);
    }
}


/* ------------------- Data Logger Button ------------------- */
void FEB_IO_HandleDataLoggerButton(void)
{
    uint8_t received_data = 0x00;
    HAL_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

    if (received_data & (1 << 2)) {
        if ((HAL_GetTick() - datalog_press_start_time) >= BTN_HOLD_TIME) {
            datalog_active = !datalog_active;
            IO_state = set_n_bit(IO_state, 2, datalog_active);
            datalog_press_start_time = HAL_GetTick();
        } else {
            IO_state = set_n_bit(IO_state, 2, datalog_active);
        }
    } else {
        datalog_press_start_time = HAL_GetTick();
        IO_state = set_n_bit(IO_state, 2, datalog_active);
    }

    /* UI feedback */
    if (datalog_active)
        lv_obj_set_style_bg_color(ui_ButtonDataLog, lv_color_hex(0x019F02), LV_PART_MAIN);
    else
        lv_obj_set_style_bg_color(ui_ButtonDataLog, lv_color_hex(0xFE0000), LV_PART_MAIN);
}


/* ------------------- Switches ------------------- */
/* ------------------- Coolant Pump Switch ------------------- */
void FEB_IO_HandleSwitch_CoolantPump(void)
{
    uint8_t received_data = 0x00;
    HAL_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

    if (received_data & (1 << 5)) {
        IO_state = set_n_bit(IO_state, 5, 1);
        lv_obj_set_style_bg_color(ui_ButtonCoolPump, lv_color_hex(0x019F02), LV_PART_MAIN);
    } else {
        IO_state = set_n_bit(IO_state, 5, 0);
        lv_obj_set_style_bg_color(ui_ButtonCoolPump, lv_color_hex(0xFE0000), LV_PART_MAIN);
    }
}

/* ------------------- Radiator Fan Switch ------------------- */
void FEB_IO_HandleSwitch_RadiatorFan(void)
{
    uint8_t received_data = 0x00;
    HAL_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

    if (received_data & (1 << 6)) {
        IO_state = set_n_bit(IO_state, 6, 1);
        lv_obj_set_style_bg_color(ui_ButtonRADFan, lv_color_hex(0x019F02), LV_PART_MAIN);
    } else {
        IO_state = set_n_bit(IO_state, 6, 0);
        lv_obj_set_style_bg_color(ui_ButtonRADFan, lv_color_hex(0xFE0000), LV_PART_MAIN);
    }
}

/* ------------------- Accumulator Fan Switch ------------------- */
void FEB_IO_HandleSwitch_AccumulatorFan(void)
{
    uint8_t received_data = 0x00;
    HAL_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

    if (received_data & (1 << 7)) {
        IO_state = set_n_bit(IO_state, 7, 1);
        lv_obj_set_style_bg_color(ui_ButtonAccFan, lv_color_hex(0x019F02), LV_PART_MAIN);
    } else {
        IO_state = set_n_bit(IO_state, 7, 0);
        lv_obj_set_style_bg_color(ui_ButtonAccFan, lv_color_hex(0xFE0000), LV_PART_MAIN);
    }
}



/* ------------------- Buzzer ------------------- */
void FEB_IO_HandleBuzzer(void)
{
    uint8_t inv_enabled = FEB_CAN_APPS_Get_Enabled();

    if (entered_drive && bms_state == FEB_SM_ST_DRIVE && inv_enabled) {
        if (rtd_buzzer_start_time == 0) {
            rtd_buzzer_start_time = HAL_GetTick();
        }
        set_rtd_buzzer = 0;
        IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
    }
    else if (exited_drive && bms_state == FEB_SM_ST_ENERGIZED && !inv_enabled) {
        if (exit_buzzer_start_time == 0) {
            exit_buzzer_start_time = HAL_GetTick();
        }
        set_rtd_buzzer = 0;
        IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
    }
    else {
        set_rtd_buzzer = 1;
        IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
    }

    if (((HAL_GetTick() - rtd_buzzer_start_time) >= RTD_BUZZER_TIME && rtd_buzzer_start_time > 0) ||
        ((HAL_GetTick() - exit_buzzer_start_time) >= RTD_BUZZER_EXIT_TIME && exit_buzzer_start_time > 0))
    {
        rtd_buzzer_start_time = 0;
        exit_buzzer_start_time = 0;
        entered_drive = 0;
        exited_drive = 0;
        set_rtd_buzzer = 1;
        IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
    }

    uint8_t transmit_rtd = (0b1111111 << 1) + set_rtd_buzzer;
    HAL_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, &transmit_rtd, 1, HAL_MAX_DELAY);

    FEB_CAN_ICS_Transmit_Button_State(IO_state);
}