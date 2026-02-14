cmake_minimum_required(VERSION 3.22)
project(FEB_FIRMWARE_SN5 C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Common libraries (must come before board subdirectories)
add_subdirectory(common)

add_subdirectory(BMS)
add_subdirectory(DASH)
add_subdirectory(DART)
add_subdirectory(DCU)
add_subdirectory(LVPDB)
add_subdirectory(PCU)
add_subdirectory(Sensor_Nodes)
add_subdirectory(UART)
add_subdirectory(UART_TEST)
