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
#define MAX_SHIPS 5

// States for the SM
enum Battleship_st_t {
    init_st,
    new_game_st,
    ship_place_st,
    attack_init_st,
    attack_wait_st,
    attack_shoot_st,
    wait_restart_st
};
static enum Battleship_st_t currentState;

// Structure for a ship
typedef struct {
    int length;       // Length of the ship
    int r;            // Row position
    int c;            // Column position
    bool horizontal;  // Orientation of the ship
    bool sunk;        // If the ship is sunk
} ship_t;

static ship_t ships[MAX_SHIPS];
static int ship_count;
static int hits;
//if total hits for a single player == total sizes of all ships then game over

// Initialize SM
void game_init() {
    currentState = init_st;
}

void game_tick() {
    static bool isWin = false, isVal = false, isRestart = false;
    static bool boolTurn = true; // Player 1 = true, Player 2 = false
    static int8_t r, c;
    static uint8_t byte;

    // State transitions
    switch (currentState) {
        case init_st:
            currentState = new_game_st;
            break;
        case new_game_st:
            currentState = ship_place_st;
            break;
        case ship_place_st:
            //cycle through the different ship types
            if (ship_count == MAX_SHIPS) {
                currentState = attack_init_st;
            }
            break;  
        case attack_init_st:
            currentState = attack_wait_st;
            break;
        case attack_wait_st:           
         
            break;
        case attack_shoot_st:
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
        case attack_init_st:
            lcd_fillScreen(CONFIG_BACK_CLR);
            graphics_drawGrid(CONFIG_GRID_CLR);
            // com_write(); Send arrays across controllers
            break;
        case attack_wait_st:
            
            break;
        case attack_shoot_st:
            // SET MARK
            if (boolTurn) { //change to set based on hit or miss
                //if (is_hit(row, column)) {

                //}
            }
            else {
                
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
                currentState = init_st;
                boolTurn = 1;
            }
            break;
        default:
            printf("ERROR ACTION");
            break;
    }
}

bool is_hit(row, column) {
    for (uint16_t i = 0; i < MAX_SHIPS; i++) {
        ship_t *ship = &ships[i];

        if (ship->horizontal) {
            for (int j = 0; j < ship->length; j++) {
                if (ship->r == row && (ship->c + j) == column) {
                    return true;
                }
            }
        }
        else {
            for (int j = 0; j < ship->length; j++) {
                if (ship->c == column && (ship->r + j) == row) {
                    return true;
                }
            }
        }
    }

    return false;
}