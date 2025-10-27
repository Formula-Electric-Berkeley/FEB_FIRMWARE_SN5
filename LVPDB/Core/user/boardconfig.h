// Our Central Config for our board-wise attributes like clock speeds and CAN bitrates

#pragma once
#define BOARD_SN4 4
#define BOARD_SN5 5

#ifndef BOARD
#  define BOARD BOARD_SN5   
#endif

#if (BOARD == BOARD_SN5)
  #include "boardsn5.h"
#elif (BOARD == BOARD_SN4)
  #include "board_sn4.h"
#else
# error "Unknown BOARD"
#endif
