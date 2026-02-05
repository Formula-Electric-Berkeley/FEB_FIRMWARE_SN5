# FEB CAN Library - CMake Configuration
#
# This creates an INTERFACE library that boards can link to.
# Usage in board CMakeLists.txt:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_can)
#
# The library automatically adds:
#   - Source files (Src/*.c)
#   - Include directories (Inc/)
#

# Define the library as INTERFACE since we're adding sources to the linking target
add_library(feb_can INTERFACE)

# Add source files
target_sources(feb_can INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_can.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_can_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_can_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_can_filter.c
)

# Add include directories
target_include_directories(feb_can INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
