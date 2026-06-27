#ifndef ARCANUM_GAME_ITEM_TECH_PROPS_H_
#define ARCANUM_GAME_ITEM_TECH_PROPS_H_

#include <stdint.h>
#include "game/timeevent.h"

// Hardcoded on-hit procs for tech weapons. Called from combat.c after hit lands.
// combat_flags is CombatContext.flags — needed for crit detection (CF_CRITICAL).
void tech_weapon_on_hit(int64_t attacker_obj, int64_t weapon_obj, int64_t target_obj,
                        unsigned int combat_flags);

// Query helpers for UI — check pending DoT timeevents for a given object.
bool dot_has_active(int64_t obj, int timeevent_type);  // check specific type
bool dot_has_any_active(int64_t obj);                  // any CE DoT (fire/acid/bleed/poison_weapon)

// Timeevent processors for DoT types.
bool fire_dot_timeevent_process(TimeEvent* timeevent);
bool acid_dot_timeevent_process(TimeEvent* timeevent);
bool bleed_dot_timeevent_process(TimeEvent* timeevent);
bool poison_weapon_dot_timeevent_process(TimeEvent* timeevent);

// Public apply helpers — call from weapon on-hit, spell procs, affix procs, etc.
void apply_fire_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks);
void apply_acid_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks);
void apply_bleed_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks);
void apply_poison_weapon_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks);

// Drench (Flood spell, water college) — non-stacking timed -2 DX debuff (effect 371),
// auto-expires after DRENCH_DURATION_MS. Re-applying refreshes the timer (never stacks).
#define EFFECT_DRENCHED 371
void apply_drench(int64_t target);
bool drench_timeevent_process(TimeEvent* timeevent);

#endif /* ARCANUM_GAME_ITEM_TECH_PROPS_H_ */
