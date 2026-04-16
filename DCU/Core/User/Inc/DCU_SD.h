/**
 ******************************************************************************
 * @file           : DCU_SD.h
 * @brief          : DCU SD card smoke test helpers
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef DCU_SD_H
#define DCU_SD_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Mount the SD card, write a smoke-test file, then read it back.
   */
  void DCU_SD_RunSmokeTest(void);

#ifdef __cplusplus
}
#endif

#endif /* DCU_SD_H */
