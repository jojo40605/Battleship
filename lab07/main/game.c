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

#define BIT_SHIFT      4
#define FULL_LOW_NIB   0x0F
#define MAX_SHIPS      5
#define SHIP_CLR_SIZE  11
#define SHIP_LIVES     15
#define R1             0
#define R2             1
#define R3             2
#define R4             3
#define R5             4
#define MY_WIN         1
#define ENEMY_WIN      2
#define delayMS(ms) \
	vTaskDelay(((ms)+(portTICK_PERIOD_MS-1))/portTICK_PERIOD_MS)

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
static int ship_count;
static uint8_t player_lives = SHIP_LIVES;
static uint8_t enemy_lives = SHIP_LIVES;
static int myShips[CONFIG_BOARD_R][CONFIG_BOARD_C];
static int enemyShips[CONFIG_BOARD_R][CONFIG_BOARD_C];
//if total hits for a single player == total sizes of all ships then game over

// Initialize SM
void game_init() {
    currentState = init_st;
}

void game_tick() {
    static bool isRestart = false;
    static bool isEmpty = false;
    static uint8_t win = 0;      // No win
    static bool enemyShipsReceived = false;
    static bool boolTurn = true; // Player 1 = true, Player 2 = false
    static int8_t r, c;
    static int8_t prev_r = -1, prev_c = -1;
    static bool prev_horizontal = true;
    static uint8_t byte;
    static bool currHor = false; //current orientation for placement
    static int8_t numShip = MAX_SHIPS - 1;

    // Bytes used to send/receive array
    uint8_t bmrow1 = 0, bmrow2 = 0, bmrow3 = 0, bmrow4 = 0, bmrow5 = 0;
    uint8_t berow1 = 0, berow2 = 0, berow3 = 0, berow4 = 0, berow5 = 0;

    printf("%d\n", currentState);

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
            if (isEmpty) {
                isEmpty = false;
                currentState = attack_shoot_st;
            }      
         
            break;
        case attack_shoot_st:
            if (win) {
                currentState = wait_restart_st;
            }
            else {
                currentState = attack_wait_st;
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
            lcd_frameEnable();
            // CLEAR BOARD
            board_clear();
            // DRAW GAME BACKGROUND
            lcd_fillScreen(CONFIG_BACK_CLR);
            // DRAW GRID 
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
                //lcd_frameEnable();
                nav_get_loc(&r, &c);
                
                if ((r != prev_r) || (c != prev_c) || (prev_horizontal != currHor)){ //if the cursor moved or rotated
                graphics_drawHighlight(r,c,WHITE); //draw cursor
                graphics_drawMessage("PLACE YOUR SHIPS (PLACE - A   ROTATE - B)", WHITE, CONFIG_BACK_CLR); //redraw text

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
        case waiting_for_opponent_st:
            // Pack friendly ship array into 5 bytes
            for (int col = 0; col < CONFIG_BOARD_C; col++) {
                if (myShips[R1][col] == 1) {
                    bmrow1 = (bmrow1 | 1 << (col));
                }
                if (myShips[R2][col] == 1) {
                    bmrow2 = (bmrow2 | 1 << (col));
                }
                if (myShips[R3][col] == 1) {
                    bmrow3 = (bmrow3 | 1 << (col));
                }
                if (myShips[R4][col] == 1) {
                    bmrow4 = (bmrow4 | 1 << (col));
                }
                if (myShips[R5][col] == 1) {
                    bmrow5 = (bmrow5 | 1 << (col));
                }
            }

            // Send friendly ship bytes
            com_write(&bmrow1, sizeof(bmrow1));
            com_write(&bmrow2, sizeof(bmrow2));
            com_write(&bmrow3, sizeof(bmrow3));
            com_write(&bmrow4, sizeof(bmrow4));
            com_write(&bmrow5, sizeof(bmrow5));

            // Read enemy data bytes
            com_read(&berow1, sizeof(berow1));
            com_read(&berow2, sizeof(berow2));
            com_read(&berow3, sizeof(berow3));
            com_read(&berow4, sizeof(berow4));
            com_read(&berow5, sizeof(berow5));

            // printf("Packed byte row 1: %d\n", bmrow1);
            // printf("Packed byte row 2: %d\n", bmrow2);
            // printf("Packed byte row 3: %d\n", bmrow3);
            // printf("Packed byte row 4: %d\n", bmrow4);
            // printf("Packed byte row 5: %d\n\n\n", bmrow5); 

            // printf("Packed byte row 1: %d\n", berow1);
            // printf("Packed byte row 2: %d\n", berow2);
            // printf("Packed byte row 3: %d\n", berow3);
            // printf("Packed byte row 4: %d\n", berow4);
            // printf("Packed byte row 5: %d\n\n\n", berow5); 

            // Unpack enemy bytes into array
            for (int col = 0; col < CONFIG_BOARD_C; col++) {
                if ((berow1 & 1) == 1) {
                    enemyShips[R1][col] = 1;
                }

                if ((berow2 & 1) == 1) {
                    enemyShips[R2][col] = 1;
                }      

                if ((berow3 & 1) == 1) {
                    enemyShips[R3][col] = 1;
                }       

                if ((berow4 & 1) == 1) {
                    enemyShips[R4][col] = 1;
                }       

                if ((berow5 & 1) == 1) {
                    enemyShips[R5][col] = 1;
                }       

                berow1 = (berow1 >> 1);
                berow2 = (berow2 >> 1);
                berow3 = (berow3 >> 1); 
                berow4 = (berow4 >> 1); 
                berow5 = (berow5 >> 1);      
            }

            // Check to see if enemy array has been received
            for (int row = 0; row < CONFIG_BOARD_R; row++) {
                for (int col = 0; col < CONFIG_BOARD_C; col++) {
                    if (enemyShips[row][col] == 1) {
                        enemyShipsReceived = true;
                    }
                }

                if (enemyShipsReceived == true) {
                    break;
                }
            }
            
            break;
        case attack_init_st:
            // for (int row = 0; row < CONFIG_BOARD_R; row++) {
            //     for (int col = 0; col < CONFIG_BOARD_C; col++) {
            //         // Access the element at [row][col] in the myShips array
            //         int shipStatus = enemyShips[row][col];

            //         // Example operation: print the status of each cell
            //         printf("enemyShips[%d][%d] = %d\n", row, col, shipStatus);
            //     }
            // }

            // Reset grid
            lcd_fillScreen(CONFIG_BACK_CLR);
            graphics_drawGrid(CONFIG_GRID_CLR);
            board_clear();

            // Reset nav location to (0, 0)
            nav_set_loc(0, 0);

            break;
        case attack_wait_st:
            // Gather how many lives left
            //com_read(enemy_lives, sizeof(enemy_lives));

            if (!pin_get_level(HW_BTN_A)) {
                nav_get_loc(&r, &c);

                // Check if spot has not been shot
                if (board_get(r, c) == no_m) {
                    isEmpty = true;
                }
            }
            
            break;
        case attack_shoot_st:
            // SET MARK
            if (boolTurn) { // My turn
                if (is_hit(r, c)) {
                    board_set(r, c, hit_m);
                    graphics_drawX(r, c, RED);
                    enemy_lives--;
                }
                else {
                    board_set(r, c, miss_m);
                    graphics_drawO(r, c, GREEN);
                }

                if (enemy_lives == 0) {
                    win = MY_WIN;
                }
            }
            else { // Enemy turn
                if (is_hit(r, c)) {
                    board_set(r, c, hit_m);
                    graphics_drawX(r, c, RED);
                    player_lives--;
                }
                else {
                    board_set(r, c, miss_m);
                    graphics_drawO(r, c, GREEN);
                }

                if (player_lives == 0) {
                    win = ENEMY_WIN;
                }
            }

            // DISPLAY STATUS
            boolTurn = !boolTurn; // Switch turn
            graphics_drawMessage(boolTurn ? "Player 1's Turn" : "Player 2's Turn", WHITE, CONFIG_BACK_CLR);
            break;
        case wait_restart_st:
            // WAIT FOR START BUTTON
            if (!pin_get_level(HW_BTN_START)) {
                isRestart = false;
                win = 0;
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