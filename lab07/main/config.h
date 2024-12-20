#ifndef CONFIG_H_
#define CONFIG_H_

#define CONFIG_GAME_TIMER_PERIOD 40.0E-3f

// Board
#define CONFIG_BOARD_R 8 // Rows
#define CONFIG_BOARD_C 8 // Columns
#define CONFIG_BOARD_N 0 // Number of contiguous marks

#define CONFIG_BOARD_SPACES (CONFIG_BOARD_R*CONFIG_BOARD_C)

// Colors
#define CONFIG_BACK_CLR rgb565(0, 16, 42)
#define CONFIG_GRID_CLR CYAN
#define CONFIG_MARK_CLR YELLOW
#define CONFIG_HIGH_CLR GREEN
#define CONFIG_MESS_CLR CYAN

#endif // CONFIG_H_
