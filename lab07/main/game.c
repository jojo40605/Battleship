#include "config.h"
#include "board.h"
#include "graphics.h"
#include "nav.h"
#include "com.h"
#include "game.h"
#include "hw.h"
#include "lcd.h"
#include "joy.h"
#include "pin.h"
#include <stdio.h>

#define BIT_SHIFT 4
#define FULL_LOW_NIB 0x0F

// States for the SM
enum ticTacToe_st_t {
    init_st,
    new_game_st,
    wait_mark_st,
    mark_st,
    wait_restart_st
};
static enum ticTacToe_st_t currentState;

// Initialize SM
void game_init() {
    currentState = init_st;
}

void game_tick() {
    static bool isWin = false, isVal = false, isRestart = false, isDraw = false;
    static bool boolTurn = true; // X = true, O = false
    static int8_t r, c;
    static uint8_t byte;

    // State transitions
    switch (currentState) {
        case init_st:
            currentState = new_game_st;
            break;
        case new_game_st:
            currentState = wait_mark_st;
            break;
        case wait_mark_st:           
            //MILESTONE 2
            // Check if data is received from the connected controller
            if (com_read(&byte, 1) > 0) {
                // Unpack the byte into row and column
                r = (byte >> BIT_SHIFT);
                c = byte & FULL_LOW_NIB;

                // Process the location as if Button A was pressed
                if (board_get(r, c) == no_m) {
                    isVal = true;
                    if (boolTurn) { //X
                    board_set(r, c, X_m);
                    graphics_drawX(r, c, CONFIG_MARK_CLR);
                } else { //O
                    board_set(r, c, O_m);
                    graphics_drawO(r, c, CONFIG_MARK_CLR);
                    }
                }                
            }
            else if (!pin_get_level(HW_BTN_A)) {
                // Encode row and column into a single byte
                nav_get_loc(&r, &c);
                byte = (r << BIT_SHIFT) | (c & FULL_LOW_NIB);
                com_write(&byte, 1);
            }
            
            //MILESTONE 1
            if (isVal) {
                currentState = mark_st;
                isVal = false;
            }            
            break;
        case mark_st:
            if (isWin || isDraw) {
                currentState = wait_restart_st;
                isWin = false;
                isDraw = false;
            } else {
                currentState = wait_mark_st;
            }
            break;
        case wait_restart_st:
            if (isRestart) {
                currentState = new_game_st;
                isRestart = false;
            }
            break;
        default:
            printf("ERROR TRANSITION");
            break;
    }

    // State Actions
    switch (currentState) {
        case init_st:
            // NOTHING
            break;
        case new_game_st:
            // CLEAR BOARD
            board_clear();
            // DRAW GAME BACKGROUND
            lcd_fillScreen(CONFIG_BACK_CLR);
            // DRAW GRID isVal = false;
            graphics_drawGrid(CONFIG_GRID_CLR);
            // X TURN
            boolTurn = true;
            // SET NAV TO CENTER
            nav_set_loc(1, 1);
            // DISPLAY NEXT PLAYER
            graphics_drawMessage("Next Player: X", WHITE, CONFIG_BACK_CLR);
            
            //MILESTONE 2
            //FLUSH COMS
            while (com_read(&byte, 1)) {
                // Just read and discard the bytes
            }
            break;
        case wait_mark_st:
            // WAIT FOR A
            if (!pin_get_level(HW_BTN_A)) {            
                nav_get_loc(&r, &c);
                if (board_get(r, c) == no_m) {
                    // Mark is valid
                    isVal = true;
                }
            }
            break;
        case mark_st:
            // SET MARK
            if (boolTurn) {
                board_set(r, c, X_m);
                graphics_drawX(r, c, CONFIG_MARK_CLR);
            } else {
                board_set(r, c, O_m);
                graphics_drawO(r, c, CONFIG_MARK_CLR);
            }

            // CHECK FOR WIN/DRAW
            if (board_winner(boolTurn ? X_m : O_m)) {
                isWin = true;
                graphics_drawMessage(boolTurn ? "Player X Wins" : "Player O Wins", WHITE, CONFIG_BACK_CLR);
                break;
            } else if (board_mark_count() >= CONFIG_BOARD_SPACES) {
                isDraw = true;
                graphics_drawMessage("It's a Draw!", WHITE, CONFIG_BACK_CLR);
                break;
            }

            // DISPLAY STATUS
            boolTurn = !boolTurn; // Switch turn
            graphics_drawMessage(boolTurn ? "Next Player: X" : "Next Player: O", WHITE, CONFIG_BACK_CLR);
            break;
        case wait_restart_st:
            // WAIT FOR START BUTTON
            if (!pin_get_level(HW_BTN_START)) {
                isRestart = false;
                isWin = false;
                isDraw = false;
                currentState = init_st;
                boolTurn = 1;
            }
            break;
        default:
            printf("ERROR ACTION");
            break;
    }
}
