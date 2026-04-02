# FEB RTOS Utils Library CMake Configuration
# ---------------------------------------------------------------------------
# Common RTOS utilities shared across FEB boards.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_rtos_utils)
#
# In your code (within USER CODE sections):
#   #include "feb_rtos_utils.h"
#
#   myMutexHandle = osMutexNew(&myMutex_attributes);
#   REQUIRE_RTOS_HANDLE(myMutexHandle);
# ---------------------------------------------------------------------------

add_library(feb_rtos_utils INTERFACE)

target_include_directories(feb_rtos_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
