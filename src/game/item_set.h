#ifndef ARCANUM_GAME_ITEM_SET_H_
#define ARCANUM_GAME_ITEM_SET_H_

#include "game/context.h"
#include "game/timeevent.h"

// Set ID stored in OBJ_F_ITEM_PAD_I_1 bits 8-23.
// ITEM_RARITY_SET items always have a non-zero SetId.
typedef enum SetId {
    SET_NONE = 0,
    SET_DREAD_GUARD,
    SET_SHROUD_OF_UNSEEN,
    SET_VENDIGROTHIAN_CONFLUENCE,
    SET_COUNT,
} SetId;

// Read/write set ID from item PAD field.
SetId item_set_get(int64_t item_obj);
void  item_set_set(int64_t item_obj, SetId id);

// Count how many pieces of a given set are currently equipped on a critter.
int item_set_count_equipped(int64_t critter_obj, SetId id);

// Returns 0 (no bonus), 1 (2/3 pieces), or 2 (3/3 pieces).
int item_set_active_tier(int64_t critter_obj, SetId id);

// Called from stat_level_get — applies set stat bonuses for active tiers.
int item_set_adjust_stat(int64_t critter_obj, int stat, int value);

// Called from item_equipped / item_unequipped in item.c.
void item_set_on_equip(int64_t item_obj, int64_t critter_obj);
void item_set_on_unequip(int64_t item_obj, int64_t critter_obj);

// Called from mt_item notify hooks.
void item_set_notify_hit(int64_t attacker_obj, int64_t weapon_obj, int64_t target_obj);
void item_set_notify_hit_taken(int64_t victim_obj, int64_t attacker_obj);
void item_set_notify_kill(int64_t killer_obj, int64_t victim_obj);

// Display helpers for tooltip.
const char* item_set_name(SetId id);
const char* item_set_tier_bonus_str(SetId id, int tier);

// Timeevent callback: restores a stat and/or clears spell flags after a proc debuff expires.
bool item_set_proc_effect_end_timeevent_process(TimeEvent* timeevent);

#endif /* ARCANUM_GAME_ITEM_SET_H_ */
