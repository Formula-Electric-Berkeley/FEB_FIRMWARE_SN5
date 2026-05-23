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

# ── Sensor Node Variants ─────────────────────────────────────────────
# One source tree, two distinct CMake executables — Sensor_Nodes_FRONT
# and Sensor_Nodes_REAR. Both variants run on identical hardware; they
# differ in which CAN frame IDs they publish and (optionally) which
# sensors are populated. Per-sensor presence flags (FEB_SN_HAS_*) live in
# Core/User/Inc/FEB_SN_Config.h.
#
# Defining the variants as separate targets — instead of a single target
# whose binary name depends on a cache var — means:
#   * VS Code's CMake Tools build-target dropdown lists both natively.
#   * `cmake --build <dir> --target Sensor_Nodes_FRONT` works.
#   * Both ELFs build into the SAME build directory in a single configure;
#     no per-variant build directory needed.

# ── Compile definitions (target-agnostic) ────────────────────────────
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

# CAN library generated source (from common submodule)
list(APPEND ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4/gen/feb_can.c")

# LwGPS library (lightweight GPS NMEA parser)
set(LWGPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/lwgps/src")
list(APPEND ALL_SOURCES "${LWGPS_DIR}/lwgps/lwgps.c")

# Fusion AHRS library (xioTechnologies)
set(FUSION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/Fusion")
list(APPEND ALL_SOURCES
    "${FUSION_DIR}/FusionAhrs.c"
    "${FUSION_DIR}/FusionBias.c"
    "${FUSION_DIR}/FusionCompass.c"
)

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

# Common CAN library (submodule)
set(CAN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4")
list(APPEND BASE_INCLUDES
    "${CAN_LIB_DIR}/.."          # resolves #include "FEB_CAN_Library_SN4/gen/feb_can.h"
    "${CAN_LIB_DIR}/gen"         # resolves #include "feb_can.h" directly
)

# LwGPS library includes
list(APPEND BASE_INCLUDES "${LWGPS_DIR}/include")

# Fusion library includes
list(APPEND BASE_INCLUDES "${FUSION_DIR}")

# Linker script (shared between variants)
file(GLOB LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F4*_FLASH.ld")
if(LINKER_SCRIPT)
    list(GET LINKER_SCRIPT 0 LINKER_SCRIPT)
endif()

# Remove wrong libob.a dependency (once, applies to both variant targets)
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Provenance helper (loads feb_apply_version once)
include(${CMAKE_SOURCE_DIR}/cmake/FEB_Version.cmake)

# ══════════════════════════════════════════════════════════════════════
#  PER-VARIANT EXECUTABLE TARGETS
#
#  feb_define_sensor_node_variant(<variant>) registers an executable named
#  Sensor_Nodes_<variant> with all the per-target settings (compile def,
#  linker, version metadata, post-build .bin/.hex/size). To add a third
#  variant later, define a new FEB_SN_VARIANT_<name> sentinel in
#  FEB_SN_Config.h and call this function with the new name.
# ══════════════════════════════════════════════════════════════════════

function(feb_define_sensor_node_variant VARIANT)
    set(TARGET_NAME "${PROJECT_NAME}_${VARIANT}")

    add_executable(${TARGET_NAME} ${ALL_SOURCES})
    target_include_directories(${TARGET_NAME} PRIVATE ${BASE_INCLUDES})
    target_link_libraries(${TARGET_NAME} PRIVATE m feb_io feb_version feb_can feb_tps)

    # The single switch that selects FRONT vs REAR. Both sentinel values
    # are defined in Core/User/Inc/FEB_SN_Config.h.
    target_compile_definitions(${TARGET_NAME} PRIVATE
        FEB_SENSOR_NODE_VARIANT=FEB_SN_VARIANT_${VARIANT}
    )

    # Embed version + build provenance (per-target so commit/dirty/build
    # timestamp are stamped into both binaries independently).
    feb_apply_version(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR})

    # Enable floating-point printf/scanf for newlib-nano
    target_link_options(${TARGET_NAME} PRIVATE
        "LINKER:-u,_printf_float"
        "LINKER:-u,_scanf_float"
    )

    if(LINKER_SCRIPT)
        target_link_options(${TARGET_NAME} PRIVATE -T "${LINKER_SCRIPT}")
    endif()
    target_link_options(${TARGET_NAME} PRIVATE
        -Wl,-Map=$<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.map
    )

    # Post-build: .bin, .hex, size
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}>
                ${TARGET_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET_NAME}>
                ${TARGET_NAME}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
        COMMENT "Generating .bin/.hex for ${TARGET_NAME}"
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        ADDITIONAL_CLEAN_FILES "${TARGET_NAME}.map;${TARGET_NAME}.bin;${TARGET_NAME}.hex"
    )
endfunction()

feb_define_sensor_node_variant(FRONT)
feb_define_sensor_node_variant(REAR)

# Convenience aggregate: `cmake --build . --target Sensor_Nodes` builds both.
add_custom_target(${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}_FRONT ${PROJECT_NAME}_REAR
)
