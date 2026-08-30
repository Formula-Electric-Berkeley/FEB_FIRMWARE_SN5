# FEB Commands 2. default system commands for feb_console_2

add_library(feb_commands_2 INTERFACE)

target_sources(feb_commands_2 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_commands_2.cpp
)

target_include_directories(feb_commands_2 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

target_link_libraries(feb_commands_2 INTERFACE feb_console_2 feb_log feb_version)
