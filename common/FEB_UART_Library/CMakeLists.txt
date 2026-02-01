# FEB UART Library - CMake Configuration
#
# This creates an INTERFACE library that boards can link to.
# Usage in board CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart)
#
# The library automatically adds:
#   - Source file (feb_uart.c)
#   - Include directories
#

# Define the library as INTERFACE since we're adding sources to the linking target
add_library(feb_uart INTERFACE)

# Add source file - INTERFACE libraries propagate sources to linking targets
target_sources(feb_uart INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/feb_uart.c
)

# Add include directories
target_include_directories(feb_uart INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For #include "FEB_UART_Library/feb_uart.h"
)
