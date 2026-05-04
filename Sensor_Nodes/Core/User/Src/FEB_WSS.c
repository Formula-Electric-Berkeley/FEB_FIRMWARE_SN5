#include "FEB_WSS.h"
#include "main.h"
#include "tim.h"
#include "feb_log.h"

#define TAG_WSS "[WSS]"

// =====================================================================
// Configuration
// =====================================================================
#define WSS_PPR 84u                                       // mechanical pulses per revolution
#define WSS_QUADRATURE_MULT 4u                            // 4x edges per pulse
#define WSS_EDGES_PER_REV (WSS_PPR * WSS_QUADRATURE_MULT) // 336

#define WSS_RING_LEN 16u     // power of two, used as ring buffer for edge timestamps
#define WSS_STALE_US 200000u // 200 ms without an edge -> wheel considered stopped
#define WSS_RPM_X10_MAX 65535u

// Quadrature decode table indexed by ((last_cos << 3) | (last_sin << 2) | (cos_now << 1) | sin_now).
static const int8_t QUAD_TABLE[16] = {0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0};

typedef struct
{
  volatile uint32_t ts[WSS_RING_LEN]; // TIM5 µs timestamp on each valid edge
  volatile int8_t dir[WSS_RING_LEN];  // +/-1 from QUAD_TABLE
  volatile uint8_t head;              // index of newest entry
  volatile uint8_t fill;              // count of valid entries (0..WSS_RING_LEN)
  // Signed accumulator of quadrature edges. Incremented in EXTI by +/-1 per
  // valid transition (336 counts = one full revolution). 32-bit aligned so
  // the main loop can read it without disabling interrupts.
  volatile int32_t pos_edges;
  uint8_t last_cos;
  uint8_t last_sin;
} WheelState;

static volatile WheelState wheel_left = {0};
static volatile WheelState wheel_right = {0};

uint16_t left_rpm_x10 = 0;
uint16_t right_rpm_x10 = 0;
int8_t left_dir = 0;
int8_t right_dir = 0;

static inline uint32_t tim5_us(void)
{
  return __HAL_TIM_GET_COUNTER(&htim5);
}

void FEB_WSS_Init(void)
{
  // Counter must be running for µs timestamps.  Start is idempotent.
  HAL_TIM_Base_Start(&htim5);

  // Seed the last-known phase from the actual pin levels so the first edge produces a valid delta.
  wheel_left.last_cos = HAL_GPIO_ReadPin(WSS_COS_L_GPIO_Port, WSS_COS_L_Pin) ? 1u : 0u;
  wheel_left.last_sin = HAL_GPIO_ReadPin(WSS_SIN_L_GPIO_Port, WSS_SIN_L_Pin) ? 1u : 0u;
  wheel_right.last_cos = HAL_GPIO_ReadPin(WSS_COS_R_GPIO_Port, WSS_COS_R_Pin) ? 1u : 0u;
  wheel_right.last_sin = HAL_GPIO_ReadPin(WSS_SIN_R_GPIO_Port, WSS_SIN_R_Pin) ? 1u : 0u;
}

static inline void wheel_record_edge(volatile WheelState *w, uint8_t cos_now, uint8_t sin_now)
{
  const uint8_t idx = (uint8_t)((w->last_cos << 3) | (w->last_sin << 2) | (cos_now << 1) | sin_now);
  const int8_t delta = QUAD_TABLE[idx];
  w->last_cos = cos_now;
  w->last_sin = sin_now;
  if (delta == 0)
  {
    return; // glitch / no transition
  }
  w->pos_edges += delta;
  const uint8_t h = (uint8_t)((w->head + 1u) & (WSS_RING_LEN - 1u));
  w->ts[h] = tim5_us();
  w->dir[h] = delta;
  w->head = h;
  if (w->fill < WSS_RING_LEN)
  {
    w->fill++;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == WSS_COS_L_Pin || GPIO_Pin == WSS_SIN_L_Pin)
  {
    const uint8_t cos_now = HAL_GPIO_ReadPin(WSS_COS_L_GPIO_Port, WSS_COS_L_Pin) ? 1u : 0u;
    const uint8_t sin_now = HAL_GPIO_ReadPin(WSS_SIN_L_GPIO_Port, WSS_SIN_L_Pin) ? 1u : 0u;
    wheel_record_edge(&wheel_left, cos_now, sin_now);
  }
  else if (GPIO_Pin == WSS_COS_R_Pin || GPIO_Pin == WSS_SIN_R_Pin)
  {
    const uint8_t cos_now = HAL_GPIO_ReadPin(WSS_COS_R_GPIO_Port, WSS_COS_R_Pin) ? 1u : 0u;
    const uint8_t sin_now = HAL_GPIO_ReadPin(WSS_SIN_R_GPIO_Port, WSS_SIN_R_Pin) ? 1u : 0u;
    wheel_record_edge(&wheel_right, cos_now, sin_now);
  }
}

