#include "FEB_WSS.h"
#include "main.h"
#include <math.h>

extern UART_HandleTypeDef huart2;

// -----------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------
#define TICKS_PER_ROTATION (84 * 4)
#define STALE_MS 200u
#define RPM_TO_U8_SCALE 0.5f // LSB = 2 RPM, max = 510 RPM

// -----------------------------------------------------------------------
// Quadrature decode table
// index = (last_cos << 3 | last_sin << 2 | cos_now << 1 | sin_now)
// -----------------------------------------------------------------------
static const int8_t QUAD_TABLE[16] = {0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0};

// -----------------------------------------------------------------------
// Encoder state
// -----------------------------------------------------------------------
typedef struct
{
  volatile int32_t count;
  volatile int32_t last_count;
  volatile uint32_t last_tick;
  volatile int8_t direction;
  uint8_t last_cos;
  uint8_t last_sin;
} EncoderState;

volatile EncoderState enc_left = {0, 0, 0, 0, 0, 0};
volatile EncoderState enc_right = {0, 0, 0, 0, 0, 0};

// -----------------------------------------------------------------------
// Return type
// -----------------------------------------------------------------------
extern uint8_t left_rpm;  // hetvi: extern
extern uint8_t right_rpm; // uint8_t = 1 byte for CAN payload

// -----------------------------------------------------------------------
// ISR callback
// -----------------------------------------------------------------------
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == WSS_COS_L_Pin || GPIO_Pin == WSS_SIN_L_Pin)
  {
    uint8_t cos_now = HAL_GPIO_ReadPin(WSS_COS_L_GPIO_Port, WSS_COS_L_Pin) ? 1 : 0;
    uint8_t sin_now = HAL_GPIO_ReadPin(WSS_SIN_L_GPIO_Port, WSS_SIN_L_Pin) ? 1 : 0;

    uint8_t index = (enc_left.last_cos << 3) | (enc_left.last_sin << 2) | (cos_now << 1) | sin_now;

    int8_t delta = QUAD_TABLE[index];
    if (delta != 0)
    {
      enc_left.count += delta;
      enc_left.direction = delta;
      enc_left.last_tick = HAL_GetTick();
    }
    enc_left.last_cos = cos_now;
    enc_left.last_sin = sin_now;
  }
  else if (GPIO_Pin == WSS_COS_R_Pin || GPIO_Pin == WSS_SIN_R_Pin)
  {
    uint8_t cos_now = HAL_GPIO_ReadPin(WSS_COS_R_GPIO_Port, WSS_COS_R_Pin) ? 1 : 0;
    uint8_t sin_now = HAL_GPIO_ReadPin(WSS_SIN_R_GPIO_Port, WSS_SIN_R_Pin) ? 1 : 0;

    uint8_t index = (enc_right.last_cos << 3) | (enc_right.last_sin << 2) | (cos_now << 1) | sin_now;

    int8_t delta = QUAD_TABLE[index];
    if (delta != 0)
    {
      enc_right.count += delta;
      enc_right.direction = delta;
      enc_right.last_tick = HAL_GetTick();
    }
    enc_right.last_cos = cos_now;
    enc_right.last_sin = sin_now;
  }
}

// -----------------------------------------------------------------------
// Velocity — can be called at any interval
// -----------------------------------------------------------------------
void WSS_Main(void)
{
  static uint32_t prev_call_tick = 0;

  uint32_t now = HAL_GetTick();
  uint32_t elapsed = now - prev_call_tick;
  prev_call_tick = now;

  if (elapsed == 0)
    elapsed = 1;

  __disable_irq();
  int32_t left_count = enc_left.count;
  int32_t left_prev = enc_left.last_count;
  uint32_t left_tick = enc_left.last_tick;
  enc_left.last_count = left_count;

  int32_t right_count = enc_right.count;
  int32_t right_prev = enc_right.last_count;
  uint32_t right_tick = enc_right.last_tick;
  enc_right.last_count = right_count;
  __enable_irq();

  float left_rpm_calc = 0.0f;
  if ((now - left_tick) <= STALE_MS)
  {
    float rotations = fabsf((float)(left_count - left_prev) / TICKS_PER_ROTATION);
    left_rpm_calc = rotations * (60000.0f / (float)elapsed);
  }

  float right_rpm_calc = 0.0f;
  if ((now - right_tick) <= STALE_MS)
  {
    float rotations = fabsf((float)(right_count - right_prev) / TICKS_PER_ROTATION);
    right_rpm_calc = rotations * (60000.0f / (float)elapsed);
  }

  float left_scaled = left_rpm_calc * RPM_TO_U8_SCALE;
  float right_scaled = right_rpm_calc * RPM_TO_U8_SCALE;

  left_rpm = (left_scaled >= 255.0f) ? 255 : (uint8_t)left_scaled;
  right_rpm = (right_scaled >= 255.0f) ? 255 : (uint8_t)right_scaled;
}
