# FEB ADBMS Library - CMake Configuration
#
# ADBMS6830B chip driver library with platform abstraction via weak functions

add_library(feb_adbms INTERFACE)

target_sources(feb_adbms INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/ADBMS6830B_Registers.c
)

target_include_directories(feb_adbms INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
