#ifndef GAME_H_
#define GAME_H_

// Initialize the game logic.
void game_init(void);

// Update the game logic.
void game_tick(void);

bool is_hit(int8_t r, int8_t c);

#endif // GAME_H_
