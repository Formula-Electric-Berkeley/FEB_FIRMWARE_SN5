cmake_minimum_required(VERSION 3.14)
project(BMS_Tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_compile_definitions(UNIT_TEST)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../Core/User/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../FEB_CAN_Library_SN4/gen
)

set(TEST_SOURCES
    unit/test_printf_redirect.cpp
    ../Core/User/Src/printf_redirect.c
    ../FEB_CAN_Library_SN4/gen/feb_can.c
    mocks/stm32f4xx_hal.c
    mocks/freertos_mock.c
)

add_executable(bms_tests ${TEST_SOURCES})

target_link_libraries(bms_tests 
    gtest_main
    gmock
    m
)

include(GoogleTest)
gtest_discover_tests(bms_tests 
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_tests
    COMMAND bms_tests
    DEPENDS bms_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_command(TARGET bms_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Tests built successfully. Run with: make run_tests"
)