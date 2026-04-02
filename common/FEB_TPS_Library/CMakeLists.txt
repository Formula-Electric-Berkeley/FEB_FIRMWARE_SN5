# FEB TPS Library - CMake Configuration
#
# TPS2482 power monitoring library with FreeRTOS support

add_library(feb_tps INTERFACE)

target_sources(feb_tps INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_tps.c
)

target_include_directories(feb_tps INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
