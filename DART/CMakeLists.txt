cmake_minimum_required(VERSION 3.22)

# ── Project identity (derived from directory name) ───────────────────
get_filename_component(PROJ_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# ── Compiler settings ────────────────────────────────────────────────
set(CMAKE_C_STANDARD                11)
set(CMAKE_C_STANDARD_REQUIRED       ON)
set(CMAKE_C_EXTENSIONS              ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS   TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(${PROJ_NAME} C CXX ASM)

# Override root-level M4 flags — DART is Cortex-M0, no FPU
set(DART_CPU_FLAGS "-mcpu=cortex-m0 -mthumb")
string(REPLACE "-mcpu=cortex-m4" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-mfpu=fpv4-sp-d16" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-mfloat-abi=hard" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-mcpu=cortex-m4" "" CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}")
string(REPLACE "-mfpu=fpv4-sp-d16" "" CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}")
string(REPLACE "-mfloat-abi=hard" "" CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}")
string(REPLACE "-mcpu=cortex-m4" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
string(REPLACE "-mfpu=fpv4-sp-d16" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
string(REPLACE "-mfloat-abi=hard" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${DART_CPU_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${DART_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${DART_CPU_FLAGS}")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET (must be created before subdirectories)
# ══════════════════════════════════════════════════════════════════════

add_executable(${PROJECT_NAME})

# ── User application sources (FEB files) ─────────────────────────────
target_sources(${PROJECT_NAME} PRIVATE
    Core/User/Src/FEB_CAN.c
    Core/User/Src/FEB_CAN_BMS.c
    Core/User/Src/FEB_Fan.c
    Core/User/Src/FEB_main.c
)

# ── Include directories ───────────────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    Core/User/Inc
)

# Common CAN library includes
set(CAN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4")
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CAN_LIB_DIR}/.."       # resolves #include "FEB_CAN_Library_SN4/gen/feb_can.h"
    "${CAN_LIB_DIR}/gen"      # resolves #include "feb_can.h" directly
)

# CAN library generated source
target_sources(${PROJECT_NAME} PRIVATE
    "${CAN_LIB_DIR}/gen/feb_can.c"
)

# ── Bring in CubeMX-generated HAL + application sources ──────────────
add_subdirectory(cmake/stm32cubemx)

# ── Link libraries ────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE m)

# ── Linker script ─────────────────────────────────────────────────────
file(GLOB LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F0*_FLASH.ld")
if(LINKER_SCRIPT)
    list(GET LINKER_SCRIPT 0 LINKER_SCRIPT)
    target_link_options(${PROJECT_NAME} PRIVATE -T "${LINKER_SCRIPT}")
endif()

target_link_options(${PROJECT_NAME} PRIVATE
    --specs=nano.specs
    -Wl,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

# Remove wrong libob.a dependency
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ── Post-build: .bin, .hex, size ──────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating .bin/.hex for ${PROJECT_NAME}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    ADDITIONAL_CLEAN_FILES "${PROJECT_NAME}.map;${PROJECT_NAME}.bin;${PROJECT_NAME}.hex"
)