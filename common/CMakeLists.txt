# FEB Common Libraries
# ===========================================================================
# This file aggregates all shared libraries in common/.
# Boards can link to these libraries using target_link_libraries().
#
# See README.md for usage documentation.
#
# Available targets:
#   feb_io       - Full I/O stack (UART + Log + Console + Commands)
#   feb_uart     - Core UART driver
#   feb_log      - Logging with configurable output
#   feb_console  - Command-line interface
#   feb_commands - Default system commands
#   feb_can      - FreeRTOS-safe CAN communication
#   feb_tps      - TPS2482 power monitoring
# ===========================================================================

# Serial Library - UART, Logging, Console, Commands (unified stack)
add_subdirectory(FEB_Serial_Library)

# CAN Library - FreeRTOS-safe CAN communication with registration/callback pattern
add_subdirectory(FEB_CAN_Library)

# TPS Library - TPS2482 power monitoring with FreeRTOS support
add_subdirectory(FEB_TPS_Library)
