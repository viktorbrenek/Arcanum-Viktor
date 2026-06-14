#ifndef ARCANUM_GAME_PLAYER_HOUSE_H_
#define ARCANUM_GAME_PLAYER_HOUSE_H_

#include <stdbool.h>

// Registers the personal "Void house" map. Call once during map mod load.
void player_house_init(void);

// Drops cached state so the next visit re-reads the active save. Call on game
// reset so a new character doesn't inherit the previous one's "built" flag.
void player_house_reset(void);

// Toggles the player between the world and their Void house: teleports home if
// outside, or back to the saved return location if already home.
void player_house_toggle(void);

// True if the local PC is currently inside the Void house map.
bool player_house_is_active(void);

// Called after a map finishes opening; builds the room + storage chest on the
// first ever visit.
void player_house_on_map_opened(int map_id);

#endif /* ARCANUM_GAME_PLAYER_HOUSE_H_ */
