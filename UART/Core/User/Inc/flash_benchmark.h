/**
 ******************************************************************************
 * @file           : flash_benchmark.h
 * @brief          : Flash Benchmark Driver for STM32F446RE
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Flash benchmark module for measuring read/write/erase cycle times on
 * reserved flash sectors. Uses the ARM Cortex-M4 DWT cycle counter for
 * precise timing measurements.
 *
 * STM32F446RE Flash Layout (512KB total, 8 sectors):
 *   Sectors 0-6: Firmware (384 KB)
 *   Sector 7:    0x08060000 - 0x0807FFFF (128 KB) - Reserved for benchmarking
 *
 * Note: Sector 0 contains the vector table and cannot be erased/written
 *       while firmware is running.
 *
 ******************************************************************************
 */

#ifndef FLASH_BENCHMARK_H
#define FLASH_BENCHMARK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  /* ============================================================================
   * Flash Sector Definitions
   * ============================================================================ */

#define FLASH_BENCH_SECTOR_7_ADDR 0x08060000U
#define FLASH_BENCH_SECTOR_7_SIZE (128U * 1024U)
#define FLASH_BENCH_SECTOR_7_NUM  7U

  /* Benchmark parameters */
#define FLASH_BENCH_READ_SIZE  1024U
#define FLASH_BENCH_WRITE_SIZE 256U

  /* ============================================================================
   * Types
   * ============================================================================ */

  typedef enum
  {
    FLASH_BENCH_OK = 0,
    FLASH_BENCH_ERR_DWT_UNAVAILABLE,
    FLASH_BENCH_ERR_INVALID_SECTOR,
    FLASH_BENCH_ERR_UNLOCK,
    FLASH_BENCH_ERR_ERASE,
    FLASH_BENCH_ERR_PROGRAM,
    FLASH_BENCH_ERR_VERIFY,
    FLASH_BENCH_ERR_LOCK,
  } FlashBench_Status_t;

  typedef struct
  {
    uint32_t cycles;
    uint32_t time_us;
    uint32_t bytes;
    uint32_t throughput_kbs;
  } FlashBench_Timing_t;

  typedef struct
  {
    FlashBench_Timing_t erase;
    FlashBench_Timing_t write;
    FlashBench_Timing_t read;
    uint32_t sector_num;
    uint32_t sector_addr;
    uint32_t sector_size;
    uint32_t cpu_freq_mhz;
  } FlashBench_Result_t;

  /* Statistics for multiple iterations */
  typedef struct
  {
    FlashBench_Timing_t min;
    FlashBench_Timing_t max;
    FlashBench_Timing_t avg;
  } FlashBench_Stats_t;

  typedef struct
  {
    FlashBench_Stats_t erase;
    FlashBench_Stats_t write;
    FlashBench_Stats_t read;
    uint32_t iterations;
    uint32_t write_pattern;
    uint32_t cpu_freq_mhz;
  } FlashBench_StatsResult_t;

  /* Callback type for async results */
  typedef void (*FlashBench_Callback_t)(const FlashBench_StatsResult_t *result);

  /* Request structure for flash task queue */
  typedef struct
  {
    uint32_t iterations;
    uint8_t write_pattern;
    FlashBench_Callback_t callback;
  } FlashBench_Request_t;

  /* ============================================================================
   * Public API
   * ============================================================================ */

  /**
   * @brief Initialize DWT cycle counter for benchmarking
   * @return FLASH_BENCH_OK on success, error code otherwise
   */
  FlashBench_Status_t FlashBench_Init(void);

  /**
   * @brief Run full benchmark on a sector (erase + write + read)
   * @param sector_num Sector number (only sector 7 is available for benchmarking)
   * @param result Pointer to result structure
   * @return Status code
   */
  FlashBench_Status_t FlashBench_RunSector(uint32_t sector_num, FlashBench_Result_t *result);

  /**
   * @brief Benchmark read operation only
   * @param addr Flash address to read from
   * @param size Number of bytes to read
   * @param timing Pointer to timing result
   * @return Status code
   */
  FlashBench_Status_t FlashBench_Read(uint32_t addr, uint32_t size, FlashBench_Timing_t *timing);

  /**
   * @brief Benchmark erase operation
   * @param sector_num Sector number to erase
   * @param timing Pointer to timing result
   * @return Status code
   */
  FlashBench_Status_t FlashBench_Erase(uint32_t sector_num, FlashBench_Timing_t *timing);

  /**
   * @brief Benchmark write operation (sector must be erased first)
   * @param addr Flash address to write to
   * @param data Data buffer to write
   * @param size Number of bytes to write (must be multiple of 4)
   * @param timing Pointer to timing result
   * @return Status code
   */
  FlashBench_Status_t FlashBench_Write(uint32_t addr, const uint8_t *data, uint32_t size,
                                       FlashBench_Timing_t *timing);

  /**
   * @brief Get sector info by number
   * @param sector_num Sector number
   * @param addr Output: sector start address (can be NULL)
   * @param size Output: sector size in bytes (can be NULL)
   * @return FLASH_BENCH_OK if valid benchmark sector, error otherwise
   */
  FlashBench_Status_t FlashBench_GetSectorInfo(uint32_t sector_num, uint32_t *addr, uint32_t *size);

  /**
   * @brief Convert cycles to microseconds based on CPU frequency
   * @param cycles DWT cycle count
   * @return Time in microseconds
   */
  uint32_t FlashBench_CyclesToUs(uint32_t cycles);

  /**
   * @brief Get CPU frequency in MHz
   * @return CPU frequency (e.g., 180 for 180 MHz)
   */
  uint32_t FlashBench_GetCpuFreqMHz(void);

  /* ============================================================================
   * FreeRTOS Task API
   * ============================================================================ */

  /**
   * @brief Flash task entry point - call from StartFlashTask
   * @param argument Task argument (unused)
   */
  void FlashBench_TaskEntry(void *argument);

  /**
   * @brief Queue a benchmark request to the flash task
   * @param request Request parameters (iterations, pattern, callback)
   * @return true if queued successfully, false if queue full
   */
  bool FlashBench_QueueRequest(const FlashBench_Request_t *request);

  /**
   * @brief Run benchmark with statistics over multiple iterations
   * @param sector_num Sector number (must be 7)
   * @param iterations Number of iterations to run
   * @param write_pattern Byte pattern to use for writes
   * @param stats Output statistics result
   * @return Status code
   */
  FlashBench_Status_t FlashBench_RunWithStats(uint32_t sector_num, uint32_t iterations,
                                              uint8_t write_pattern, FlashBench_StatsResult_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_BENCHMARK_H */
