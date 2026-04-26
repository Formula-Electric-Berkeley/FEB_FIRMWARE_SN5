# FEB Console Library CMake Configuration
# ---------------------------------------------------------------------------
# Command-line interface for UART with pipe-delimited arguments.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_console)
#
# In your code:
#   #include "feb_console.h"
#
#   FEB_Console_Init(false);  // Don't register default commands
#   FEB_Console_Register(&my_cmd);
#   FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);
# ---------------------------------------------------------------------------

add_library(feb_console INTERFACE)

target_sources(feb_console INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_console.c
)

target_include_directories(feb_console INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # For #include "FEB_Serial_Library/FEB_Console/..."
)

# Console depends on UART for output and feb_time for CSV row timestamps
target_link_libraries(feb_console INTERFACE feb_uart feb_time)
