cmake_minimum_required(VERSION 3.22)

# ── Project identity (derived from directory name) ───────────────────
get_filename_component(PROJ_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(CMAKE_PROJECT_NAME ${PROJ_NAME})

# ── Compiler settings ────────────────────────────────────────────────
set(CMAKE_C_STANDARD                11)
set(CMAKE_C_STANDARD_REQUIRED       ON)
set(CMAKE_C_EXTENSIONS              ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS   TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(${CMAKE_PROJECT_NAME} C CXX ASM)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Compile definitions (DASH = STM32F469xx) ─────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F469xx
    $<$<CONFIG:Debug>:DEBUG>
)

# ══════════════════════════════════════════════════════════════════════
#  SOURCE COLLECTION
# ══════════════════════════════════════════════════════════════════════

# 1) Core application sources (CubeMX-generated + user code)
file(GLOB_RECURSE CORE_SOURCES "Core/*.c")

# 2) Startup assembly
file(GLOB STARTUP_ASM "startup_*.s")

# 3) HAL/LL driver sources (safe: HAL_MODULE_ENABLED guards; exclude templates)
file(GLOB HAL_SOURCES "Drivers/STM32F4xx_HAL_Driver/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

# 4) FreeRTOS kernel + port (explicit to avoid wrong heap/port selection)
set(FREERTOS_SOURCES
    Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
)

# 5) FatFs (explicit to avoid extra codepage files)
set(FATFS_SOURCES
    Middlewares/Third_Party/FatFs/src/diskio.c
    Middlewares/Third_Party/FatFs/src/ff.c
    Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
    Middlewares/Third_Party/FatFs/src/option/syscall.c
)

# 6) FATFS application layer (CubeMX-generated)
file(GLOB FATFS_APP_SOURCES "FATFS/Target/*.c" "FATFS/App/*.c")

# 7) LVGL core (src/ only — NOT demos/examples/tests)
file(GLOB_RECURSE LVGL_SOURCES "Drivers/lvgl/src/*.c")

# 8) LVGL HAL bridge
file(GLOB_RECURSE LVGL_HAL_SOURCES "Drivers/hal_stm_lvgl/*.c")

# 9) SquareLine UI
file(GLOB_RECURSE SQUARELINE_SOURCES "Drivers/SquareLine_UI/*.c")

# 10) BSP (explicit — only the drivers actually used)
set(BSP_SOURCES
    Drivers/BSP/STM32469I-Discovery/stm32469i_discovery.c
    Drivers/BSP/STM32469I-Discovery/stm32469i_discovery_lcd.c
    Drivers/BSP/STM32469I-Discovery/stm32469i_discovery_sdram.c
    Drivers/BSP/Components/otm8009a/otm8009a.c
    Drivers/BSP/Components/nt35510/nt35510.c
    Drivers/BSP/Components/ft6x06/ft6x06.c
)

# 11) Utility fonts
file(GLOB UTILITY_SOURCES "Drivers/Utilities/Fonts/*.c")

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET
# ══════════════════════════════════════════════════════════════════════

add_executable(${CMAKE_PROJECT_NAME}
    ${CORE_SOURCES}
    ${STARTUP_ASM}
    ${HAL_SOURCES}
    ${FREERTOS_SOURCES}
    ${FATFS_SOURCES}
    ${FATFS_APP_SOURCES}
    ${LVGL_SOURCES}
    ${LVGL_HAL_SOURCES}
    ${SQUARELINE_SOURCES}
    ${BSP_SOURCES}
    ${UTILITY_SOURCES}
)

# ══════════════════════════════════════════════════════════════════════
#  INCLUDE DIRECTORIES
# ══════════════════════════════════════════════════════════════════════

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Core
    Core/Inc
    Core/User/Inc

    # HAL / CMSIS
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include

    # FreeRTOS
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F

    # FatFs
    Middlewares/Third_Party/FatFs/src

    # FATFS application layer
    FATFS/Target
    FATFS/App

    # BSP
    Drivers/BSP/STM32469I-Discovery
    Drivers/BSP/Components
    Drivers/BSP/Components/otm8009a
    Drivers/BSP/Components/nt35510
    Drivers/BSP/Components/Common
    Drivers/BSP/Components/ft6x06

    # LVGL + HAL + SquareLine
    Drivers/lvgl
    Drivers/hal_stm_lvgl
    Drivers/SquareLine_UI

    # Fonts & Utilities
    Drivers/Utilities/Fonts

    # LVGL config header (lv_conf.h lives in Drivers/)
    Drivers
)

# ── Link libraries ───────────────────────────────────────────────────
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)

# Enable floating-point printf/scanf for newlib-nano
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)

# Remove wrong libob.a dependency
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ── Post-build: .bin, .hex, size ─────────────────────────────────────
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating .bin/.hex for ${CMAKE_PROJECT_NAME}"
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_PROJECT_NAME}.map;${CMAKE_PROJECT_NAME}.bin;${CMAKE_PROJECT_NAME}.hex"
)
