#ifndef GAME_H_
#define GAME_H_

#define CONFIG_BOARD_R 8
#define CONFIG_BOARD_C 8

// Structure for a ship
typedef struct {
    int length;       // Length of the ship
    int r;            // Row position
    int c;            // Column position
    bool horizontal;  // Orientation of the ship
    bool sunk;        // If the ship is sunk
    color_t color;    //Color of placement
} ship_t;

// Initialize the game logic.
void game_init(void);

// Update the game logic.
void game_tick(void);

bool is_hit(int8_t r, int8_t c, int shipMap[CONFIG_BOARD_R][CONFIG_BOARD_C]);

bool is_validShip(ship_t shipToCheck, int8_t r, int8_t c, bool currHor);

#endif // GAME_H_
