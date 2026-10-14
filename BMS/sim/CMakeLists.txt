# BMS ECU simulator — host build (NOT part of the firmware build).
#
# Compiles the unmodified BMS state machine (Core/User/Src/FEB_SM.cpp) for
# the host and links it against a plant model instead of the STM32 HAL.
#
#   cmake -S BMS/sim -B build/bms_sim && cmake --build build/bms_sim
#   ctest --test-dir build/bms_sim --output-on-failure
#
# Use the host compiler: do not pass the repo's ARM toolchain file.

cmake_minimum_required(VERSION 3.22)
project(bms_sim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(BMS_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(REPO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Firmware sources under simulation (compiled as-is)
set(FIRMWARE_SOURCES
    ${BMS_DIR}/Core/User/Src/FEB_SM.cpp
)

add_executable(bms_sim
    ${FIRMWARE_SOURCES}
    src/sim.cpp
    src/plant.cpp
    src/bindings.cpp
    src/scenario.cpp
    src/main.cpp
    scenarios/scenarios.cpp
)

# shim/ first so it shadows the STM32 HAL, main.h and the UART logger
target_include_directories(bms_sim PRIVATE
    shim
    src
    ${BMS_DIR}/Core/User/Inc
    ${REPO_DIR}/common/FEB_Time_Library/Inc
)

target_compile_options(bms_sim PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME bms_sim_scenarios COMMAND bms_sim all -q)
