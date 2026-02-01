cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#
include_directories(
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

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

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/User/Src/bms_tasks.c
    Core/User/Src/FEB_HW.c
    Core/User/Src/FEB_AD68xx_Interface.c
    Core/User/Src/FEB_ADBMS6830B.c
    Core/User/Src/FEB_ADBMS6830B_Driver.c
    Core/User/Src/FEB_SM.c
    Core/User/Src/FEB_CAN_IVT.c
    Core/User/Src/FEB_i2c_protected.c
    Core/User/Src/FEB_TPS2482.c
)

# Auto-discover Core/User/Inc if it exists
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Core/User/Inc")
    list(APPEND BASE_INCLUDES Core/User/Inc)
endif()

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET
# ══════════════════════════════════════════════════════════════════════

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${BASE_INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE m)

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

# Add linker options
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    # Enable floating-point printf/scanf support for newlib-nano
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)
