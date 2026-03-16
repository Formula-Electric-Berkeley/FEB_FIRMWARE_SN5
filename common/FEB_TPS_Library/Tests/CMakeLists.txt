# CMakeLists.txt for FEB_TPS_Library host-based unit tests
#
# Builds a native (Linux) test binary that links against a stub HAL mock
# instead of the real STM32 HAL. This allows the library logic to be tested
# without any embedded toolchain.
#
# Usage (from repo root or this directory):
#   mkdir build_tests && cd build_tests
#   cmake ../common/FEB_TPS_Library/Tests
#   make
#   ./test_feb_tps

cmake_minimum_required(VERSION 3.16)
project(feb_tps_tests C)

set(CMAKE_C_STANDARD 11)

# Source files
set(TEST_SOURCES
    test_feb_tps.c
    hal_mock.c
    # Library implementation compiled for host
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/feb_tps.c
)

add_executable(test_feb_tps ${TEST_SOURCES})

# Include paths
target_include_directories(test_feb_tps PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}           # hal_mock.h (acts as stm32f4xx_hal.h)
    ${CMAKE_CURRENT_SOURCE_DIR}/../Inc    # feb_tps.h, feb_tps_config.h, etc.
)

# Compiler definitions
target_compile_definitions(test_feb_tps PRIVATE
    # Override the HAL chip detection in feb_tps.c so it falls through to main.h,
    # which we redirect to hal_mock.h via the -include flag below.
    # We use a define that is not any of the STM32 variants so it falls through
    # to the fallback "include main.h" path — then we provide our own main.h.
    FEB_TPS_USE_FREERTOS=0
)

# Provide a fake "main.h" that includes our mock HAL
# We create it as stm32f4xx_hal.h alias so the fallback works;
# actually we use the compiler -include trick to inject hal_mock.h first.
target_compile_options(test_feb_tps PRIVATE
    # Inject hal_mock.h as the first include so all HAL types are available
    -include ${CMAKE_CURRENT_SOURCE_DIR}/hal_mock.h
    # Treat the test include dir as a system dir so it satisfies #include "main.h"
    -iquote ${CMAKE_CURRENT_SOURCE_DIR}
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Math library for fabsf()
target_link_libraries(test_feb_tps m)

# CTest integration
enable_testing()
add_test(NAME feb_tps_unit_tests COMMAND test_feb_tps)