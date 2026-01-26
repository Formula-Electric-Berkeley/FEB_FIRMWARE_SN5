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

# Combine base sources
set(ALL_SOURCES ${CORE_SOURCES} ${STARTUP_ASM} ${HAL_SOURCES})

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

# ══════════════════════════════════════════════════════════════════════
#  EXECUTABLE TARGET
# ══════════════════════════════════════════════════════════════════════

add_executable(${CMAKE_PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${BASE_INCLUDES})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)

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
