#ifndef ARCANUM_GAME_ITEM_ORB_H_
#define ARCANUM_GAME_ITEM_ORB_H_

#include "game/context.h"

// Art num base for orb inventory/ground art (TIG_ART_ITEM_TYPE_GENERIC, subtype 0).
// ORB_REFORGING → num 900, ORB_ASCENSION → 901, etc.
#define ORB_ART_NUM_BASE 900

// Orb type — stored in OBJ_F_GENERIC_USAGE_BONUS on items with OGF_IS_ORB flag set.
typedef enum OrbType {
    ORB_NONE      = 0,
    ORB_REFORGING = 1,  // reroll affixes, keep rarity (UNCOMMON-CURSED)
    ORB_ASCENSION = 2,  // upgrade rarity one tier (UNCOMMON->RARE, RARE->EPIC)
    ORB_CLEANSING = 3,  // remove curse, reroll affixes as RARE
    ORB_ANNULMENT = 4,  // strip all affixes, reset to COMMON
    ORB_AWAKENING    = 5,  // awaken a plain (NONE/COMMON) item to UNCOMMON; common drop
    ORB_AUGMENTATION = 6,  // add one random affix if item has an open slot
    ORB_CORRUPTION   = 7,  // random chaotic effect: ascend / curse / augment / strip / nothing
    ORB_ENTROPY      = 8,  // reroll each affix within its own pool, keep rarity and count
    ORB_COUNT,
} OrbType;

// Randomly select an orb type using weighted drop probabilities.
// Awakening 35% · Reforging 20% · Annulment 15% · Augmentation 12% · Cleansing 8% · Ascension 5% · Entropy 3% · Corruption 2%.
OrbType item_orb_roll_type(void);

// Display name for an orb type, e.g. "Orb of Reforging". Returns NULL for ORB_NONE.
const char* item_orb_display_name(OrbType type);

// Short description of what an orb does (one sentence). Returns NULL for ORB_NONE.
const char* item_orb_description(OrbType type);

// Get orb type from a GENERIC item (returns ORB_NONE if not an orb).
OrbType item_orb_get_type(int64_t item_obj);

// Mark a GENERIC item as an orb of the given type.
void item_orb_set_type(int64_t item_obj, OrbType type);

// Try to apply item_obj as a crafting orb. When target_obj == source_obj or NULL,
// falls back to hovered inventory item. Consumes item_obj on success.
// Returns true if handled (caller should return from item_use_on_obj).
bool item_orb_try_apply(int64_t source_obj, int64_t item_obj, int64_t target_obj);

// Stack count stored in OBJ_F_GENERIC_PAD_IAS_1[0]. Returns >= 1 (0 treated as 1).
int item_orb_stack_count_get(int64_t item_obj);
void item_orb_stack_count_set(int64_t item_obj, int count);

// Find first orb of given type in critter's inventory. Returns OBJ_HANDLE_NULL if none.
int64_t item_orb_find_in_inventory(int64_t critter_obj, OrbType type);

#endif /* ARCANUM_GAME_ITEM_ORB_H_ */
