cmake_minimum_required(VERSION 3.22)
project(FEB_FIRMWARE_SN5 C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Per-project build toggles (VSCode CMake extension shows these as options)
option(BUILD_BMS          "Build BMS firmware"          OFF)
option(BUILD_DASH         "Build DASH firmware"         OFF)
option(BUILD_DART         "Build DART firmware"         OFF)
option(BUILD_DCU          "Build DCU firmware"          OFF)
option(BUILD_LVPDB        "Build LVPDB firmware"        ON)
option(BUILD_PCU          "Build PCU firmware"          OFF)
option(BUILD_Sensor_Nodes "Build Sensor_Nodes firmware" OFF)

if(BUILD_BMS)
    add_subdirectory(BMS)
endif()

if(BUILD_DASH)
    add_subdirectory(DASH)
endif()

if(BUILD_DART)
    add_subdirectory(DART)
endif()

if(BUILD_DCU)
    add_subdirectory(DCU)
endif()

if(BUILD_LVPDB)
    add_subdirectory(LVPDB)
endif()

if(BUILD_PCU)
    add_subdirectory(PCU)
endif()

if(BUILD_Sensor_Nodes)
    add_subdirectory(Sensor_Nodes)
endif()
