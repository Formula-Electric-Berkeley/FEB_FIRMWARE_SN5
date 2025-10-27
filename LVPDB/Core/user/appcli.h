#pragma once
#include "uart_cli.h"

// The bridge between UART and our CAN and I2C protocols

extern const cli_cmd_t APP_CLI_CMDS[];  // exposes "get voltage", "get temp"
extern const uint16_t  APP_CLI_CMDS_N;