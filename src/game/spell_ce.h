#ifndef ARCANUM_GAME_SPELL_CE_H_
#define ARCANUM_GAME_SPELL_CE_H_

#include <stdint.h>
#include "game/timeevent.h"

// Called from magictech_process() before the component loop (action == BEGIN only).
// May cap or reduce *aptitude_ptr for Harm spam prevention.
void spell_ce_pre_begin(int spell, int64_t caster_obj, int* aptitude_ptr);

// Called from magictech_process() per critter target after all components fire (BEGIN and MAINTAIN).
// Applies secondary school effects: DoTs, chains, slow, stun, stat steals, etc.
// action: MAGICTECH_ACTION_BEGIN or MAGICTECH_ACTION_MAINTAIN
void spell_ce_on_target(int spell, int action, int64_t caster_obj, int64_t target_obj);

// Inflicts unresistable Blood Magic HP damage cost on the caster.
void spell_ce_blood_magic_pay(int64_t caster_obj, int hp_cost);

// Lifetaker (Black Necro): called once per maintain tick from the maintenance
// hook. Drains a few HP from every enemy near the caster and heals the caster
// for the total drained.
void spell_ce_lifetaker_drain(int64_t caster_obj);

// Timeevent processor for Harm cooldown — just expires, no-op.
bool spell_ce_harm_cooldown_process(TimeEvent* timeevent);

// Called from combat_dmg to get Wolf Form life-on-hit bonus (2 if active, else 0).
int spell_ce_life_on_hit_bonus(int64_t attacker_obj);

// Called from combat_dmg to get Bear God Form thorns bonus (6 if active, else 0).
int spell_ce_thorns_bonus(int64_t target_obj);

// True if obj is currently in Wolf Form (Lycanthropy) — apply bleed on hit.
bool spell_ce_has_wolf_form(int64_t attacker_obj);

// True if obj is currently in Lizard Form — apply poison on hit.
bool spell_ce_has_lizard_form(int64_t attacker_obj);

#endif /* ARCANUM_GAME_SPELL_CE_H_ */
