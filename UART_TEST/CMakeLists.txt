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
    STM32U575xx
    USE_FreeRTOS_HEAP_5
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
file(GLOB HAL_SOURCES "Drivers/STM32U5xx_HAL_Driver/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

# FreeRTOS kernel sources
file(GLOB FREERTOS_SOURCES
    "Middlewares/Third_Party/FreeRTOS/Source/*.c"
    "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c"
    "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/*.c"
    "Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_5.c"
)

# Combine all sources
set(ALL_SOURCES ${CORE_SOURCES} ${STARTUP_ASM} ${HAL_SOURCES} ${FREERTOS_SOURCES})

# ══════════════════════════════════════════════════════════════════════
#  INCLUDE DIRECTORIES
# ══════════════════════════════════════════════════════════════════════

set(BASE_INCLUDES
    Core/Inc
    Drivers/STM32U5xx_HAL_Driver/Inc
    Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32U5xx/Include
    Drivers/CMSIS/Include
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
    Middlewares/Third_Party/CMSIS/RTOS2/Include
)

# Auto-discover Core/User/Inc if it exists
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Core/User/Inc")
    list(APPEND BASE_INCLUDES Core/User/Inc)
endif()

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET
# ══════════════════════════════════════════════════════════════════════

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# STM32U575xx is Cortex-M33 - override the default Cortex-M4 flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m33
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -Wall -fdata-sections -ffunction-sections
)

target_include_directories(${PROJECT_NAME} PRIVATE ${BASE_INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE m feb_uart feb_console)

# Enable floating-point printf/scanf for newlib-nano
target_link_options(${PROJECT_NAME} PRIVATE
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)

# Linker script and map file
target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m33
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -T "${CMAKE_CURRENT_SOURCE_DIR}/STM32U575xx_FLASH.ld"
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
