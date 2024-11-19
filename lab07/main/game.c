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
#include "sound.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdio.h>

#define MAX_SHIPS      2
#define SHIP_CLR_SIZE  11
#define SHIP_LIVES     (MAX_SHIPS * (MAX_SHIPS + 1)) / 2
#define STAT_STR_LEN   40
#define WIN_ID         8
#define MISS_ID        0x21
#define HIT_ID         0x31
#define delayMS(ms) \
	vTaskDelay(((ms)+(portTICK_PERIOD_MS-1))/portTICK_PERIOD_MS)
#define CONFIG_GAME_TIMER_PERIOD 40.0E-3f
#define PER_MS ((uint32_t)(CONFIG_GAME_TIMER_PERIOD*1000))

// States for the SM
enum Battleship_st_t {
    init_st,
    new_game_st,
    ship_place_st,
    waiting_for_opponent_st,
    attack_init_st,
    attack_wait_st,
    attack_shoot_st,
    wait_restart_st
};
static enum Battleship_st_t currentState;


//Enum for ship colors
const uint16_t ship_colors[SHIP_CLR_SIZE] = {
    rgb565(0, 255, 0),   // Bright green
    rgb565(255, 165, 0), // Bright orange
    rgb565(160, 32, 240), // Bright purple
    rgb565(0, 150, 0),    // Medium green
    rgb565(255, 100, 0),  // Darker orange
    rgb565(75, 0, 130),   // Indigo
    rgb565(34, 139, 34),  // Forest green
    rgb565(255, 69, 0),   // Red-orange
    rgb565(218, 112, 214), // Orchid
    rgb565(173, 255, 47), // Green-yellow
    rgb565(238, 130, 238) // Violet
};


static ship_t ships[MAX_SHIPS];
static uint8_t enemy_lives;
static int myShips[CONFIG_BOARD_R][CONFIG_BOARD_C];
static int enemyShips[CONFIG_BOARD_R][CONFIG_BOARD_C];
char shipPlaceStat[STAT_STR_LEN];
//if total hits for a single player == total sizes of all ships then game over

// Initialize SM
void game_init() {
    currentState = init_st;
}

