cmake_minimum_required(VERSION 3.22)
project(FEB_FIRMWARE_SN5 C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_subdirectory(BMS)
add_subdirectory(DASH)
add_subdirectory(DART)
add_subdirectory(DCU)
add_subdirectory(LVPDB)
add_subdirectory(PCU)
add_subdirectory(Sensor_Nodes)
add_subdirectory(RES_EBS)
