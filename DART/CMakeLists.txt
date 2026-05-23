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
foreach(var CMAKE_C_FLAGS CMAKE_ASM_FLAGS CMAKE_EXE_LINKER_FLAGS)
    string(REPLACE "-mcpu=cortex-m4"    "" ${var} "${${var}}")
    string(REPLACE "-mfpu=fpv4-sp-d16"  "" ${var} "${${var}}")
    string(REPLACE "-mfloat-abi=hard"   "" ${var} "${${var}}")
    set(${var} "${${var}} ${DART_CPU_FLAGS}")
endforeach()

# STM32F042K6 has only 32 KB flash. -O0 Debug overflows once feb_io is linked,
# so force size-optimized debug on this board (-g3 still emitted).
set(CMAKE_C_FLAGS_DEBUG   "-Os -g3")
set(CMAKE_CXX_FLAGS_DEBUG "-Os -g3")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Compile definitions ──────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F042x6
    $<$<CONFIG:Debug>:DEBUG>
)

# ══════════════════════════════════════════════════════════════════════
#  SOURCE COLLECTION (globbed — CubeMX regens are a no-op for CMake)
# ══════════════════════════════════════════════════════════════════════

file(GLOB_RECURSE CORE_SOURCES "Core/*.c")          # Core/Src + Core/User/Src
file(GLOB         STARTUP_ASM  "startup_*.s")
file(GLOB         HAL_SOURCES  "Drivers/STM32F0xx_HAL_Driver/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

set(CAN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4")

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${STARTUP_ASM}
    ${HAL_SOURCES}
    "${CAN_LIB_DIR}/gen/feb_can.c"
)

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET
# ══════════════════════════════════════════════════════════════════════

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    Core/User/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F0xx/Include
    Drivers/CMSIS/Include
    "${CAN_LIB_DIR}/.."
    "${CAN_LIB_DIR}/gen"
)

# feb_io serial stack — bare-metal mode to fit 32 KB flash.
# Link only feb_uart + feb_log; DART-local command parser handles console.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FEB_UART_FORCE_BARE_METAL=1
    FEB_UART_MAX_INSTANCES=1
    FEB_UART_DEFAULT_TX_BUFFER_SIZE=256
    FEB_UART_DEFAULT_RX_BUFFER_SIZE=128
    FEB_UART_DEFAULT_LINE_BUFFER_SIZE=64
    FEB_UART_STAGING_BUFFER_SIZE=256
    FEB_UART_COMPILE_LOG_LEVEL=3    # INFO
)

target_link_libraries(${PROJECT_NAME} PRIVATE m feb_io feb_version)

# Embed version + build provenance (reads VERSION + git, generates feb_build_info.c)
include(${CMAKE_SOURCE_DIR}/cmake/FEB_Version.cmake)
feb_apply_version(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR})

# ── Linker script ─────────────────────────────────────────────────────
file(GLOB LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F0*_FLASH.ld")
if(LINKER_SCRIPT)
    list(GET LINKER_SCRIPT 0 LINKER_SCRIPT)
    target_link_options(${PROJECT_NAME} PRIVATE -T "${LINKER_SCRIPT}")
endif()

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.map
)

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
