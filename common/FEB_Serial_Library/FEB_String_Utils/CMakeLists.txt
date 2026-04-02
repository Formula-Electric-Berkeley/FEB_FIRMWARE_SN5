# FEB String Utils Library CMake Configuration
# ---------------------------------------------------------------------------
# Common string utilities shared across FEB libraries.
#
# Usage in your CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_string_utils)
#
# In your code:
#   #include "feb_string_utils.h"
#
#   if (FEB_strcasecmp(str1, str2) == 0) { ... }
# ---------------------------------------------------------------------------

add_library(feb_string_utils INTERFACE)

target_sources(feb_string_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_string_utils.c
)

target_include_directories(feb_string_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
