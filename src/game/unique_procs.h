#ifndef ARCANUM_GAME_UNIQUE_PROCS_H_
#define ARCANUM_GAME_UNIQUE_PROCS_H_

#include <stdint.h>
#include <stdbool.h>

// Called after hit lands (dam > 0, attacker valid).
// was_concealed: OCF_IS_CONCEALED was set on attacker before this combat call.
void unique_procs_on_hit(int64_t attacker, int64_t target, int dam, bool was_concealed);

// Called when target dies (after critter_is_dead confirmed true).
void unique_procs_on_kill(int64_t attacker, int64_t target);

// Called after defender takes physical damage (dam > 0).
void unique_procs_on_damage_received(int64_t defender, int64_t attacker, int dam);

// Called from spell_ce_on_target for offensive spells (BEGIN only, caster != target).
// Returns true if an echo hit should be applied (Tullian Focus).
bool unique_procs_on_spell_hit(int spell, int64_t caster, int64_t target);

// Called from spell_ce_on_target when target takes spell damage (BEGIN only).
// dam: approximate spell damage (0 if unknown). Handles Galatea Mirror absorption.
void unique_procs_on_spell_received(int64_t caster, int64_t target, int dam);

// Returns false if target's Stonehide Mantle blocks knockdown/stun.
bool unique_procs_can_knockdown(int64_t target);

#endif /* ARCANUM_GAME_UNIQUE_PROCS_H_ */
