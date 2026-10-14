/**
 * @file feb_log.h (sim shim)
 * @brief Routes the firmware LOG_* macros to the sim logger, which stamps
 *        each line with simulated time.
 */
#ifndef SIM_FEB_LOG_H
#define SIM_FEB_LOG_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
  FEB_LOG_NONE = 0,
  FEB_LOG_ERROR = 1,
  FEB_LOG_WARN = 2,
  FEB_LOG_INFO = 3,
  FEB_LOG_DEBUG = 4,
  FEB_LOG_TRACE = 5,
} FEB_Log_Level_t;

void Sim_Log(FEB_Log_Level_t level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));

#define LOG_E(tag, fmt, ...) Sim_Log(FEB_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) Sim_Log(FEB_LOG_WARN, tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) Sim_Log(FEB_LOG_INFO, tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) Sim_Log(FEB_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_T(tag, fmt, ...) Sim_Log(FEB_LOG_TRACE, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* SIM_FEB_LOG_H */
