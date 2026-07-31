add_library(feb_console_2 INTERFACE)

target_sources(feb_console_2 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_console_2.cpp
)

target_include_directories(feb_console_2 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# UART for output, feb_time for row timestamps, feb_version for board_name
target_link_libraries(feb_console_2 INTERFACE feb_uart feb_time feb_version)
