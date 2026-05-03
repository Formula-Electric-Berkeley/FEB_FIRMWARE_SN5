# FEB Version Library
# ---------------------------------------------------------------------------
# Exposes the FEB_Build_Info_t and FEB_Flash_Info_t types and the flash-info
# placeholder struct that flash-patcher.py rewrites at programming time.
#
# Each board's build_info is generated per-target by cmake/FEB_Version.cmake
# and compiled directly into that target - the feb_version library itself
# only carries the shared types and the flash-info placeholder.
#
# Usage:
#   target_link_libraries(${PROJECT_NAME} PRIVATE feb_version)
#   include(${CMAKE_SOURCE_DIR}/cmake/FEB_Version.cmake)
#   feb_apply_version(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR})
# ---------------------------------------------------------------------------

add_library(feb_version INTERFACE)

target_sources(feb_version INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/feb_flash_info.c
)

target_include_directories(feb_version INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)
