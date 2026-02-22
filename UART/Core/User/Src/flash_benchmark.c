/**
 ******************************************************************************
 * @file           : flash_benchmark.c
 * @brief          : Flash Benchmark Driver Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "flash_benchmark.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"

/* ============================================================================
 * Private Defines
 * ============================================================================ */

/* DWT Lock Access Register (not in all CMSIS versions) */
#define DWT_LAR        (*(volatile uint32_t *)0xE0001FB0)
#define DWT_LAR_UNLOCK 0xC5ACCE55

/* Queue depth for benchmark requests */
#define FLASH_BENCH_QUEUE_DEPTH 4

/* ============================================================================
 * Private Variables
 * ============================================================================ */

static bool dwt_initialized = false;
static osMutexId_t flash_mutex = NULL;
static osMessageQueueId_t flash_queue = NULL;

/* ============================================================================
 * Private Functions
 * ============================================================================ */

static FlashBench_Status_t DWT_Init(void)
{
  /* Enable trace unit in CoreDebug DEMCR */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  /* Unlock DWT (required on some Cortex-M4 implementations) */
  DWT_LAR = DWT_LAR_UNLOCK;

  /* Reset cycle counter */
  DWT->CYCCNT = 0;

  /* Enable cycle counter */
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* Verify counter is running */
  uint32_t start = DWT->CYCCNT;
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  if (DWT->CYCCNT == start)
  {
    return FLASH_BENCH_ERR_DWT_UNAVAILABLE;
  }

  dwt_initialized = true;
  return FLASH_BENCH_OK;
}

static inline uint32_t DWT_GetCycles(void)
{
  return DWT->CYCCNT;
}

static void ComputeTiming(FlashBench_Timing_t *timing, uint32_t start, uint32_t end, uint32_t bytes)
{
  timing->cycles = end - start;
  timing->bytes = bytes;
  timing->time_us = FlashBench_CyclesToUs(timing->cycles);

  /* Compute throughput in KB/s, avoid division by zero */
  if (timing->time_us > 0)
  {
    timing->throughput_kbs = (bytes * 1000U) / timing->time_us;
  }
  else
  {
    timing->throughput_kbs = 0;
  }
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

FlashBench_Status_t FlashBench_Init(void)
{
  if (dwt_initialized)
  {
    return FLASH_BENCH_OK;
  }
  return DWT_Init();
}

FlashBench_Status_t FlashBench_GetSectorInfo(uint32_t sector_num, uint32_t *addr, uint32_t *size)
{
  /* Only sector 7 is available for benchmarking.
   * Sectors 0-6 contain running firmware and cannot be erased. */
  if (sector_num != 7)
  {
    return FLASH_BENCH_ERR_INVALID_SECTOR;
  }

  if (addr != NULL)
  {
    *addr = FLASH_BENCH_SECTOR_7_ADDR;
  }
  if (size != NULL)
  {
    *size = FLASH_BENCH_SECTOR_7_SIZE;
  }

  return FLASH_BENCH_OK;
}

uint32_t FlashBench_CyclesToUs(uint32_t cycles)
{
  uint32_t freq_mhz = FlashBench_GetCpuFreqMHz();
  if (freq_mhz == 0)
  {
    return 0;
  }
  return cycles / freq_mhz;
}

uint32_t FlashBench_GetCpuFreqMHz(void)
{
  return SystemCoreClock / 1000000U;
}

FlashBench_Status_t FlashBench_Read(uint32_t addr, uint32_t size, FlashBench_Timing_t *timing)
{
  volatile uint32_t *flash_ptr = (volatile uint32_t *)addr;
  volatile uint32_t dummy = 0;
  uint32_t words = size / sizeof(uint32_t);

  uint32_t start = DWT_GetCycles();
  for (uint32_t i = 0; i < words; i++)
  {
    dummy ^= flash_ptr[i];
  }
  uint32_t end = DWT_GetCycles();

  (void)dummy; /* Suppress unused warning */

  ComputeTiming(timing, start, end, size);
  return FLASH_BENCH_OK;
}

FlashBench_Status_t FlashBench_Erase(uint32_t sector_num, FlashBench_Timing_t *timing)
{
  uint32_t sector_size;
  FlashBench_Status_t status = FlashBench_GetSectorInfo(sector_num, NULL, &sector_size);
  if (status != FLASH_BENCH_OK)
  {
    return status;
  }

  /* Acquire mutex for task-level serialization */
  if (flash_mutex != NULL)
  {
    osMutexAcquire(flash_mutex, osWaitForever);
  }

  FLASH_EraseInitTypeDef erase_init;
  uint32_t sector_error;

  erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase_init.Sector = sector_num;
  erase_init.NbSectors = 1;
  erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* 2.7V - 3.6V */

  /* Enter critical section for flash unlock and flag clear */
  taskENTER_CRITICAL();

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    taskEXIT_CRITICAL();
    if (flash_mutex != NULL)
    {
      osMutexRelease(flash_mutex);
    }
    return FLASH_BENCH_ERR_UNLOCK;
  }

  /* Clear any pending flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                         FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

  taskEXIT_CRITICAL();

  /* Flash erase runs with interrupts enabled (HAL handles wait states internally) */
  uint32_t start = DWT_GetCycles();
  HAL_StatusTypeDef hal_status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
  uint32_t end = DWT_GetCycles();

  taskENTER_CRITICAL();
  HAL_FLASH_Lock();
  taskEXIT_CRITICAL();

  if (flash_mutex != NULL)
  {
    osMutexRelease(flash_mutex);
  }

  if (hal_status != HAL_OK)
  {
    return FLASH_BENCH_ERR_ERASE;
  }

  ComputeTiming(timing, start, end, sector_size);
  return FLASH_BENCH_OK;
}

