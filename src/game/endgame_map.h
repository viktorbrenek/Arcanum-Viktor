#ifndef ARCANUM_GAME_ENDGAME_MAP_H_
#define ARCANUM_GAME_ENDGAME_MAP_H_

#include <stdbool.h>
#include <stdint.h>

// Called from map_mod_load after MapList.mes is parsed.
// Injects the endgame_dungeon map entry so map_open_in_game can find it.
void endgame_map_init(void);

// Called from item_orb_try_apply (ORB_MAP case).
// Saves return map+location, wipes dungeon save state, loads dungeon, spawns critters.
// Returns false if NG+ is not unlocked or dungeon is already active.
bool endgame_map_enter(void);

// Called when the player activates the exit portal or all critters are dead.
// Restores the map and tile the player came from.
void endgame_map_exit(void);

// Called from critter_kill whenever a critter dies.
// On the endgame map: decrements counter; when zero, initiates exit sequence.
void endgame_map_on_critter_killed(int64_t critter_obj);

// Called from map_open_in_game after every successful map load.
// If the dungeon just loaded and critter spawn is pending, spawns critters.
void endgame_map_on_map_opened(int map_id);

// True when the current map is the endgame dungeon.
bool endgame_map_is_active(void);

int endgame_map_get_tier(void);

#endif /* ARCANUM_GAME_ENDGAME_MAP_H_ */
