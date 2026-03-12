cmake_minimum_required(VERSION 3.22)

# Project identity derived from directory name.
get_filename_component(PROJ_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Compiler settings
set(CMAKE_C_STANDARD                11)
set(CMAKE_C_STANDARD_REQUIRED       ON)
set(CMAKE_C_EXTENSIONS              ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS   TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(${PROJ_NAME} C CXX ASM)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Compile definitions
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F446xx
    $<$<CONFIG:Debug>:DEBUG>
)

# Source collection
file(GLOB_RECURSE CORE_SOURCES "Core/*.c")
file(GLOB STARTUP_ASM "startup_*.s")
file(GLOB HAL_SOURCES "Drivers/STM32F4xx_HAL_Driver/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

set(ALL_SOURCES ${CORE_SOURCES} ${STARTUP_ASM} ${HAL_SOURCES})

# Include directories
set(BASE_INCLUDES
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Core/User/Inc")
    list(APPEND BASE_INCLUDES Core/User/Inc)
endif()

# Executable target
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${BASE_INCLUDES})
target_compile_definitions(${PROJECT_NAME} PRIVATE FEB_UART_COMPILE_LOG_LEVEL=5)
target_link_libraries(${PROJECT_NAME} PRIVATE m feb_console feb_can)

target_link_options(${PROJECT_NAME} PRIVATE
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)

file(GLOB LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F4*_FLASH.ld")
if(LINKER_SCRIPT)
    list(GET LINKER_SCRIPT 0 LINKER_SCRIPT)
    target_link_options(${PROJECT_NAME} PRIVATE -T "${LINKER_SCRIPT}")
endif()
target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.map
)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

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