FlashBench_Status_t FlashBench_Write(uint32_t addr, const uint8_t *data, uint32_t size,
                                     FlashBench_Timing_t *timing)
{
  uint32_t words = size / sizeof(uint32_t);
  const uint32_t *word_data = (const uint32_t *)data;

  /* Acquire mutex for task-level serialization */
  if (flash_mutex != NULL)
  {
    osMutexAcquire(flash_mutex, osWaitForever);
  }

  /* Enter critical section for flash unlock and flag clear */
  taskENTER_CRITICAL();

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    taskEXIT_CRITICAL();
    if (flash_mutex != NULL)
    {
      osMutexRelease(flash_mutex);
    }
    return FLASH_BENCH_ERR_UNLOCK;
  }

  /* Clear any pending flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                         FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

  taskEXIT_CRITICAL();

  /* Flash program runs with interrupts enabled (HAL handles wait states internally) */
  uint32_t start = DWT_GetCycles();
  for (uint32_t i = 0; i < words; i++)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (i * sizeof(uint32_t)), word_data[i]) != HAL_OK)
    {
      taskENTER_CRITICAL();
      HAL_FLASH_Lock();
      taskEXIT_CRITICAL();
      if (flash_mutex != NULL)
      {
        osMutexRelease(flash_mutex);
      }
      return FLASH_BENCH_ERR_PROGRAM;
    }
  }
  uint32_t end = DWT_GetCycles();

  taskENTER_CRITICAL();
  HAL_FLASH_Lock();
  taskEXIT_CRITICAL();

  if (flash_mutex != NULL)
  {
    osMutexRelease(flash_mutex);
  }

  ComputeTiming(timing, start, end, size);
  return FLASH_BENCH_OK;
}

static FlashBench_Status_t FlashBench_RunSectorWithPattern(uint32_t sector_num, uint8_t pattern,
                                                           FlashBench_Result_t *result)
{
  FlashBench_Status_t status;
  uint32_t addr, size;

  /* Validate sector and get info */
  status = FlashBench_GetSectorInfo(sector_num, &addr, &size);
  if (status != FLASH_BENCH_OK)
  {
    return status;
  }

  /* Initialize DWT if needed */
  status = FlashBench_Init();
  if (status != FLASH_BENCH_OK)
  {
    return status;
  }

  result->sector_num = sector_num;
  result->sector_addr = addr;
  result->sector_size = size;
  result->cpu_freq_mhz = FlashBench_GetCpuFreqMHz();

  /* 1. Benchmark erase */
  status = FlashBench_Erase(sector_num, &result->erase);
  if (status != FLASH_BENCH_OK)
  {
    return status;
  }

  /* 2. Benchmark write with specified pattern */
  static uint8_t test_pattern[FLASH_BENCH_WRITE_SIZE];
  for (uint32_t i = 0; i < sizeof(test_pattern); i++)
  {
    test_pattern[i] = pattern;
  }
  status = FlashBench_Write(addr, test_pattern, sizeof(test_pattern), &result->write);
  if (status != FLASH_BENCH_OK)
  {
    return status;
  }

  /* 3. Benchmark read */
  status = FlashBench_Read(addr, FLASH_BENCH_READ_SIZE, &result->read);

  return status;
}

