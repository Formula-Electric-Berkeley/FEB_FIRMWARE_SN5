cmake_minimum_required(VERSION 3.22)

# ── Project identity (derived from directory name) ───────────────────
get_filename_component(PROJ_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# ── Compiler settings ────────────────────────────────────────────────
set(CMAKE_C_STANDARD                11)
set(CMAKE_C_STANDARD_REQUIRED       ON)
set(CMAKE_CXX_STANDARD              20)
set(CMAKE_CXX_STANDARD_REQUIRED     ON)
set(CMAKE_C_EXTENSIONS              ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS   TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
   
project(${PROJ_NAME} C CXX ASM) 
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ... STM32 toolchain setup ...

# Include common libraries 
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common common)

# Link what you need
target_link_libraries(${PROJECT_NAME} PRIVATE
    feb_io      # UART + Log + Console + Commands
    feb_can     # CAN communication
    feb_rtos_utils # RTOS Utils
    feb_tps # For monitoring and sending power
)

