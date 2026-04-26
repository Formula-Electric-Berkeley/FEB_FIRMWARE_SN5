# FEB Commands Library CMake Configuration
# ---------------------------------------------------------------------------
# Default system commands for the FEB Console.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_commands)
#
# In your code:
#   #include "feb_commands.h"
#
#   // Register all system commands
#   FEB_Commands_RegisterSystem();
#
#   // Or register individual commands:
#   #include "feb_commands_system.h"
#   FEB_Console_Register(&feb_cmd_help);
# ---------------------------------------------------------------------------

add_library(feb_commands INTERFACE)

target_sources(feb_commands INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_commands_system.c
)

target_include_directories(feb_commands INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # For #include "FEB_Serial_Library/FEB_Commands/..."
)

# Commands depends on Console, Log, and Version libraries
target_link_libraries(feb_commands INTERFACE feb_console feb_log feb_version)