FlashBench_Status_t FlashBench_RunSector(uint32_t sector_num, FlashBench_Result_t *result)
{
  return FlashBench_RunSectorWithPattern(sector_num, 0xAA, result);
}

/* ============================================================================
 * Statistics Helper Functions
 * ============================================================================ */

static void UpdateTimingStats(FlashBench_Stats_t *stats, const FlashBench_Timing_t *timing, uint32_t iter)
{
  if (iter == 0)
  {
    /* First iteration - initialize min/max/avg */
    stats->min = *timing;
    stats->max = *timing;
    stats->avg = *timing;
  }
  else
  {
    /* Update min */
    if (timing->cycles < stats->min.cycles)
    {
      stats->min = *timing;
    }
    /* Update max */
    if (timing->cycles > stats->max.cycles)
    {
      stats->max = *timing;
    }
    /* Update running average (simple mean) */
    stats->avg.cycles = ((stats->avg.cycles * iter) + timing->cycles) / (iter + 1);
    stats->avg.time_us = ((stats->avg.time_us * iter) + timing->time_us) / (iter + 1);
    stats->avg.bytes = timing->bytes;
    stats->avg.throughput_kbs = ((stats->avg.throughput_kbs * iter) + timing->throughput_kbs) / (iter + 1);
  }
}

FlashBench_Status_t FlashBench_RunWithStats(uint32_t sector_num, uint32_t iterations, uint8_t write_pattern,
                                            FlashBench_StatsResult_t *stats)
{
  FlashBench_Status_t status;
  FlashBench_Result_t result;

  if (iterations == 0)
  {
    return FLASH_BENCH_ERR_INVALID_SECTOR;
  }

  stats->iterations = iterations;
  stats->write_pattern = write_pattern;
  stats->cpu_freq_mhz = FlashBench_GetCpuFreqMHz();

  for (uint32_t i = 0; i < iterations; i++)
  {
    status = FlashBench_RunSectorWithPattern(sector_num, write_pattern, &result);
    if (status != FLASH_BENCH_OK)
    {
      return status;
    }

    UpdateTimingStats(&stats->erase, &result.erase, i);
    UpdateTimingStats(&stats->write, &result.write, i);
    UpdateTimingStats(&stats->read, &result.read, i);
  }

  return FLASH_BENCH_OK;
}

/* ============================================================================
 * FreeRTOS Task Functions
 * ============================================================================ */

void FlashBench_TaskEntry(void *argument)
{
  (void)argument;

  /* Initialize mutex and queue */
  flash_mutex = osMutexNew(NULL);
  flash_queue = osMessageQueueNew(FLASH_BENCH_QUEUE_DEPTH, sizeof(FlashBench_Request_t), NULL);

  /* Initialize DWT cycle counter */
  FlashBench_Init();

  /* Task main loop */
  for (;;)
  {
    FlashBench_Request_t req;
    if (osMessageQueueGet(flash_queue, &req, NULL, osWaitForever) == osOK)
    {
      FlashBench_StatsResult_t stats;
      FlashBench_Status_t status = FlashBench_RunWithStats(7, req.iterations, req.write_pattern, &stats);

      if (status == FLASH_BENCH_OK && req.callback != NULL)
      {
        req.callback(&stats);
      }
    }
  }
}

bool FlashBench_QueueRequest(const FlashBench_Request_t *request)
{
  if (flash_queue == NULL || request == NULL)
  {
    return false;
  }
  return osMessageQueuePut(flash_queue, request, 0, 0) == osOK;
}
