# FEB Console Library
# Command-line interface for UART console with case-insensitive commands
# Uses pipe (|) as argument delimiter - spaces within arguments are preserved

add_library(feb_console INTERFACE)

target_sources(feb_console INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_console.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_console_commands.c
)

target_include_directories(feb_console INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For #include "FEB_Console_Library/..."
)

# Console depends on UART library for output
target_link_libraries(feb_console INTERFACE feb_uart)
