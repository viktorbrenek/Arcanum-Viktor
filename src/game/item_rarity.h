#ifndef ARCANUM_GAME_ITEM_RARITY_H_
#define ARCANUM_GAME_ITEM_RARITY_H_

#include "game/context.h"
#include "game/stat.h"
#include "tig/color.h"

// OBJ_F_ITEM_PAD_I_1 layout:
//   bits 0-7  : ItemRarity value
//   bit  30   : cursed-bound flag (1 = cannot unequip without Remove Curse)
//   bit  31   : identified flag (1 = identified, 0 = unidentified)
// Common items are always treated as identified.
#define ITEM_RARITY_IDENTIFIED_BIT  0x80000000
#define ITEM_RARITY_CURSED_BOUND_BIT 0x40000000

typedef enum ItemRarity {
    ITEM_RARITY_NONE = 0,
    ITEM_RARITY_COMMON,
    ITEM_RARITY_UNCOMMON,
    ITEM_RARITY_RARE,
    ITEM_RARITY_EPIC,
    ITEM_RARITY_UNIQUE,
    ITEM_RARITY_CURSED,
    ITEM_RARITY_COUNT,
} ItemRarity;

// Affix IDs stored in OBJ_F_ITEM_PAD_IAS_1 slots 0..ITEM_RARITY_MAX_AFFIXES-1.
typedef enum ItemAffix {
    ITEM_AFFIX_NONE = 0,
    // --- Weapon prefixes ---
    ITEM_AFFIX_W_STRIKING,      // +3 normal damage
    ITEM_AFFIX_W_CRUSHING,      // +7 normal damage
    ITEM_AFFIX_W_ACCURATE,      // +5 to-hit
    ITEM_AFFIX_W_DEADLY,        // +10 to-hit
    ITEM_AFFIX_W_SWIFT,         // -1 speed factor (faster)
    ITEM_AFFIX_W_KEEN,          // +5% crit chance
    ITEM_AFFIX_W_SAVAGE,        // +10% crit chance
    ITEM_AFFIX_W_BLAZING,       // +4 fire damage
    // --- Armor prefixes ---
    ITEM_AFFIX_A_STURDY,        // +5 AC
    ITEM_AFFIX_A_FORTIFIED,     // +10 AC
    ITEM_AFFIX_A_FIREPROOF,     // +20% fire resistance
    ITEM_AFFIX_A_INSULATED,     // +20% electrical resistance
    ITEM_AFFIX_A_VENOMPROOF,    // +20% poison resistance
    ITEM_AFFIX_A_SANCTIFIED,    // +20% magic resistance
    ITEM_AFFIX_A_SHADOW,        // +5 silent move
    // --- Shared stat suffixes (while equipped) ---
    ITEM_AFFIX_OF_MIGHT,        // +1 STR
    ITEM_AFFIX_OF_GIANTS,       // +2 STR
    ITEM_AFFIX_OF_SWIFTNESS,    // +1 DEX
    ITEM_AFFIX_OF_THE_WIND,     // +2 DEX
    ITEM_AFFIX_OF_ENDURANCE,    // +1 CON
    ITEM_AFFIX_OF_THE_SAGE,     // +1 INT
    ITEM_AFFIX_OF_CLARITY,      // +1 PER
    ITEM_AFFIX_OF_WILL,         // +1 WIL
    ITEM_AFFIX_OF_CHARM,        // +1 CHA
    ITEM_AFFIX_OF_BEAUTY,       // +1 BEA
    // --- Hidden penalty affixes (no name, used by unique items) ---
    ITEM_AFFIX_PENALTY_STR_1,   // -1 STR
    ITEM_AFFIX_PENALTY_STR_2,   // -2 STR
    ITEM_AFFIX_PENALTY_DEX_1,   // -1 DEX
    ITEM_AFFIX_PENALTY_DEX_2,   // -2 DEX
    ITEM_AFFIX_PENALTY_CON_1,   // -1 CON
    ITEM_AFFIX_PENALTY_INT_1,   // -1 INT
    ITEM_AFFIX_PENALTY_CHA_1,   // -1 CHA
    ITEM_AFFIX_PENALTY_BEA_1,   // -1 BEA
    ITEM_AFFIX_PENALTY_SPEED_2, // -2 speed factor (slower armor)
    // --- Cursed weapon prefixes ---
    ITEM_AFFIX_CW_FRENZIED,     // +15 normal damage (unhinged fury)
    ITEM_AFFIX_CW_CORRUPTED,    // +20 to-hit (blade guides itself)
    ITEM_AFFIX_CW_SOULREAVER,   // +12 fire damage (soul-consuming flame)
    ITEM_AFFIX_CW_BLOODLUST,    // +15% critical chance (thirsts for blood)
    // --- Cursed armor prefixes ---
    ITEM_AFFIX_CA_IRONBOUND,    // +25 armor class (iron fused to flesh)
    ITEM_AFFIX_CA_WRAITHFORGED, // +40% magic resistance (forged in the void)
    ITEM_AFFIX_CA_ABYSSAL,      // +30% fire resistance (touched by the abyss)
    // --- Cursed suffixes (visible penalties) ---
    ITEM_AFFIX_CURSE_OF_FRAILTY,    // -2 CON (equipped)
    ITEM_AFFIX_CURSE_OF_WEAKNESS,   // -2 STR (equipped)
    ITEM_AFFIX_CURSE_OF_CLUMSINESS, // -2 DEX (equipped)
    ITEM_AFFIX_CURSE_OF_DIMNESS,    // -2 INT (equipped)
    ITEM_AFFIX_CURSE_OF_COWARDICE,  // -2 WIL (equipped)
    ITEM_AFFIX_COUNT,
} ItemAffix;