// Compute RPM from the per-edge timestamp ring.  Algorithm:
//   - Snapshot up to WSS_RING_LEN of the most recent edges with IRQ off.
//   - net_edges = sum(dir[]) over snapshot.  period_us = newest_ts - oldest_ts.
//   - rpm = (|net_edges| / WSS_EDGES_PER_REV) * (60e6 / period_us).
//   - sign = sign(net_edges); store as dir flag.
//   - If newest edge older than WSS_STALE_US, wheel is stopped.
static void compute_wheel_rpm(volatile WheelState *w, uint16_t *rpm_x10_out, int8_t *dir_out)
{
  uint8_t fill;
  uint8_t head;
  uint32_t ts_snapshot[WSS_RING_LEN];
  int8_t dir_snapshot[WSS_RING_LEN];

  __disable_irq();
  fill = w->fill;
  head = w->head;
  for (uint8_t i = 0; i < fill; i++)
  {
    const uint8_t idx = (uint8_t)((head + WSS_RING_LEN - i) & (WSS_RING_LEN - 1u));
    ts_snapshot[i] = w->ts[idx];
    dir_snapshot[i] = w->dir[idx];
  }
  __enable_irq();

  const uint32_t now = tim5_us();

  if (fill < 2u)
  {
    *rpm_x10_out = 0;
    *dir_out = 0;
    return;
  }

  const uint32_t newest_ts = ts_snapshot[0];
  const uint32_t oldest_ts = ts_snapshot[fill - 1u];

  if ((uint32_t)(now - newest_ts) > WSS_STALE_US)
  {
    *rpm_x10_out = 0;
    *dir_out = 0;
    return;
  }

  const uint32_t period_us = (uint32_t)(newest_ts - oldest_ts);
  if (period_us == 0u)
  {
    *rpm_x10_out = 0;
    *dir_out = 0;
    return;
  }

  int32_t net_edges = 0;
  for (uint8_t i = 0; i < fill; i++)
  {
    net_edges += dir_snapshot[i];
  }

  const int8_t sign = (net_edges >= 0) ? (int8_t) + 1 : (int8_t)-1;
  const uint32_t abs_edges = (uint32_t)((net_edges >= 0) ? net_edges : -net_edges);

  // rpm = abs_edges / EDGES_PER_REV * 60e6 / period_us
  // rpm_x10 = abs_edges * 600e6 / (EDGES_PER_REV * period_us)
  const uint64_t numer = (uint64_t)abs_edges * 600000000ull;
  const uint64_t denom = (uint64_t)WSS_EDGES_PER_REV * (uint64_t)period_us;
  const uint64_t rpm_x10 = (denom == 0ull) ? 0ull : (numer / denom);

  *rpm_x10_out = (rpm_x10 >= WSS_RPM_X10_MAX) ? (uint16_t)WSS_RPM_X10_MAX : (uint16_t)rpm_x10;
  *dir_out = (abs_edges == 0u) ? (int8_t)0 : sign;
}

void WSS_Main(void)
{
  compute_wheel_rpm(&wheel_left, &left_rpm_x10, &left_dir);
  compute_wheel_rpm(&wheel_right, &right_rpm_x10, &right_dir);

  LOG_D(TAG_WSS, "L: pos=%ld rpm_x10=%u dir=%d | R: pos=%ld rpm_x10=%u dir=%d", (long)wheel_left.pos_edges,
        (unsigned)left_rpm_x10, (int)left_dir, (long)wheel_right.pos_edges, (unsigned)right_rpm_x10, (int)right_dir);
}
