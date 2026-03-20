# FEB Serial Library - Unified Serial Stack
# ===========================================================================
# This is the parent CMake file for all FEB I/O libraries. It provides a
# modular architecture where each component can be used independently or
# together as a complete stack.
#
# Architecture:
#   feb_uart     - Core UART driver (DMA, line/binary modes, framing)
#   feb_log      - Logging with configurable output backend
#   feb_console  - Command-line interface (depends on feb_uart)
#   feb_commands - Default system commands (depends on feb_console, feb_log)
#   feb_io       - Convenience target linking all of the above
#
# Usage Examples:
#
#   # Full I/O stack (recommended for most boards):
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_io)
#
#   # UART only (for device-to-device communication):
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart)
#
#   # UART + Logging (no console):
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart feb_log)
#
#   # Console without default commands:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_console)
#
# ===========================================================================

# Core UART driver - standalone, no dependencies on other FEB libraries
add_subdirectory(FEB_UART)

# Logging library - standalone, user provides output function
add_subdirectory(FEB_Log)

# Console library - depends on UART for output
add_subdirectory(FEB_Console)

# Default system commands - depends on Console and Log
add_subdirectory(FEB_Commands)

# ---------------------------------------------------------------------------
# Convenience target: links all I/O libraries
# ---------------------------------------------------------------------------
# This provides a single target for boards that want the full I/O stack.
# Linking feb_io automatically brings in UART, Log, Console, and Commands.
add_library(feb_io INTERFACE)
target_link_libraries(feb_io INTERFACE
    feb_uart
    feb_log
    feb_console
    feb_commands
)