#define ITEM_RARITY_MAX_AFFIXES 6

// Unique item identifiers — stored as a "rarity" of ITEM_RARITY_UNIQUE
// plus a unique ID in an extra pad slot (slot 5).
typedef enum UniqueItemId {
    UNIQUE_ITEM_NONE = 0,
    // Warrior build-enabler: high damage + crit, at the cost of DEX
    UNIQUE_BLADE_OF_ALBERICH,
    // Shadow/thief build-enabler: extreme stealth + DEX at cost of STR
    UNIQUE_VEIL_OF_SHADOWS,
    // Tank build-enabler: massive AC + CON, but very slow
    UNIQUE_IRONCLAD_COGPLATE,
    // Necromancer enabler: WIL bonus, grants Harm spell, costs BEA + CHA
    UNIQUE_RING_OF_MEPHISTIS,
    // Force mage enabler: high INT + WIL, converts STR to INT (STR penalty)
    UNIQUE_TULLIAN_FOCUS,
    UNIQUE_ITEM_COUNT,
} UniqueItemId;

// ---- Public API ----

ItemRarity item_rarity_get(int64_t item_obj);
void item_rarity_set(int64_t item_obj, ItemRarity rarity);
int item_affix_get(int64_t item_obj, int slot);
void item_affix_set(int64_t item_obj, int slot, int affix);

// Roll rarity + affixes for a weapon or armor item. No-op if already rolled.
void item_rarity_roll(int64_t item_obj);

// Roll affixes for a specific forced rarity. No-op if already rolled.
void item_rarity_roll_forced(int64_t item_obj, ItemRarity forced_rarity);

// Build a magic item name like "Crushing Sword of Giants" into out_buf.
// Falls back to base_name for COMMON/NONE items.
void item_rarity_generate_name(int64_t item_obj, const char* base_name,
    char* out_buf, int buf_size);

// Append affix description lines to buf (for examine window).
void item_rarity_describe_affixes(int64_t item_obj, char* buf, int buf_size);

// Force-apply a specific unique definition to item_obj (bypasses random roll).
// No-op if item already has a rarity assigned.
void item_rarity_apply_unique(int64_t item_obj, UniqueItemId uid);

// Check critter BP against unique_table drop_critter_bp; if match, apply that unique
// to the first inventory item of matching type. Call before item_rarity_roll loop.
void item_rarity_try_boss_drop(int64_t critter_obj);

// Identification: Uncommon+ items spawn unidentified.
// item_rarity_is_identified returns true for COMMON/NONE always.
bool item_rarity_is_identified(int64_t item_obj);
void item_rarity_identify(int64_t item_obj);

// Cursed binding: cursed items may be bound (cannot unequip without Remove Curse).
bool item_rarity_is_cursed_bound(int64_t item_obj);

// Called from stat_level_get to apply equipped-item stat bonuses.
int item_rarity_adjust_stat(int64_t critter_obj, int stat, int value);

// Tooltip: compact line of equipped stat bonuses e.g. "+2 STR | -2 DEX (equipped)".
// buf[0] = '\0' if no stat bonuses.
void item_rarity_format_equipped_stats(int64_t item_obj, char* buf, int buf_size);

// Tooltip: newline-separated affix bonus list for the ARPG floating tooltip.
// Each line is e.g. "+7 normal damage" or "+2 STR (equipped)".
// buf[0] = '\0' if no affixes.
void item_rarity_format_tooltip_affixes(int64_t item_obj, char* buf, int buf_size);

// Infer display rarity without modifying the object.
// Returns stored rarity if already rolled; otherwise maps OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY
// to UNCOMMON/RARE/EPIC for base-game magic weapons and armor.
// Use only for UI color — never for game logic.
ItemRarity item_rarity_infer(int64_t item_obj);

// UI color per rarity.
tig_color_t item_rarity_color(ItemRarity rarity);

#endif /* ARCANUM_GAME_ITEM_RARITY_H_ */
