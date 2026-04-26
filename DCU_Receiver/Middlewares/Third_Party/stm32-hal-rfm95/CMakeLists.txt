# stm32-hal-rfm95-receiver Library
# Simplified RFM95W LoRa driver (no LoRaWAN)

# Get the DCU directory (3 levels up from here)
get_filename_component(DCU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_library(stm32-hal-rfm95-receiver STATIC
    rfm95.c
)

target_include_directories(stm32-hal-rfm95-receiver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# The library needs HAL and other includes from the parent project
target_include_directories(stm32-hal-rfm95-receiver PRIVATE
    ${DCU_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${DCU_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${DCU_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${DCU_DIR}/Drivers/CMSIS/Include
    ${DCU_DIR}/Core/Inc
    ${DCU_DIR}/Core/User/Inc
    # FreeRTOS includes (for cmsis_os2.h)
    ${DCU_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${DCU_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${DCU_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    # FEB Serial Library
    ${DCU_DIR}/../common/FEB_Serial_Library/FEB_Log/Inc
    ${DCU_DIR}/../common/FEB_Serial_Library/FEB_UART/Inc
)