void game_tick() {
    static bool isRestart = false;
    static bool isEmpty = false;
    static bool win = false;      // No win
    static uint8_t winTurn = 0;
    static uint8_t hitByte = 0x01;
    static bool enemyShipsReceived = false;
    //static bool boolTurn; // Player 1 = true, Player 2 = false
    static int8_t r, c;
    static int8_t prev_r = -1, prev_c = -1;
    static bool prev_horizontal = true;
    static uint8_t flushbyte;
    static bool currHor = false; //current orientation for placement
    static int8_t numShip = MAX_SHIPS - 1;

    // Bytes used to send/receive array
    uint8_t bmrow[CONFIG_BOARD_R] = {0};
    uint8_t berow[CONFIG_BOARD_R] = {0};

    // State transitions
    printf("%d: %x\n", currentState, hitByte);
    switch (currentState) {
        case init_st:
            currentState = new_game_st;
            break;
        case new_game_st:
            currentState = ship_place_st;
            break;
        case ship_place_st:
            if (numShip < 0){
                currentState = waiting_for_opponent_st;
            }
            break;        
        case waiting_for_opponent_st:
            if (enemyShipsReceived) {
                currentState = attack_init_st;
            }
            break;
        case attack_init_st:
            currentState = attack_wait_st;
            break;
        case attack_wait_st: 
            if (win) {
                currentState = wait_restart_st;
            }   
            else if (isEmpty) {
                isEmpty = false;
                currentState = attack_shoot_st;
            }      
            break;
        case attack_shoot_st:
            currentState = attack_wait_st;
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
            // DRAW GRID 
            graphics_drawGrid(CONFIG_GRID_CLR);
            // New game variables
            hitByte = 0x01;
            enemy_lives = SHIP_LIVES;
            isRestart = false;
            // SET NAV TO CENTER
            nav_set_loc(0,0);
            // DISPLAY NEXT PLAYER
            graphics_drawMessage("PLACE YOUR SHIPS (PLACE - A ROTATE - B)", WHITE, CONFIG_BACK_CLR);
            lcd_writeFrame();

            //create ships size 1 - MAX_SHIPS
            for(int i = 0; i < MAX_SHIPS; i++){
                ships[i].length = i+1;
                ships[i].horizontal = true;
                ships[i].color = ship_colors[i % (sizeof(ship_colors) / sizeof(ship_colors[0]))];
            }

            //FLUSH COMS
            while (com_read(&flushbyte, 1)) {
                // Just read and discard the bytes
                continue;
            }
            break;
        case ship_place_st:
                //lcd_frameEnable();
                nav_get_loc(&r, &c);
                
                if ((r != prev_r) || (c != prev_c) || (prev_horizontal != currHor)) { //if the cursor moved or rotated
                graphics_drawHighlight(r,c,WHITE); //draw cursor
                graphics_drawMessage("PLACE YOUR SHIPS (PLACE - A   ROTATE - B)", WHITE, CONFIG_BACK_CLR); //redraw text
                //TODO SOUND cursor move click?

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
                        //todo sound ship place
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
                    if (numShip > -1){
                        sprintf(shipPlaceStat, "Ship placed, remaining ships: %d", numShip+1);
                        }
                    else{
                        sprintf(shipPlaceStat, "All ships placed, waiting for opponent!");
                    }
                    graphics_drawMessage(shipPlaceStat, WHITE, CONFIG_BACK_CLR);
                    lcd_writeFrame();

                    } else { //can't place
                        graphics_drawHighlight(r,c,WHITE);
                        graphics_drawMessage("Invalid placement.", WHITE, CONFIG_BACK_CLR);
                        lcd_writeFrame();
                    }                     
                }

                break;
        case waiting_for_opponent_st:
            // Pack friendly ship array into 5 bytes
            for (int col = 0; col < CONFIG_BOARD_C; col++) {
                for (int row = 0; row < CONFIG_BOARD_R; row++) {
                    if (myShips[row][col] == 1) {
                        bmrow[row] = (bmrow[row] | 1 << (col));
                    }
                }
            }

            // Send friendly ship bytes
            for (int i = 0; i < CONFIG_BOARD_R; i++) {
                com_write(&bmrow[i], sizeof(bmrow[i]));
            }

            // Read enemy data bytes
            for (int i = 0; i < CONFIG_BOARD_R; i++) {
                com_read(&berow[i], sizeof(berow[i]));
            }

            // Unpack enemy bytes into array
            for (int col = 0; col < CONFIG_BOARD_C; col++) {
                for (int row = 0; row < CONFIG_BOARD_R; row++) {
                    if ((berow[row] & 1) == 1) {
                        enemyShips[row][col] = 1;
                    }

                    berow[row] = (berow[row] >> 1);
                }  
            }

            // Check to see if enemy array has been received
            for (int row = 0; row < CONFIG_BOARD_R; row++) {
                for (int col = 0; col < CONFIG_BOARD_C; col++) {
                    if (enemyShips[row][col] == 1) {
                        enemyShipsReceived = true;
                    }
                }
            }
            
            break;
        case attack_init_st:
            // Reset grid
            lcd_fillScreen(CONFIG_BACK_CLR);
            graphics_drawGrid(CONFIG_GRID_CLR);
            // Reset nav location to (0, 0)
            nav_set_loc(0, 0);
            graphics_drawHighlight(0,0,WHITE); //draw cursor at (0,0)
            graphics_drawMessage("Start!", WHITE, CONFIG_BACK_CLR); //Draw default player's turn
            lcd_writeFrame(); //push buffer

            // Flush coms
            while(com_read(&flushbyte, 1)) {
                continue;
            }

            delayMS(400);  // Delay so players can see Start message.

            break;
        case attack_wait_st:
            // Gather how many lives left
            nav_get_loc(&r, &c); //get cursor location
            graphics_drawHighlight(r,c,WHITE); //draw cursor
            lcd_writeFrame(); //push buffer
            
            
            if (!pin_get_level(HW_BTN_A) && (hitByte & 0x01)) {
                // Check if spot has not been shot
                if (board_get(r, c) == no_m) {
                    isEmpty = true;
                    hitByte = 0;
                }
                else {
                    //todo sound invalid shot
                    graphics_drawMessage("Invalid location.", WHITE, CONFIG_BACK_CLR);
                    lcd_writeFrame();
                }
            }
            
            com_read(&hitByte, sizeof(hitByte));

            if ((hitByte == HIT_ID)) {
                graphics_drawMessage("Your opponent hit your ship!", WHITE, CONFIG_BACK_CLR);
                lcd_writeFrame();
                delayMS(700);
                hitByte = 0x01;
            }
            else if ((hitByte == MISS_ID)) {
                graphics_drawMessage("Your opponent missed your ships!", WHITE, CONFIG_BACK_CLR);
                lcd_writeFrame();
                delayMS(700);
                hitByte = 0x01;
            }

            // If com_read returns true (occurs when opposing controller plays)
            if (hitByte == 0x01) {
                graphics_drawMessage("Your turn.", WHITE, CONFIG_BACK_CLR);
                lcd_writeFrame();
            }

            com_read(&winTurn, sizeof(winTurn));

            if (winTurn == WIN_ID) {
                //todo sound loss
                win = true;
                graphics_drawMessage("You lost! Press \"Start\" to play again.", WHITE, CONFIG_BACK_CLR);
                lcd_writeFrame();
            }
            
            break;
        case attack_shoot_st:
            // SET MARK
            
            if (!(hitByte & 0x0F)) { // My turn
                if (is_hit(r, c, enemyShips)) {
                    //todo sound ship hit
                    board_set(r, c, hit_m);
                    graphics_drawX(r, c, RED);
                    graphics_drawMessage("Hit! Waiting for opponent...", WHITE, CONFIG_BACK_CLR);
                    hitByte = HIT_ID;
                    enemy_lives--;
                }
                else {
                    //todo sound ship miss
                    board_set(r, c, miss_m);
                    graphics_drawO(r, c, GREEN);
                    graphics_drawMessage("Miss! Waiting for opponent...", WHITE, CONFIG_BACK_CLR);
                    hitByte = MISS_ID;
                }

                com_write(&hitByte, sizeof(hitByte));
                hitByte = hitByte & 0xF0;

                if (enemy_lives == 0) {
                    win = true;
                    winTurn = WIN_ID;
                }

                if (winTurn == WIN_ID) {
                    //todo sound win
                    com_write(&winTurn, sizeof(winTurn));
                    graphics_drawMessage("You won! Press \"Start\" to play again.", WHITE, CONFIG_BACK_CLR);
                    lcd_writeFrame();
                    winTurn = 0;
                }
                
                
            }
        
            lcd_writeFrame(); //push buffer
            
            break;
        case wait_restart_st:
            // WAIT FOR START BUTTON
            if (!pin_get_level(HW_BTN_START)) {
                // Reset stats
                isRestart = true;
                win = false;
                winTurn = 0;
                currentState = init_st;
                enemyShipsReceived = false;
                hitByte = 0x01;
                numShip = MAX_SHIPS - 1;
                currHor = false;

                // Clear ship arrays
                for (int r = 0; r < CONFIG_BOARD_R; r++) {
                    for (int c = 0; c < CONFIG_BOARD_C; c++) {
                        myShips[r][c] = 0;
                        enemyShips[r][c] = 0;
                    }
                }
            }

            break;
        default:
            printf("ERROR ACTION");
            break;
    }
}

// Check valid ship
bool is_validShip(ship_t shipToCheck, int8_t r, int8_t c, bool currHor){
    //check cursor
    if (myShips[r][c] != 0){
        return false;
    }

    //check for other ships or edges
    if (currHor){ //Horizontal
        for (int i = 0; i < shipToCheck.length; i++){
            if ((myShips[r][c+i] != 0) || (c + i >= CONFIG_BOARD_C)){
                return false;
            };
        }
    } else{ //Vertical
        for (int i = 0; i < shipToCheck.length; i++){
            if ((myShips[r+i][c] != 0) || (r + i >= CONFIG_BOARD_R)){
                return false;
            };
        }
    }
    return true; //no issues detected
}

// Detect if there is a hit
bool is_hit(int8_t row, int8_t column, int shipMap[CONFIG_BOARD_R][CONFIG_BOARD_C]) {
    if (shipMap[row][column] == 1) {
        return true;
    }

    return false;
}