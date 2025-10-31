cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME DASH)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Collect LVGL Core Sources
file(GLOB_RECURSE LVGL_SOURCES
    Drivers/lvgl/src/*.c
)

# Collect LVGL HAL Sources
file(GLOB_RECURSE LVGL_HAL_SOURCES
    Drivers/hal_stm_lvgl/*.c
)

# Collect SquareLine UI Sources
file(GLOB_RECURSE SQUARELINE_SOURCES
    Drivers/SquareLine_UI/*.c
)


# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/User/Src/FEB_UI.c

    # BSP
    Drivers/BSP/STM32469I-Discovery/stm32469i_discovery.c
    Drivers/BSP/STM32469I-Discovery/stm32469i_discovery_lcd.c
    Drivers/BSP/STM32469I-Discovery/stm32469i_discovery_sdram.c
    Drivers/BSP/Components/otm8009a/otm8009a.c
    Drivers/BSP/Components/nt35510/nt35510.c
    Drivers/BSP/Components/ft6x06/ft6x06.c

    # LVGL + HAL + UI
    ${LVGL_SOURCES}
    ${LVGL_HAL_SOURCES}
    ${SQUARELINE_SOURCES}

)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/User/Inc

    # BSP Drivers
    Drivers/BSP/STM32469I-Discovery
    Drivers/BSP/Components
    Drivers/BSP/Components/otm8009a
    Drivers/BSP/Components/nt35510
    Drivers/BSP/Components/Common
    Drivers/BSP/Components/ft6x06

    # LVGL + HAL
    Drivers/lvgl
    Drivers/hal_stm_lvgl

    # SquareLine UI
    Drivers/SquareLine_UI

    # Fonts & Utilities
    Drivers/Utilities/Fonts

    # LVGL config header location (directory, not file)
    Drivers
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Add linker options
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    # Enable floating-point printf/scanf support for newlib-nano
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)
