# FEB Time Library CMake Configuration
# ---------------------------------------------------------------------------
# Microsecond-precision monotonic clock backed by the Cortex-M4 DWT cycle
# counter. No peripheral usage, zero runtime overhead outside the Us() call.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_time)
#
# In your code:
#   #include "feb_time.h"
#
#   FEB_Time_Init();
#   uint64_t t_us = FEB_Time_Us();
# ---------------------------------------------------------------------------

add_library(feb_time INTERFACE)

target_sources(feb_time INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_time.c
)

target_include_directories(feb_time INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
