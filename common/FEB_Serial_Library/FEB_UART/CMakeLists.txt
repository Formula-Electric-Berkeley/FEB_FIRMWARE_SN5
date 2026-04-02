# FEB UART Library CMake Configuration
# ---------------------------------------------------------------------------
# Core UART driver with DMA support, multiple modes, and framing.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart)
#
# In your code:
#   #include "feb_uart.h"
#
#   // Line mode (for console):
#   FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg);
#   FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, my_line_callback);
#
#   // Binary mode (for device-to-device):
#   FEB_UART_SetMode(FEB_UART_INSTANCE_1, FEB_UART_MODE_BINARY);
#   FEB_UART_SetFramingConfig(FEB_UART_INSTANCE_1, &framing);
#   FEB_UART_SetRxBinaryCallback(FEB_UART_INSTANCE_1, my_binary_callback, 0, 10);
# ---------------------------------------------------------------------------

add_library(feb_uart INTERFACE)

target_sources(feb_uart INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_uart.c
)

target_include_directories(feb_uart INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # For #include "FEB_Serial_Library/FEB_UART/..."
)
