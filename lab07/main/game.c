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
#define MAX_SHIPS 5
#define delayMS(ms) \
	vTaskDelay(((ms)+(portTICK_PERIOD_MS-1))/portTICK_PERIOD_MS)

#define RGB565(r, g, b)  (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))


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


//Enum for ship colors
const uint16_t ship_colors[MAX_SHIPS] = {
    RGB565(0, 255, 0),   // Bright green
    RGB565(255, 165, 0), // Bright orange
    RGB565(160, 32, 240), // Bright purple
    RGB565(0, 150, 0),    // Medium green
    RGB565(255, 100, 0),  // Darker orange
    RGB565(75, 0, 130),   // Indigo
    RGB565(34, 139, 34),  // Forest green
    RGB565(255, 69, 0),   // Red-orange
    RGB565(218, 112, 214), // Orchid
    RGB565(173, 255, 47), // Green-yellow
    RGB565(238, 130, 238) // Violet
};


static ship_t ships[MAX_SHIPS];
static int ship_count;
static int hits;
static int myShips[CONFIG_BOARD_R][CONFIG_BOARD_C];
static int enemyShips[CONFIG_BOARD_R][CONFIG_BOARD_C];
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
            graphics_drawMessage("PLACE YOUR SHIPS (PLACE-A ROTATE-B)", WHITE, CONFIG_BACK_CLR);
            lcd_writeFrame();

            //create ships size 1 - MAX_SHIPS
            for(int i = 0; i < MAX_SHIPS; i++){
                ships[i].length = i+1;
                ships[i].horizontal = true;
                ships[i].color = ship_colors[i % (sizeof(ship_colors) / sizeof(ship_colors[0]))];
            }

            //FLUSH COMS
            while (com_read(&byte, 1)) {
                // Just read and discard the bytes
            }
            break;
        case ship_place_st:
                lcd_frameEnable();
                nav_get_loc(&r, &c);
                
                if ((r != prev_r) || (c != prev_c) || (prev_horizontal != currHor)){ //if the cursor moved or rotated
                graphics_drawHighlight(r,c,WHITE); //draw cursor
                graphics_drawMessage("PLACE YOUR SHIPS (PLACE-A ROTATE-B)", WHITE, CONFIG_BACK_CLR); //redraw text

                    //erase previous higlights
                    if (prev_horizontal){ // was Horizontal
                        for (int i = 0; i < ships[numShip].length + 1; i++){
                            graphics_drawHighlight(prev_r, prev_c + i, CONFIG_BACK_CLR);
                        }
                    } else{
                        for (int i = 0; i < ships[numShip].length + 1; i++){
                            graphics_drawHighlight(prev_r + i, prev_c, CONFIG_BACK_CLR);
                        }
                    }

                    //draw new higlights
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
               
                prev_c = c; //update the prev trackers for erasing on the next loop
                prev_r = r;
                prev_horizontal = currHor;

                
                if (!pin_get_level(HW_BTN_B)) { //rotate
                    currHor = !currHor;
                    for(;;){
                            if (pin_get_level(HW_BTN_B)) break; //prevent holding the button
                        };
                }

                if (!pin_get_level(HW_BTN_A)) { //Place ship button pressed
                    if (is_validShip(ships[numShip], r, c, currHor)){ //ship place is valid
                        graphics_drawHighlight(r,c,CYAN);
                        myShips[r][c] = 1;
                        if (currHor){ //Horizontal
                            for (int i = 0; i < ships[numShip].length; i++){
                                graphics_drawShip(r,c+i,ships[numShip].color);
                                myShips[r][c+i] = 1;
                            }
                        } else{
                            for (int i = 0; i < ships[numShip].length; i++){
                                graphics_drawShip(r+i,c,ships[numShip].color);
                                myShips[r+i][c] = 1;
                            }
                        }
                        numShip--; //move to next ship     
                        for(;;){
                            if (pin_get_level(HW_BTN_A)) break; //prevent holding the button
                        };
                    }else{ //can't place
                        //TODO ADD VISUAL ERROR NOTICE
                    }                     
                }
                
                
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

bool is_validShip(ship_t shipToCheck, int8_t r, int8_t c, bool currHor){
    //check cursor
    if (myShips[r][c] !=0){
        return false;
    }

    //check for other ships or edges
    if (currHor){ //Horizontal
        for (int i = 0; i < shipToCheck.length; i++){
            if ((myShips[r][c+i] != 0) || (c+i > CONFIG_BOARD_C)){
                return false;
            };
        }
    } else{ //Vertical
        for (int i = 0; i < shipToCheck.length; i++){
            if ((myShips[r+i][c] != 0) || (r+i > CONFIG_BOARD_R)){
                return false;
            };
        }
    }
    return true; //no issues detected
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