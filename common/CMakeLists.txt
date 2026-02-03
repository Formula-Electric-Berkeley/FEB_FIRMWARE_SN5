# FEB Common Libraries
#
# This file aggregates all shared libraries in common/.
# Boards can link to these libraries using target_link_libraries().
#

# UART Library - printf/scanf redirection with DMA support
add_subdirectory(FEB_UART_Library)

# Console Library - command-line interface with case-insensitive commands
add_subdirectory(FEB_Console_Library)
