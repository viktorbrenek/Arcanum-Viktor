#ifndef ARCANUM_GAME_MANNOX_VAULT_H_
#define ARCANUM_GAME_MANNOX_VAULT_H_

#include <stdbool.h>

// The Ring of Brodar / Mannox vault is cut content: the vault door teleporter
// points at an "Unused" MapList placeholder slot whose interior map was never
// built. Instead of loading a non-existent map (which used to hang the game),
// CE treats stepping into the vault as opening the stone sarcophagus and hands
// the player Mannox's relics in place.

// True if `map_name` is the placeholder the vault teleporter targets, i.e. the
// teleport should be intercepted rather than loading a map.
bool mannox_vault_is_target(const char* map_name);

// Grant the vault reward (Sword + Journal of Mannox) to the local PC. Guarded so
// it only ever grants once.
void mannox_vault_give_reward(void);

#endif /* ARCANUM_GAME_MANNOX_VAULT_H_ */
