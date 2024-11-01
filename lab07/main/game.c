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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdio.h>

#define BIT_SHIFT 4
#define FULL_LOW_NIB 0x0F
#define MAX_SHIPS 4
#define delayMS(ms) \
	vTaskDelay(((ms)+(portTICK_PERIOD_MS-1))/portTICK_PERIOD_MS)



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
    color_t color;    //Color of placement
} ship_t;

/*TODOS FOR JOSEPH
    create enum for colors and set unique ship color in new_game_st
    fix bug where when placing ship the last cursor is not removed
    create array to actually save the ship placement

    create a helper function to see if ship placement is valid
    use array to check if ship placement is valid (no other ships)
    check if ship is out of bounds (check grid size)
*/

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
    static int8_t prev_r = -1, prev_c = -1;
    static bool prev_horizontal = true;
    static uint8_t byte;
    static bool currHor = false; //current orientation for placement
    static int8_t numShip = MAX_SHIPS - 1;

    // State transitions
    switch (currentState) {
        case init_st:
            currentState = new_game_st;
            break;
        case new_game_st:
            currentState = ship_place_st;
            break;
        case ship_place_st:
            if (numShip < 0){
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
            lcd_frameEnable();
            // CLEAR BOARD
            board_clear();
            // DRAW GAME BACKGROUND
            lcd_fillScreen(CONFIG_BACK_CLR);
            // DRAW GRID isVal = false;
            graphics_drawGrid(CONFIG_GRID_CLR);
            // X TURN
            boolTurn = true;
            // SET NAV TO CENTER
            nav_set_loc(0,0);
            // DISPLAY NEXT PLAYER
            graphics_drawMessage("Next Player: X", WHITE, CONFIG_BACK_CLR);
            lcd_writeFrame();

            //create ships size 1 - MAX_SHIPS
            for(int i = 0; i < MAX_SHIPS; i++){
                ships[i].length = i+1;
                ships[i].horizontal = true;
                ships[i].color = CYAN;
            }

            //MILESTONE 2
            //FLUSH COMS
            while (com_read(&byte, 1)) {
                // Just read and discard the bytes
            }
            break;
        case ship_place_st:
                bool shipPlaced = false;
                lcd_frameEnable();
                nav_get_loc(&r, &c);
                
                if ((r != prev_r) || (c != prev_c) || (prev_horizontal != currHor)){ //if the cursor moved or rotated
                graphics_drawHighlight(r,c,WHITE);

                    //erase previous cursor
                    if (prev_horizontal){ // was Horizontal
                        for (int i = 0; i < ships[numShip].length; i++){
                            graphics_drawHighlight(prev_r, prev_c + i, CONFIG_BACK_CLR);
                        }
                    } else{
                        for (int i = 0; i < ships[numShip].length; i++){
                            graphics_drawHighlight(prev_r + i, prev_c, CONFIG_BACK_CLR);
                        }
                    }

                    //draw new cursor
                    if (currHor){ //Horizontal
                        for (int i = 0; i < ships[numShip].length; i++){
                            graphics_drawHighlight(r,c+i,WHITE);
                        }
                    } else{
                        for (int i = 0; i < ships[numShip].length; i++){
                            graphics_drawHighlight(r+i,c,WHITE);
                        }
                    }
                    lcd_writeFrame();
                  	


                } else { //the cursor didn't move

                }
               
                prev_c = c;
                prev_r = r;
                prev_horizontal = currHor;

                
                if (!pin_get_level(HW_BTN_B)) { //rotate
                    currHor = !currHor;
                }

                if (!pin_get_level(HW_BTN_A)) { //place
                    graphics_drawHighlight(r,c,CYAN);
                    if (currHor){ //Horizontal
                        for (int i = 0; i < ships[numShip].length; i++){
                            graphics_drawShip(r,c+i,ships[numShip].color);
                        }
                    } else{
                        for (int i = 0; i < ships[numShip].length; i++){
                            graphics_drawShip(r+i,c,ships[numShip].color);
                        }
                    }
                    if (!shipPlaced){
                        numShip--; //move to next ship     
                        shipPlaced = true;
                        for(;;){
                            if (pin_get_level(HW_BTN_A)) break;
                        };
                    }                     
                }
                
                printf("%d\n", numShip); 
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

bool is_hit(int8_t row, int8_t column) {
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