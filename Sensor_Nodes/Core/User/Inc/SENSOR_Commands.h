#ifndef SENSOR_COMMANDS_H
#define SENSOR_COMMANDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void SENSOR_Console_ProcessLine(const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_COMMANDS_H */
