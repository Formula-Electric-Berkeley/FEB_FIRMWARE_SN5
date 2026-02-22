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
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Compile definitions ──────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F446xx
    $<$<CONFIG:Debug>:DEBUG>
)

# ══════════════════════════════════════════════════════════════════════
#  SOURCE COLLECTION
# ══════════════════════════════════════════════════════════════════════

# Core application sources (CubeMX-generated + user code)
file(GLOB_RECURSE CORE_SOURCES "Core/*.c")

# Startup assembly
file(GLOB STARTUP_ASM "startup_*.s")

# HAL/LL driver sources (safe: HAL_MODULE_ENABLED guards; exclude templates)
file(GLOB HAL_SOURCES "Drivers/STM32F4xx_HAL_Driver/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

# CAN library generated source (from common submodule)
set(CAN_GEN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4/gen/feb_can.c")

# Combine base sources
set(ALL_SOURCES ${CORE_SOURCES} ${STARTUP_ASM} ${HAL_SOURCES} ${CAN_GEN_SOURCE})

# ══════════════════════════════════════════════════════════════════════
#  INCLUDE DIRECTORIES
# ══════════════════════════════════════════════════════════════════════

set(BASE_INCLUDES
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
)

# Auto-discover Core/User/Inc if it exists
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Core/User/Inc")
    list(APPEND BASE_INCLUDES Core/User/Inc)
endif()

# Common CAN library includes (submodule)
set(CAN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4")
list(APPEND BASE_INCLUDES
    "${CAN_LIB_DIR}/.."     # resolves #include "FEB_CAN_Library_SN4/gen/feb_can.h"
    "${CAN_LIB_DIR}/gen"    # resolves #include "feb_can.h" directly
)

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET
# ══════════════════════════════════════════════════════════════════════

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${BASE_INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE m feb_uart feb_console feb_can)

# Enable floating-point printf/scanf for newlib-nano
target_link_options(${PROJECT_NAME} PRIVATE
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)

# Linker script and map file (project-specific, not in shared toolchain)
file(GLOB LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F4*_FLASH.ld")
if(LINKER_SCRIPT)
    list(GET LINKER_SCRIPT 0 LINKER_SCRIPT)
    target_link_options(${PROJECT_NAME} PRIVATE -T "${LINKER_SCRIPT}")
endif()
target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.map
)

# Remove wrong libob.a dependency
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ── Post-build: .bin, .hex, size ─────────────────────────────────────
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
