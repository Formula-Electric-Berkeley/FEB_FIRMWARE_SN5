# FEB Log Library CMake Configuration
# ---------------------------------------------------------------------------
# Standalone logging library with built-in UART routing or custom output.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_log)
#
# In your code:
#   #include "feb_log.h"
#
#   // Initialize with config struct (built-in UART routing)
#   FEB_Log_Config_t log_cfg = {
#       .uart_instance = FEB_UART_INSTANCE_1,
#       .level = FEB_LOG_INFO,
#       .colors = true,
#       .timestamps = true,
#       .get_tick_ms = HAL_GetTick,
#   };
#   FEB_Log_Init(&log_cfg);
#
#   // Then use logging macros
#   LOG_I(TAG_MAIN, "System started");
# ---------------------------------------------------------------------------

add_library(feb_log INTERFACE)

target_sources(feb_log INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_log.c
)

target_include_directories(feb_log INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # For #include "FEB_Serial_Library/FEB_Log/feb_log.h"
)

# FEB_Log uses FEB_UART for built-in output routing
target_link_libraries(feb_log INTERFACE feb_uart)
