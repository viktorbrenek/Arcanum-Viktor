#include "game/item_rarity.h"

#include <stdio.h>
#include <string.h>

#include "game/damage_type.h"
#include "game/ng_plus.h"
#include "game/item.h"
#include "game/item_set.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/object.h"
#include "game/random.h"
#include "game/resistance.h"
#include "game/spell.h"
#include "game/stat.h"
#include "tig/color.h"

// ---------------------------------------------------------------------------
// Affix definitions
// ---------------------------------------------------------------------------

typedef struct AffixDef {
    const char* prefix;
    const char* suffix;
    bool weapon_eligible;
    bool armor_eligible;
    int weapon_field;
    int weapon_field_idx;
    int weapon_value;
    int armor_field;
    int armor_field_idx;
    int armor_value;
    int equip_stat;
    int equip_stat_val;
    const char* desc;   // human-readable bonus for tooltip; NULL = auto-format from equip_stat
    int equip_loh;      // life on hit HP recovery per successful hit (0 = none)
    int equip_thorns;   // damage reflected to attacker per hit received (0 = none)
} AffixDef;

// Must match ItemAffix enum order exactly (index 0 = ITEM_AFFIX_NONE sentinel).
static const AffixDef affix_table[ITEM_AFFIX_COUNT] = {
    // ITEM_AFFIX_NONE
    { NULL, NULL, false, false, -1, -1, 0, -1, -1, 0, -1, 0, NULL },
    // ITEM_AFFIX_W_STRIKING
    { "Striking", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_NORMAL, 3, -1, -1, 0, -1, 0,
        "+3 normal damage" },
    // ITEM_AFFIX_W_CRUSHING
    { "Crushing", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_NORMAL, 7, -1, -1, 0, -1, 0,
        "+7 normal damage" },
    // ITEM_AFFIX_W_ACCURATE
    { "Accurate", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_HIT_ADJ, -1, 5, -1, -1, 0, -1, 0,
        "+5 to-hit" },
    // ITEM_AFFIX_W_DEADLY
    { "Deadly", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_HIT_ADJ, -1, 10, -1, -1, 0, -1, 0,
        "+10 to-hit" },
    // ITEM_AFFIX_W_SWIFT
    { "Swift", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_SPEED_ADJ, -1, -1, -1, -1, 0, -1, 0,
        "-1 speed (faster)" },
    // ITEM_AFFIX_W_KEEN
    { "Keen", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_CRIT_HIT_CHANCE, -1, 5, -1, -1, 0, -1, 0,
        "+5% critical chance" },
    // ITEM_AFFIX_W_SAVAGE
    { "Savage", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_CRIT_HIT_CHANCE, -1, 10, -1, -1, 0, -1, 0,
        "+10% critical chance" },
    // ITEM_AFFIX_W_BLAZING
    { "Blazing", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_FIRE, 4, -1, -1, 0, -1, 0,
        "+4 fire damage" },
    // ITEM_AFFIX_A_STURDY
    { "Sturdy", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 5, -1, 0,
        "+5 armor class" },
    // ITEM_AFFIX_A_FORTIFIED
    { "Fortified", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 10, -1, 0,
        "+10 armor class" },
    // ITEM_AFFIX_A_FIREPROOF
    { "Fireproof", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_FIRE, 20, -1, 0,
        "+20% fire resistance" },
    // ITEM_AFFIX_A_INSULATED
    { "Insulated", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_ELECTRICAL, 20, -1, 0,
        "+20% electrical resistance" },
    // ITEM_AFFIX_A_VENOMPROOF
    { "Venomproof", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_POISON, 20, -1, 0,
        "+20% poison resistance" },
    // ITEM_AFFIX_A_SANCTIFIED
    { "Sanctified", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_MAGIC, 20, -1, 0,
        "+20% magic resistance" },
    // ITEM_AFFIX_A_SHADOW
    { "Shadow", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_SILENT_MOVE_ADJ, -1, 5, -1, 0,
        "+5 silent move" },
    // ITEM_AFFIX_OF_MIGHT
    { NULL, "of Might", true, true, -1, -1, 0, -1, -1, 0, STAT_STRENGTH, 1, NULL },
    // ITEM_AFFIX_OF_GIANTS
    { NULL, "of Giants", true, true, -1, -1, 0, -1, -1, 0, STAT_STRENGTH, 2, NULL },
    // ITEM_AFFIX_OF_SWIFTNESS
    { NULL, "of Swiftness", true, true, -1, -1, 0, -1, -1, 0, STAT_DEXTERITY, 1, NULL },
    // ITEM_AFFIX_OF_THE_WIND
    { NULL, "of the Wind", true, true, -1, -1, 0, -1, -1, 0, STAT_DEXTERITY, 2, NULL },
    // ITEM_AFFIX_OF_ENDURANCE
    { NULL, "of Endurance", true, true, -1, -1, 0, -1, -1, 0, STAT_CONSTITUTION, 1, NULL },
    // ITEM_AFFIX_OF_THE_SAGE
    { NULL, "of the Sage", true, true, -1, -1, 0, -1, -1, 0, STAT_INTELLIGENCE, 1, NULL },
    // ITEM_AFFIX_OF_CLARITY
    { NULL, "of Clarity", true, true, -1, -1, 0, -1, -1, 0, STAT_PERCEPTION, 1, NULL },
    // ITEM_AFFIX_OF_WILL
    { NULL, "of Will", true, true, -1, -1, 0, -1, -1, 0, STAT_WILLPOWER, 1, NULL },
    // ITEM_AFFIX_OF_CHARM
    { NULL, "of Charm", true, true, -1, -1, 0, -1, -1, 0, STAT_CHARISMA, 1, NULL },
    // ITEM_AFFIX_OF_BEAUTY
    { NULL, "of Beauty", true, true, -1, -1, 0, -1, -1, 0, STAT_BEAUTY, 1, NULL },
    // ITEM_AFFIX_PENALTY_STR_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_STRENGTH, -1, NULL },
    // ITEM_AFFIX_PENALTY_STR_2
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_STRENGTH, -2, NULL },
    // ITEM_AFFIX_PENALTY_DEX_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_DEXTERITY, -1, NULL },
    // ITEM_AFFIX_PENALTY_DEX_2
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_DEXTERITY, -2, NULL },
    // ITEM_AFFIX_PENALTY_CON_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_CONSTITUTION, -1, NULL },
    // ITEM_AFFIX_PENALTY_INT_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_INTELLIGENCE, -1, NULL },
    // ITEM_AFFIX_PENALTY_CHA_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_CHARISMA, -1, NULL },
    // ITEM_AFFIX_PENALTY_BEA_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_BEAUTY, -1, NULL },
    // ITEM_AFFIX_PENALTY_SPEED_2
    { NULL, NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_SILENT_MOVE_ADJ, -1, -2, -1, 0, NULL },
    // ITEM_AFFIX_CW_FRENZIED
    { "Frenzied", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_NORMAL, 15, -1, -1, 0, -1, 0,
        "+15 normal damage" },
    // ITEM_AFFIX_CW_CORRUPTED
    { "Corrupted", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_HIT_ADJ, -1, 20, -1, -1, 0, -1, 0,
        "+20 to-hit" },
    // ITEM_AFFIX_CW_SOULREAVER
    { "Soulreaver", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_FIRE, 12, -1, -1, 0, -1, 0,
        "+12 fire damage" },
    // ITEM_AFFIX_CW_BLOODLUST
    { "Bloodlust", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_CRIT_HIT_CHANCE, -1, 15, -1, -1, 0, -1, 0,
        "+15% critical chance" },
    // ITEM_AFFIX_CA_IRONBOUND
    { "Ironbound", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 25, -1, 0,
        "+25 armor class" },
    // ITEM_AFFIX_CA_WRAITHFORGED
    { "Wraithforged", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_MAGIC, 40, -1, 0,
        "+40% magic resistance" },
    // ITEM_AFFIX_CA_ABYSSAL
    { "Abyssal", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_FIRE, 30, -1, 0,
        "+30% fire resistance" },
    // ITEM_AFFIX_CURSE_OF_FRAILTY
    { NULL, "of Frailty", true, true, -1, -1, 0, -1, -1, 0, STAT_CONSTITUTION, -2,
        "-2 CON (cursed)" },
    // ITEM_AFFIX_CURSE_OF_WEAKNESS
    { NULL, "of Weakness", true, true, -1, -1, 0, -1, -1, 0, STAT_STRENGTH, -2,
        "-2 STR (cursed)" },
    // ITEM_AFFIX_CURSE_OF_CLUMSINESS
    { NULL, "of Clumsiness", true, true, -1, -1, 0, -1, -1, 0, STAT_DEXTERITY, -2,
        "-2 DEX (cursed)" },
    // ITEM_AFFIX_CURSE_OF_DIMNESS
    { NULL, "of Dimness", true, true, -1, -1, 0, -1, -1, 0, STAT_INTELLIGENCE, -2,
        "-2 INT (cursed)" },
    // ITEM_AFFIX_CURSE_OF_COWARDICE
    { NULL, "of Cowardice", true, true, -1, -1, 0, -1, -1, 0, STAT_WILLPOWER, -2,
        "-2 WIL (cursed)" },
    // ITEM_AFFIX_W_VENOMOUS
    { "Venomous", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_POISON, 5, -1, -1, 0, -1, 0,
        "+5 poison damage" },
    // ITEM_AFFIX_W_THUNDERING
    { "Thundering", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_ELECTRICAL, 5, -1, -1, 0, -1, 0,
        "+5 electrical damage" },
    // ITEM_AFFIX_W_BRUTAL
    { "Brutal", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_NORMAL, 5, -1, -1, 0, -1, 0,
        "+5 normal damage" },
    // ITEM_AFFIX_W_VICIOUS
    { "Vicious", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_NORMAL, 12, -1, -1, 0, -1, 0,
        "+12 normal damage" },
    // ITEM_AFFIX_W_BALANCED
    { "Balanced", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_HIT_ADJ, -1, 7, -1, -1, 0, -1, 0,
        "+7 to-hit" },
    // ITEM_AFFIX_W_RAZOR
    { "Razor", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_CRIT_HIT_CHANCE, -1, 7, -1, -1, 0, -1, 0,
        "+7% critical chance" },
    // ITEM_AFFIX_W_FLEET
    { "Fleet", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_SPEED_ADJ, -1, -2, -1, -1, 0, -1, 0,
        "-2 speed (very fast)" },
    // ITEM_AFFIX_A_REINFORCED
    { "Reinforced", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 15, -1, 0,
        "+15 armor class" },
    // ITEM_AFFIX_A_BLAZEWARD
    { "Blazeward", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_FIRE, 30, -1, 0,
        "+30% fire resistance" },
    // ITEM_AFFIX_A_SPELLWARD
    { "Spellward", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_MAGIC, 15, -1, 0,
        "+15% magic resistance" },
    // ITEM_AFFIX_A_PHANTOM
    { "Phantom", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_SILENT_MOVE_ADJ, -1, 10, -1, 0,
        "+10 silent move" },
    // ITEM_AFFIX_A_GROUNDED
    { "Grounded", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_ELECTRICAL, 25, -1, 0,
        "+25% electrical resistance" },
    // ITEM_AFFIX_OF_THE_OX
    { NULL, "of the Ox", true, true, -1, -1, 0, -1, -1, 0, STAT_CONSTITUTION, 2, NULL },
    // ITEM_AFFIX_OF_THE_HAWK
    { NULL, "of the Hawk", true, true, -1, -1, 0, -1, -1, 0, STAT_PERCEPTION, 2, NULL },
    // ITEM_AFFIX_OF_THE_ORACLE
    { NULL, "of the Oracle", true, true, -1, -1, 0, -1, -1, 0, STAT_WILLPOWER, 2, NULL },
    // ITEM_AFFIX_OF_THE_SCHOLAR
    { NULL, "of the Scholar", true, true, -1, -1, 0, -1, -1, 0, STAT_INTELLIGENCE, 2, NULL },
    // ITEM_AFFIX_OF_CELERITY
    { NULL, "of Celerity", true, true, -1, -1, 0, -1, -1, 0, STAT_SPEED, 1, NULL },
    // ITEM_AFFIX_OF_TITANS
    { NULL, "of Titans", true, true, -1, -1, 0, -1, -1, 0, STAT_STRENGTH, 3, NULL },
    // ITEM_AFFIX_PENALTY_PER_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_PERCEPTION, -1, NULL },
    // ITEM_AFFIX_PENALTY_WIL_1
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_WILLPOWER, -1, NULL },
    // ITEM_AFFIX_PENALTY_SPD_2
    { NULL, NULL, true, true, -1, -1, 0, -1, -1, 0, STAT_SPEED, -2, NULL },
    // ITEM_AFFIX_CW_RENDING
    { "Rending", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_ELECTRICAL, 8, -1, -1, 0, -1, 0,
        "+8 electrical damage" },
    // ITEM_AFFIX_CW_PESTILENT
    { "Pestilent", NULL, true, false,
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_POISON, 10, -1, -1, 0, -1, 0,
        "+10 poison damage" },
    // ITEM_AFFIX_CA_STYGIAN
    { "Stygian", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_POISON, 35, -1, 0,
        "+35% poison resistance" },
    // ITEM_AFFIX_CA_THUNDERCLAD
    { "Thunderclad", NULL, false, true,
        -1, -1, 0, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_ELECTRICAL, 30, -1, 0,
        "+30% electrical resistance" },
    // ITEM_AFFIX_CURSE_OF_BLINDNESS
    { NULL, "of Blindness", true, true, -1, -1, 0, -1, -1, 0, STAT_PERCEPTION, -2,
        "-2 PER (cursed)" },
    // ITEM_AFFIX_CURSE_OF_LETHARGY
    { NULL, "of Lethargy", true, true, -1, -1, 0, -1, -1, 0, STAT_SPEED, -2,
        "-2 SPD (cursed)" },
    // ITEM_AFFIX_OF_THE_VAMPIRE
    { NULL, "of the Vampire", true, true, -1, -1, 0, -1, -1, 0, -1, 0,
        "+1 life on hit", 1 },
    // ITEM_AFFIX_OF_THORNS
    { NULL, "of Thorns", false, true, -1, -1, 0, -1, -1, 0, -1, 0,
        "+2 thorns (damage returned to attacker)", 0, 2 },
};

// ---------------------------------------------------------------------------
// Affix pools for random selection
// ---------------------------------------------------------------------------

static const int weapon_prefix_pool[] = {
    ITEM_AFFIX_W_STRIKING,
    ITEM_AFFIX_W_CRUSHING,
    ITEM_AFFIX_W_ACCURATE,
    ITEM_AFFIX_W_DEADLY,
    ITEM_AFFIX_W_SWIFT,
    ITEM_AFFIX_W_KEEN,
    ITEM_AFFIX_W_SAVAGE,
    ITEM_AFFIX_W_BLAZING,
    ITEM_AFFIX_W_VENOMOUS,
    ITEM_AFFIX_W_THUNDERING,
    ITEM_AFFIX_W_BRUTAL,
    ITEM_AFFIX_W_VICIOUS,
    ITEM_AFFIX_W_BALANCED,
    ITEM_AFFIX_W_RAZOR,
    ITEM_AFFIX_W_FLEET,
};
static const int weapon_prefix_pool_size = (int)(sizeof(weapon_prefix_pool) / sizeof(weapon_prefix_pool[0]));

static const int armor_prefix_pool[] = {
    ITEM_AFFIX_A_STURDY,
    ITEM_AFFIX_A_FORTIFIED,
    ITEM_AFFIX_A_FIREPROOF,
    ITEM_AFFIX_A_INSULATED,
    ITEM_AFFIX_A_VENOMPROOF,
    ITEM_AFFIX_A_SANCTIFIED,
    ITEM_AFFIX_A_SHADOW,
    ITEM_AFFIX_A_REINFORCED,
    ITEM_AFFIX_A_BLAZEWARD,
    ITEM_AFFIX_A_SPELLWARD,
    ITEM_AFFIX_A_PHANTOM,
    ITEM_AFFIX_A_GROUNDED,
};
static const int armor_prefix_pool_size = (int)(sizeof(armor_prefix_pool) / sizeof(armor_prefix_pool[0]));

static const int suffix_pool[] = {
    ITEM_AFFIX_OF_MIGHT,
    ITEM_AFFIX_OF_GIANTS,
    ITEM_AFFIX_OF_SWIFTNESS,
    ITEM_AFFIX_OF_THE_WIND,
    ITEM_AFFIX_OF_ENDURANCE,
    ITEM_AFFIX_OF_THE_SAGE,
    ITEM_AFFIX_OF_CLARITY,
    ITEM_AFFIX_OF_WILL,
    ITEM_AFFIX_OF_CHARM,
    ITEM_AFFIX_OF_BEAUTY,
    ITEM_AFFIX_OF_THE_OX,
    ITEM_AFFIX_OF_THE_HAWK,
    ITEM_AFFIX_OF_THE_ORACLE,
    ITEM_AFFIX_OF_THE_SCHOLAR,
    ITEM_AFFIX_OF_CELERITY,
    ITEM_AFFIX_OF_TITANS,
};
static const int suffix_pool_size = (int)(sizeof(suffix_pool) / sizeof(suffix_pool[0]));

static const int cursed_weapon_prefix_pool[] = {
    ITEM_AFFIX_CW_FRENZIED,
    ITEM_AFFIX_CW_CORRUPTED,
    ITEM_AFFIX_CW_SOULREAVER,
    ITEM_AFFIX_CW_BLOODLUST,
    ITEM_AFFIX_CW_RENDING,
    ITEM_AFFIX_CW_PESTILENT,
};
static const int cursed_weapon_prefix_pool_size = (int)(sizeof(cursed_weapon_prefix_pool) / sizeof(cursed_weapon_prefix_pool[0]));

static const int cursed_armor_prefix_pool[] = {
    ITEM_AFFIX_CA_IRONBOUND,
    ITEM_AFFIX_CA_WRAITHFORGED,
    ITEM_AFFIX_CA_ABYSSAL,
    ITEM_AFFIX_CA_STYGIAN,
    ITEM_AFFIX_CA_THUNDERCLAD,
};
static const int cursed_armor_prefix_pool_size = (int)(sizeof(cursed_armor_prefix_pool) / sizeof(cursed_armor_prefix_pool[0]));

// Rare affixes only available on EPIC items (second suffix slot, 30% chance).
static const int rare_suffix_pool[] = {
    ITEM_AFFIX_OF_THE_VAMPIRE,
    ITEM_AFFIX_OF_THORNS,
};
static const int rare_suffix_pool_size = (int)(sizeof(rare_suffix_pool) / sizeof(rare_suffix_pool[0]));

static const int cursed_suffix_pool[] = {
    ITEM_AFFIX_CURSE_OF_FRAILTY,
    ITEM_AFFIX_CURSE_OF_WEAKNESS,
    ITEM_AFFIX_CURSE_OF_CLUMSINESS,
    ITEM_AFFIX_CURSE_OF_DIMNESS,
    ITEM_AFFIX_CURSE_OF_COWARDICE,
    ITEM_AFFIX_CURSE_OF_BLINDNESS,
    ITEM_AFFIX_CURSE_OF_LETHARGY,
};
static const int cursed_suffix_pool_size = (int)(sizeof(cursed_suffix_pool) / sizeof(cursed_suffix_pool[0]));

// ---------------------------------------------------------------------------
// Unique item definitions
// ---------------------------------------------------------------------------

typedef struct UniqueItemDef {
    const char* name;
    int obj_type;                              // OBJ_TYPE_WEAPON or OBJ_TYPE_ARMOR
    int drop_critter_bp;                       // BP_* that guarantees this drop; 0 = world random only
    int affixes[ITEM_RARITY_MAX_AFFIXES];      // ITEM_AFFIX_* entries, padded with NONE
    // Extra baked bonuses beyond what affixes handle:
    int extra_weapon_field;
    int extra_weapon_field_idx;
    int extra_weapon_value;
    int extra_armor_field;
    int extra_armor_field_idx;
    int extra_armor_value;
} UniqueItemDef;

// Slot 5 stores the UniqueItemId so we can look up the definition.
#define UNIQUE_ID_SLOT 5

static const UniqueItemDef unique_table[UNIQUE_ITEM_COUNT] = {
    // UNIQUE_ITEM_NONE (unused, index 0)
    { NULL, 0, 0, { 0 }, -1, -1, 0, -1, -1, 0 },

    // UNIQUE_BLADE_OF_ALBERICH
    // Warrior blade: extreme damage + crit at cost of dexterity.
    // Affixes: Crushing, Savage, of Giants, Penalty:DEX-2
    {
        "Blade of Alberich",
        OBJ_TYPE_WEAPON,
        0, // drop_critter_bp: set to e.g. BP_DARK_CHAMPION_NPC for guaranteed drop
        {
            ITEM_AFFIX_W_CRUSHING,
            ITEM_AFFIX_W_SAVAGE,
            ITEM_AFFIX_OF_GIANTS,
            ITEM_AFFIX_PENALTY_DEX_2,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        // Extra: additional +5 base damage baked in
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_NORMAL, 5,
        -1, -1, 0,
    },

    // UNIQUE_VEIL_OF_SHADOWS
    // Rogue armor: massive stealth + dexterity at cost of strength.
    // Affixes: Shadow x2 (stacking), of the Wind, Penalty:STR-2
    {
        "Veil of Shadows",
        OBJ_TYPE_ARMOR,
        0, // drop_critter_bp: set to e.g. BP_SHADOW_WARRIOR_NPC
        {
            ITEM_AFFIX_A_SHADOW,
            ITEM_AFFIX_A_SHADOW,
            ITEM_AFFIX_OF_THE_WIND,
            ITEM_AFFIX_PENALTY_STR_2,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        -1, -1, 0,
        // Extra: +5 more AC for the unique version
        OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 5,
    },

    // UNIQUE_IRONCLAD_COGPLATE
    // Tank armor: enormous AC + constitution, but very slow.
    // Affixes: Fortified x2, of Endurance, Penalty:Speed-2
    {
        "Ironclad Cogplate",
        OBJ_TYPE_ARMOR,
        0, // drop_critter_bp: set to e.g. BP_CURSED_PALADIN_NPC
        {
            ITEM_AFFIX_A_FORTIFIED,
            ITEM_AFFIX_A_FORTIFIED,
            ITEM_AFFIX_OF_ENDURANCE,
            ITEM_AFFIX_PENALTY_SPEED_2,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        -1, -1, 0,
        // Extra: +10 more AC
        OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 10,
    },

    // UNIQUE_RING_OF_MEPHISTIS
    // Necromancer focus weapon: willpower at cost of beauty and charisma.
    // The weapon's ITEM_SPELL fields should be set to SPELL_HARM via editor/script.
    // Affixes: of Will, Penalty:BEA-1, Penalty:CHA-1
    {
        "Ring of Mephistis",
        OBJ_TYPE_WEAPON,
        0, // drop_critter_bp: set to e.g. BP_BLOOD_SPIRIT_NPC
        {
            ITEM_AFFIX_W_KEEN,
            ITEM_AFFIX_OF_WILL,
            ITEM_AFFIX_PENALTY_BEA_1,
            ITEM_AFFIX_PENALTY_CHA_1,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        OBJ_F_WEAPON_MAGIC_CRIT_HIT_CHANCE, -1, 5,
        -1, -1, 0,
    },

    // UNIQUE_TULLIAN_FOCUS
    // Force mage staff: converts brawn into brains (STR penalty, INT+WIL bonus).
    // Affixes: of the Sage x2, of Will, Penalty:STR-2
    {
        "Tullian Focus",
        OBJ_TYPE_WEAPON,
        0, // drop_critter_bp: set to e.g. BP_BOLT_SLAYER
        {
            ITEM_AFFIX_OF_THE_SAGE,
            ITEM_AFFIX_OF_THE_SAGE,
            ITEM_AFFIX_OF_WILL,
            ITEM_AFFIX_PENALTY_STR_2,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        // Extra: +10 to-hit for a magic-user weapon
        OBJ_F_WEAPON_MAGIC_HIT_ADJ, -1, 10,
        -1, -1, 0,
    },

    // UNIQUE_WYRMFANG
    // Fire+poison hybrid: burning venom, high crit, minor to-hit penalty
    {
        "Wyrmfang",
        OBJ_TYPE_WEAPON,
        0,
        {
            ITEM_AFFIX_W_BLAZING,
            ITEM_AFFIX_W_VENOMOUS,
            ITEM_AFFIX_W_RAZOR,
            ITEM_AFFIX_PENALTY_DEX_1,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        // Extra: +3 fire damage on top of W_BLAZING
        OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, DAMAGE_TYPE_FIRE, 3,
        -1, -1, 0,
    },

    // UNIQUE_COGSWORTH_REPEATER
    // Mechanically tuned to fire impossibly fast; trades raw damage for speed
    {
        "Cogsworth Repeater",
        OBJ_TYPE_WEAPON,
        0,
        {
            ITEM_AFFIX_W_SWIFT,
            ITEM_AFFIX_W_FLEET,
            ITEM_AFFIX_W_BALANCED,
            ITEM_AFFIX_PENALTY_STR_1,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        // Extra: another -1 speed for extreme attack rate
        OBJ_F_WEAPON_MAGIC_SPEED_ADJ, -1, -1,
        -1, -1, 0,
    },

    // UNIQUE_STONEHIDE_MANTLE
    // Armour carved from a living boulder; immovable but indestructible
    {
        "Stonehide Mantle",
        OBJ_TYPE_ARMOR,
        0,
        {
            ITEM_AFFIX_A_FORTIFIED,
            ITEM_AFFIX_A_REINFORCED,
            ITEM_AFFIX_OF_THE_OX,
            ITEM_AFFIX_PENALTY_SPEED_2,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        -1, -1, 0,
        // Extra: +12 AC on top of Fortified+Reinforced
        OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 12,
    },

    // UNIQUE_GALATEA_MIRROR
    // Polished to supernatural clarity; spells skitter off its surface
    {
        "Galatea Mirror",
        OBJ_TYPE_ARMOR,
        0,
        {
            ITEM_AFFIX_A_SANCTIFIED,
            ITEM_AFFIX_A_SPELLWARD,
            ITEM_AFFIX_OF_THE_ORACLE,
            ITEM_AFFIX_PENALTY_STR_1,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        -1, -1, 0,
        // Extra: +20% magic resistance
        OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_MAGIC, 20,
    },

    // UNIQUE_THORNWEAVE
    // Woven from shadow-thistle; poisons cannot touch the wearer
    {
        "Thornweave",
        OBJ_TYPE_ARMOR,
        0,
        {
            ITEM_AFFIX_A_VENOMPROOF,
            ITEM_AFFIX_A_PHANTOM,
            ITEM_AFFIX_OF_THE_WIND,
            ITEM_AFFIX_PENALTY_CON_1,
            ITEM_AFFIX_NONE,
            ITEM_AFFIX_NONE,
        },
        -1, -1, 0,
        // Extra: +6 more stealth
        OBJ_F_ARMOR_MAGIC_SILENT_MOVE_ADJ, -1, 6,
    },
};

// ---------------------------------------------------------------------------
// Boss drop
// ---------------------------------------------------------------------------

void item_rarity_try_boss_drop(int64_t critter_obj)
{
    int critter_bp;
    int u;
    int cnt;
    int idx;
    int64_t item_obj;
    int item_type;

    critter_bp = obj_field_int32_get(critter_obj, OBJ_F_DESCRIPTION);
    if (critter_bp == 0) {
        return;
    }

    for (u = 1; u < UNIQUE_ITEM_COUNT; u++) {
        if (unique_table[u].drop_critter_bp != critter_bp) {
            continue;
        }
        // Found a matching unique. Find first item in inventory matching obj_type.
        cnt = obj_field_int32_get(critter_obj, OBJ_F_CRITTER_INVENTORY_NUM);
        for (idx = 0; idx < cnt; idx++) {
            item_obj = obj_arrayfield_handle_get(critter_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, idx);
            if (item_obj == OBJ_HANDLE_NULL) {
                continue;
            }
            item_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
            if (item_type == unique_table[u].obj_type) {
                item_rarity_apply_unique(item_obj, (UniqueItemId)u);
                break;
            }
        }
        // Only one guaranteed unique per critter (first match wins).
        break;
    }
}

// ---------------------------------------------------------------------------
// Storage helpers
// ---------------------------------------------------------------------------

ItemRarity item_rarity_get(int64_t item_obj)
{
    int v = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    return (ItemRarity)(v & 0xFF);
}

void item_rarity_set(int64_t item_obj, ItemRarity rarity)
{
    int v = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    v = (v & ((int)ITEM_RARITY_IDENTIFIED_BIT | (int)ITEM_RARITY_CURSED_BOUND_BIT)) | (int)rarity;
    obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, v);
}

bool item_rarity_is_identified(int64_t item_obj)
{
    ItemRarity rarity = item_rarity_get(item_obj);
    if (rarity <= ITEM_RARITY_COMMON) {
        return true;
    }
    int v = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    if ((v & (int)ITEM_RARITY_IDENTIFIED_BIT) != 0) {
        return true;
    }
    return (obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_IDENTIFIED) != 0;
}

bool item_rarity_is_cursed_bound(int64_t item_obj)
{
    int v = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    return (v & (int)ITEM_RARITY_CURSED_BOUND_BIT) != 0;
}

void item_rarity_identify(int64_t item_obj)
{
    int v = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    v |= (int)ITEM_RARITY_IDENTIFIED_BIT;
    obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, v);
}

int item_affix_get(int64_t item_obj, int slot)
{
    return (int)obj_arrayfield_uint32_get(item_obj, OBJ_F_ITEM_PAD_IAS_1, slot);
}

void item_affix_set(int64_t item_obj, int slot, int affix)
{
    obj_arrayfield_uint32_set(item_obj, OBJ_F_ITEM_PAD_IAS_1, slot, (unsigned int)affix);
}

// ---------------------------------------------------------------------------
// Affix baking (writes to MAGIC_* fields)
// ---------------------------------------------------------------------------

static void bake_affix(int64_t item_obj, int obj_type, const AffixDef* def)
{
    int field, field_idx, value;

    if (obj_type == OBJ_TYPE_WEAPON) {
        field = def->weapon_field;
        field_idx = def->weapon_field_idx;
        value = def->weapon_value;
    } else {
        field = def->armor_field;
        field_idx = def->armor_field_idx;
        value = def->armor_value;
    }

    if (field == -1 || value == 0) {
        return;
    }

    if (field_idx == -1) {
        int current = obj_field_int32_get(item_obj, field);
        obj_field_int32_set(item_obj, field, current + value);
    } else {
        int current = obj_arrayfield_int32_get(item_obj, field, field_idx);
        obj_arrayfield_int32_set(item_obj, field, field_idx, current + value);
    }
}

static void unbake_affix(int64_t item_obj, int obj_type, const AffixDef* def)
{
    int field, field_idx, value;

    if (obj_type == OBJ_TYPE_WEAPON) {
        field = def->weapon_field;
        field_idx = def->weapon_field_idx;
        value = def->weapon_value;
    } else {
        field = def->armor_field;
        field_idx = def->armor_field_idx;
        value = def->armor_value;
    }

    if (field == -1 || value == 0) {
        return;
    }

    if (field_idx == -1) {
        int current = obj_field_int32_get(item_obj, field);
        obj_field_int32_set(item_obj, field, current - value);
    } else {
        int current = obj_arrayfield_int32_get(item_obj, field, field_idx);
        obj_arrayfield_int32_set(item_obj, field, field_idx, current - value);
    }
}

static void bake_extra(int64_t item_obj, int field, int field_idx, int value)
{
    if (field == -1 || value == 0) {
        return;
    }
    if (field_idx == -1) {
        int current = obj_field_int32_get(item_obj, field);
        obj_field_int32_set(item_obj, field, current + value);
    } else {
        int current = obj_arrayfield_int32_get(item_obj, field, field_idx);
        obj_arrayfield_int32_set(item_obj, field, field_idx, current + value);
    }
}

// ---------------------------------------------------------------------------
// Rarity roll
// ---------------------------------------------------------------------------

// NG+ rarity thresholds (cumulative, out of 100):
// [level] { CURSED, UNIQUE, EPIC, RARE, UNCOMMON }
static const int ng_plus_item_thresholds[4][5] = {
    {  1,  3,  8, 20, 45 },  // NG+0 (vanilla)
    {  2,  5, 13, 30, 55 },  // NG+1
    {  3,  8, 18, 38, 60 },  // NG+2
    {  4, 12, 24, 45, 65 },  // NG+3
};

static ItemRarity roll_rarity(void)
{
    int r = random_between(1, 100);
    const int* t = ng_plus_item_thresholds[ng_plus_get_level()];
    if (r <= t[0]) return ITEM_RARITY_CURSED;
    if (r <= t[1]) return ITEM_RARITY_UNIQUE;
    if (r <= t[2]) return ITEM_RARITY_EPIC;
    if (r <= t[3]) return ITEM_RARITY_RARE;
    if (r <= t[4]) return ITEM_RARITY_UNCOMMON;
    return ITEM_RARITY_COMMON;
}

// Pick a random affix from pool, avoiding duplicates already chosen.
static int pick_affix(const int* pool, int pool_size, const int* chosen, int chosen_cnt)
{
    int tries;
    int candidate;
    int i;
    bool duplicate;

    for (tries = 0; tries < 20; tries++) {
        candidate = pool[random_between(0, pool_size - 1)];
        duplicate = false;
        for (i = 0; i < chosen_cnt; i++) {
            if (chosen[i] == candidate) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            return candidate;
        }
    }
    // Fallback: return first pool entry.
    return pool[0];
}

void item_rarity_roll(int64_t item_obj)
{
    int obj_type;
    ItemRarity rarity;
    int prefix_count;
    int suffix_count;
    int affix_slot;
    int affix_id;
    int chosen[ITEM_RARITY_MAX_AFFIXES];
    int chosen_cnt;
    const int* prefix_pool;
    int prefix_pool_size;

    if (item_rarity_get(item_obj) != ITEM_RARITY_NONE) {
        return; // already rolled
    }

    obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    if (obj_type != OBJ_TYPE_WEAPON && obj_type != OBJ_TYPE_ARMOR) {
        item_rarity_set(item_obj, ITEM_RARITY_COMMON);
        return;
    }

    // SET: ~0.5% chance (1-in-200) before regular roll.
    if (random_between(1, 200) == 1) {
        int set_idx = random_between(1, SET_COUNT - 1);
        item_rarity_set(item_obj, ITEM_RARITY_SET);
        item_set_set(item_obj, (SetId)set_idx);
        item_rarity_identify(item_obj);
        if (obj_type == OBJ_TYPE_WEAPON) {
            bake_extra(item_obj, OBJ_F_WEAPON_MAGIC_HIT_ADJ, -1, 5);
        } else {
            bake_extra(item_obj, OBJ_F_ARMOR_MAGIC_AC_ADJ, -1, 5);
        }
        return;
    }

    rarity = roll_rarity();
    item_rarity_set(item_obj, rarity);

    if (rarity == ITEM_RARITY_COMMON) {
        return;
    }

    if (rarity == ITEM_RARITY_UNIQUE) {
        // Pick a random unique definition matching the item type.
        int candidates[UNIQUE_ITEM_COUNT];
        int candidate_cnt = 0;
        for (int u = 1; u < UNIQUE_ITEM_COUNT; u++) {
            if (unique_table[u].obj_type == obj_type) {
                candidates[candidate_cnt++] = u;
            }
        }
        if (candidate_cnt == 0) {
            // No unique for this type — downgrade to epic.
            item_rarity_set(item_obj, ITEM_RARITY_EPIC);
            rarity = ITEM_RARITY_EPIC;
            goto roll_epic;
        }

        int uid = candidates[random_between(0, candidate_cnt - 1)];
        const UniqueItemDef* def = &unique_table[uid];

        for (int s = 0; s < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
            int a = def->affixes[s];
            item_affix_set(item_obj, s, a);
            if (a != ITEM_AFFIX_NONE) {
                bake_affix(item_obj, obj_type, &affix_table[a]);
            }
        }
        // Store unique ID in last slot.
        item_affix_set(item_obj, UNIQUE_ID_SLOT, uid);

        // Bake extra unique bonus.
        if (obj_type == OBJ_TYPE_WEAPON) {
            bake_extra(item_obj,
                def->extra_weapon_field, def->extra_weapon_field_idx, def->extra_weapon_value);
        } else {
            bake_extra(item_obj,
                def->extra_armor_field, def->extra_armor_field_idx, def->extra_armor_value);
        }
        return;
    }

    if (rarity == ITEM_RARITY_CURSED) {
        const int* cprefix_pool = (obj_type == OBJ_TYPE_WEAPON)
            ? cursed_weapon_prefix_pool : cursed_armor_prefix_pool;
        int cprefix_pool_size = (obj_type == OBJ_TYPE_WEAPON)
            ? cursed_weapon_prefix_pool_size : cursed_armor_prefix_pool_size;

        int chosen_c[ITEM_RARITY_MAX_AFFIXES];
        int chosen_c_cnt = 0;
        affix_slot = 0;

        // Cursed prefix (mandatory)
        affix_id = pick_affix(cprefix_pool, cprefix_pool_size, chosen_c, chosen_c_cnt);
        chosen_c[chosen_c_cnt++] = affix_id;
        item_affix_set(item_obj, affix_slot++, affix_id);
        bake_affix(item_obj, obj_type, &affix_table[affix_id]);

        // Cursed suffix penalty (mandatory)
        affix_id = pick_affix(cursed_suffix_pool, cursed_suffix_pool_size, chosen_c, chosen_c_cnt);
        chosen_c[chosen_c_cnt++] = affix_id;
        item_affix_set(item_obj, affix_slot++, affix_id);

        // 40% chance: extra regular prefix (tempting extra power)
        if (random_between(1, 100) <= 40) {
            const int* rprefix_pool = (obj_type == OBJ_TYPE_WEAPON)
                ? weapon_prefix_pool : armor_prefix_pool;
            int rprefix_pool_size = (obj_type == OBJ_TYPE_WEAPON)
                ? weapon_prefix_pool_size : armor_prefix_pool_size;
            affix_id = pick_affix(rprefix_pool, rprefix_pool_size, chosen_c, chosen_c_cnt);
            chosen_c[chosen_c_cnt++] = affix_id;
            item_affix_set(item_obj, affix_slot++, affix_id);
            bake_affix(item_obj, obj_type, &affix_table[affix_id]);
        }

        // 40% chance: cursed-bound (cannot unequip without Remove Curse)
        if (random_between(1, 100) <= 40) {
            int32_t pad = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
            obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, pad | (int32_t)ITEM_RARITY_CURSED_BOUND_BIT);
        }
        return;
    }

roll_epic:;
    // Determine prefix/suffix counts per rarity.
    switch (rarity) {
    case ITEM_RARITY_UNCOMMON:
        prefix_count = 1;
        suffix_count = 0;
        if (random_between(0, 1) == 1) {
            prefix_count = 0;
            suffix_count = 1;
        }
        break;
    case ITEM_RARITY_RARE:
        prefix_count = random_between(1, 2);
        suffix_count = 1;
        break;
    case ITEM_RARITY_EPIC:
    default:
        prefix_count = random_between(2, 3);
        suffix_count = 2;
        break;
    }

    prefix_pool = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool : armor_prefix_pool;
    prefix_pool_size = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool_size : armor_prefix_pool_size;

    chosen_cnt = 0;
    affix_slot = 0;

    for (int p = 0; p < prefix_count && affix_slot < ITEM_RARITY_MAX_AFFIXES - 1; p++) {
        affix_id = pick_affix(prefix_pool, prefix_pool_size, chosen, chosen_cnt);
        chosen[chosen_cnt++] = affix_id;
        item_affix_set(item_obj, affix_slot++, affix_id);
        bake_affix(item_obj, obj_type, &affix_table[affix_id]);
    }

    for (int s = 0; s < suffix_count && affix_slot < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
        const int* spool = suffix_pool;
        int spool_size = suffix_pool_size;
        // Last suffix on EPIC: 30% chance to draw from the rare pool instead.
        if (rarity == ITEM_RARITY_EPIC && s == suffix_count - 1 && random_between(1, 100) <= 30) {
            spool = rare_suffix_pool;
            spool_size = rare_suffix_pool_size;
        }
        affix_id = pick_affix(spool, spool_size, chosen, chosen_cnt);
        chosen[chosen_cnt++] = affix_id;
        item_affix_set(item_obj, affix_slot++, affix_id);
        // Suffix affixes don't bake (stat bonuses are applied via item_rarity_adjust_stat).
    }
}

// ---------------------------------------------------------------------------
// Force-apply unique (for scripted item placement)
// ---------------------------------------------------------------------------

void item_rarity_roll_forced(int64_t item_obj, ItemRarity forced_rarity)
{
    int obj_type;
    int prefix_count;
    int suffix_count;
    int affix_slot;
    int affix_id;
    int chosen[ITEM_RARITY_MAX_AFFIXES];
    int chosen_cnt;
    const int* prefix_pool;
    int prefix_pool_size;

    if (item_rarity_get(item_obj) != ITEM_RARITY_NONE) {
        return;
    }

    obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    if (obj_type != OBJ_TYPE_WEAPON && obj_type != OBJ_TYPE_ARMOR) {
        item_rarity_set(item_obj, ITEM_RARITY_COMMON);
        return;
    }

    item_rarity_set(item_obj, forced_rarity);

    if (forced_rarity <= ITEM_RARITY_COMMON) {
        return;
    }

    if (forced_rarity == ITEM_RARITY_UNIQUE) {
        int candidates[UNIQUE_ITEM_COUNT];
        int candidate_cnt = 0;
        for (int u = 1; u < UNIQUE_ITEM_COUNT; u++) {
            if (unique_table[u].obj_type == obj_type) {
                candidates[candidate_cnt++] = u;
            }
        }
        if (candidate_cnt == 0) {
            item_rarity_set(item_obj, ITEM_RARITY_EPIC);
            forced_rarity = ITEM_RARITY_EPIC;
            goto forced_roll_affixes;
        }
        int uid = candidates[random_between(0, candidate_cnt - 1)];
        const UniqueItemDef* def = &unique_table[uid];
        for (int s = 0; s < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
            int a = def->affixes[s];
            item_affix_set(item_obj, s, a);
            if (a != ITEM_AFFIX_NONE) {
                bake_affix(item_obj, obj_type, &affix_table[a]);
            }
        }
        item_affix_set(item_obj, UNIQUE_ID_SLOT, uid);
        if (obj_type == OBJ_TYPE_WEAPON) {
            bake_extra(item_obj,
                def->extra_weapon_field, def->extra_weapon_field_idx, def->extra_weapon_value);
        } else {
            bake_extra(item_obj,
                def->extra_armor_field, def->extra_armor_field_idx, def->extra_armor_value);
        }
        return;
    }

    if (forced_rarity == ITEM_RARITY_CURSED) {
        const int* cprefix_pool = (obj_type == OBJ_TYPE_WEAPON)
            ? cursed_weapon_prefix_pool : cursed_armor_prefix_pool;
        int cprefix_pool_size = (obj_type == OBJ_TYPE_WEAPON)
            ? cursed_weapon_prefix_pool_size : cursed_armor_prefix_pool_size;

        int chosen_c[ITEM_RARITY_MAX_AFFIXES];
        int chosen_c_cnt = 0;
        int affix_slot_c = 0;
        int affix_id_c;

        affix_id_c = pick_affix(cprefix_pool, cprefix_pool_size, chosen_c, chosen_c_cnt);
        chosen_c[chosen_c_cnt++] = affix_id_c;
        item_affix_set(item_obj, affix_slot_c++, affix_id_c);
        bake_affix(item_obj, obj_type, &affix_table[affix_id_c]);

        affix_id_c = pick_affix(cursed_suffix_pool, cursed_suffix_pool_size, chosen_c, chosen_c_cnt);
        chosen_c[chosen_c_cnt++] = affix_id_c;
        item_affix_set(item_obj, affix_slot_c++, affix_id_c);

        if (random_between(1, 100) <= 40) {
            const int* rprefix_pool = (obj_type == OBJ_TYPE_WEAPON)
                ? weapon_prefix_pool : armor_prefix_pool;
            int rprefix_pool_size = (obj_type == OBJ_TYPE_WEAPON)
                ? weapon_prefix_pool_size : armor_prefix_pool_size;
            affix_id_c = pick_affix(rprefix_pool, rprefix_pool_size, chosen_c, chosen_c_cnt);
            chosen_c[chosen_c_cnt++] = affix_id_c;
            item_affix_set(item_obj, affix_slot_c++, affix_id_c);
            bake_affix(item_obj, obj_type, &affix_table[affix_id_c]);
        }

        if (random_between(1, 100) <= 40) {
            int32_t pad = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
            obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, pad | (int32_t)ITEM_RARITY_CURSED_BOUND_BIT);
        }
        return;
    }

forced_roll_affixes:;
    switch (forced_rarity) {
    case ITEM_RARITY_UNCOMMON:
        prefix_count = 1;
        suffix_count = 0;
        if (random_between(0, 1) == 1) {
            prefix_count = 0;
            suffix_count = 1;
        }
        break;
    case ITEM_RARITY_RARE:
        prefix_count = random_between(1, 2);
        suffix_count = 1;
        break;
    case ITEM_RARITY_EPIC:
    default:
        prefix_count = random_between(2, 3);
        suffix_count = 2;
        break;
    }

    prefix_pool = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool : armor_prefix_pool;
    prefix_pool_size = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool_size : armor_prefix_pool_size;

    chosen_cnt = 0;
    affix_slot = 0;

    for (int p = 0; p < prefix_count && affix_slot < ITEM_RARITY_MAX_AFFIXES - 1; p++) {
        affix_id = pick_affix(prefix_pool, prefix_pool_size, chosen, chosen_cnt);
        chosen[chosen_cnt++] = affix_id;
        item_affix_set(item_obj, affix_slot++, affix_id);
        bake_affix(item_obj, obj_type, &affix_table[affix_id]);
    }

    for (int s = 0; s < suffix_count && affix_slot < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
        const int* spool = suffix_pool;
        int spool_size = suffix_pool_size;
        if (forced_rarity == ITEM_RARITY_EPIC && s == suffix_count - 1 && random_between(1, 100) <= 30) {
            spool = rare_suffix_pool;
            spool_size = rare_suffix_pool_size;
        }
        affix_id = pick_affix(spool, spool_size, chosen, chosen_cnt);
        chosen[chosen_cnt++] = affix_id;
        item_affix_set(item_obj, affix_slot++, affix_id);
    }
}

void item_rarity_apply_unique(int64_t item_obj, UniqueItemId uid)
{
    int obj_type;
    const UniqueItemDef* def;

    if (uid <= UNIQUE_ITEM_NONE || uid >= UNIQUE_ITEM_COUNT) {
        return;
    }
    if (item_rarity_get(item_obj) != ITEM_RARITY_NONE) {
        return;
    }

    def = &unique_table[uid];
    obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);

    item_rarity_set(item_obj, ITEM_RARITY_UNIQUE);

    for (int s = 0; s < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
        int a = def->affixes[s];
        item_affix_set(item_obj, s, a);
        if (a != ITEM_AFFIX_NONE) {
            bake_affix(item_obj, obj_type, &affix_table[a]);
        }
    }
    item_affix_set(item_obj, UNIQUE_ID_SLOT, (int)uid);

    if (obj_type == OBJ_TYPE_WEAPON) {
        bake_extra(item_obj,
            def->extra_weapon_field, def->extra_weapon_field_idx, def->extra_weapon_value);
    } else {
        bake_extra(item_obj,
            def->extra_armor_field, def->extra_armor_field_idx, def->extra_armor_value);
    }
}

// ---------------------------------------------------------------------------
// Name generation
// ---------------------------------------------------------------------------

void item_rarity_generate_name(int64_t item_obj, const char* base_name,
    char* out_buf, int buf_size)
{
    ItemRarity rarity;
    const char* prefix;
    const char* suffix;
    int affix_id;
    int unique_id;

    rarity = item_rarity_get(item_obj);

    if (rarity <= ITEM_RARITY_COMMON || rarity >= ITEM_RARITY_COUNT) {
        snprintf(out_buf, buf_size, "%s", base_name);
        return;
    }

    if (rarity == ITEM_RARITY_UNIQUE) {
        unique_id = item_affix_get(item_obj, UNIQUE_ID_SLOT);
        if (unique_id > 0 && unique_id < UNIQUE_ITEM_COUNT && unique_table[unique_id].name != NULL) {
            snprintf(out_buf, buf_size, "%s", unique_table[unique_id].name);
        } else {
            snprintf(out_buf, buf_size, "%s", base_name);
        }
        return;
    }

    // Find first prefix and first suffix from affix slots.
    prefix = NULL;
    suffix = NULL;
    for (int slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
        affix_id = item_affix_get(item_obj, slot);
        if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
            continue;
        }
        if (prefix == NULL && affix_table[affix_id].prefix != NULL) {
            prefix = affix_table[affix_id].prefix;
        }
        if (suffix == NULL && affix_table[affix_id].suffix != NULL) {
            suffix = affix_table[affix_id].suffix;
        }
    }

    if (prefix != NULL && suffix != NULL) {
        snprintf(out_buf, buf_size, "%s %s %s", prefix, base_name, suffix);
    } else if (prefix != NULL) {
        snprintf(out_buf, buf_size, "%s %s", prefix, base_name);
    } else if (suffix != NULL) {
        snprintf(out_buf, buf_size, "%s %s", base_name, suffix);
    } else {
        snprintf(out_buf, buf_size, "%s", base_name);
    }
}

// ---------------------------------------------------------------------------
// Affix description (for examine window)
// ---------------------------------------------------------------------------

void item_rarity_describe_affixes(int64_t item_obj, char* buf, int buf_size)
{
    ItemRarity rarity;
    int affix_id;
    int pos;
    const AffixDef* def;
    const char* stat_names[] = {
        "STR", "DEX", "CON", "BEA", "INT", "PER", "WIL", "CHA"
    };

    rarity = item_rarity_get(item_obj);
    if (rarity <= ITEM_RARITY_COMMON) {
        return;
    }

    static const char* rarity_names[] = {
        "", "Common", "Uncommon", "Rare", "Epic", "Unique", "Cursed", "Set"
    };
    pos = (int)strlen(buf);
    pos += snprintf(buf + pos, buf_size - pos, "\n[%s]\n",
        (rarity < ITEM_RARITY_COUNT) ? rarity_names[rarity] : "?");

    for (int slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
        affix_id = item_affix_get(item_obj, slot);
        if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
            continue;
        }
        // Skip UNIQUE_ID_SLOT.
        if (rarity == ITEM_RARITY_UNIQUE && slot == UNIQUE_ID_SLOT) {
            continue;
        }

        def = &affix_table[affix_id];

        if (def->desc != NULL) {
            pos += snprintf(buf + pos, buf_size - pos, "  %s\n", def->desc);
        } else if (def->equip_stat >= 0 && def->equip_stat < (int)(sizeof(stat_names) / sizeof(stat_names[0]))) {
            pos += snprintf(buf + pos, buf_size - pos,
                "  %+d %s (equipped)\n", def->equip_stat_val, stat_names[def->equip_stat]);
        }

        if (pos >= buf_size - 1) {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Stat adjustment for equipped items
// ---------------------------------------------------------------------------

int item_rarity_adjust_stat(int64_t critter_obj, int stat, int value)
{
    static const int wear_slots[] = {
        ITEM_INV_LOC_HELMET,
        ITEM_INV_LOC_RING1,
        ITEM_INV_LOC_RING2,
        ITEM_INV_LOC_MEDALLION,
        ITEM_INV_LOC_WEAPON,
        ITEM_INV_LOC_SHIELD,
        ITEM_INV_LOC_ARMOR,
        ITEM_INV_LOC_GAUNTLET,
        ITEM_INV_LOC_BOOTS,
    };
    int num_slots = (int)(sizeof(wear_slots) / sizeof(wear_slots[0]));

    for (int s = 0; s < num_slots; s++) {
        int64_t item_obj = item_wield_get(critter_obj, wear_slots[s]);
        if (item_obj == OBJ_HANDLE_NULL) {
            continue;
        }

        ItemRarity rarity = item_rarity_get(item_obj);
        if (rarity <= ITEM_RARITY_COMMON) {
            continue;
        }

        for (int slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
            int affix_id = item_affix_get(item_obj, slot);
            if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
                continue;
            }
            // Skip unique ID slot.
            if (rarity == ITEM_RARITY_UNIQUE && slot == UNIQUE_ID_SLOT) {
                continue;
            }
            const AffixDef* def = &affix_table[affix_id];
            if (def->equip_stat == stat) {
                value += def->equip_stat_val;
            }
        }
    }

    return value;
}

int item_rarity_life_on_hit_get(int64_t critter_obj)
{
    static const int wear_slots[] = {
        ITEM_INV_LOC_HELMET,
        ITEM_INV_LOC_RING1,
        ITEM_INV_LOC_RING2,
        ITEM_INV_LOC_MEDALLION,
        ITEM_INV_LOC_WEAPON,
        ITEM_INV_LOC_SHIELD,
        ITEM_INV_LOC_ARMOR,
        ITEM_INV_LOC_GAUNTLET,
        ITEM_INV_LOC_BOOTS,
    };
    int num_slots = (int)(sizeof(wear_slots) / sizeof(wear_slots[0]));
    int loh = 0;

    for (int s = 0; s < num_slots; s++) {
        int64_t item_obj = item_wield_get(critter_obj, wear_slots[s]);
        if (item_obj == OBJ_HANDLE_NULL) {
            continue;
        }
        ItemRarity rarity = item_rarity_get(item_obj);
        if (rarity <= ITEM_RARITY_COMMON) {
            continue;
        }
        for (int slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
            if (rarity == ITEM_RARITY_UNIQUE && slot == UNIQUE_ID_SLOT) {
                continue;
            }
            int affix_id = item_affix_get(item_obj, slot);
            if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
                continue;
            }
            loh += affix_table[affix_id].equip_loh;
        }
    }

    return loh;
}

int item_rarity_thorns_get(int64_t critter_obj)
{
    static const int wear_slots[] = {
        ITEM_INV_LOC_HELMET,
        ITEM_INV_LOC_RING1,
        ITEM_INV_LOC_RING2,
        ITEM_INV_LOC_MEDALLION,
        ITEM_INV_LOC_WEAPON,
        ITEM_INV_LOC_SHIELD,
        ITEM_INV_LOC_ARMOR,
        ITEM_INV_LOC_GAUNTLET,
        ITEM_INV_LOC_BOOTS,
    };
    int num_slots = (int)(sizeof(wear_slots) / sizeof(wear_slots[0]));
    int thorns = 0;

    for (int s = 0; s < num_slots; s++) {
        int64_t item_obj = item_wield_get(critter_obj, wear_slots[s]);
        if (item_obj == OBJ_HANDLE_NULL) {
            continue;
        }
        ItemRarity rarity = item_rarity_get(item_obj);
        if (rarity <= ITEM_RARITY_COMMON) {
            continue;
        }
        for (int slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
            if (rarity == ITEM_RARITY_UNIQUE && slot == UNIQUE_ID_SLOT) {
                continue;
            }
            int affix_id = item_affix_get(item_obj, slot);
            if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
                continue;
            }
            thorns += affix_table[affix_id].equip_thorns;
        }
    }

    return thorns;
}

// ---------------------------------------------------------------------------
// Tooltip helpers
// ---------------------------------------------------------------------------

void item_rarity_format_equipped_stats(int64_t item_obj, char* buf, int buf_size)
{
    static const char* stat_names[] = { "STR", "DEX", "CON", "BEA", "INT", "PER", "WIL", "CHA" };
    ItemRarity rarity = item_rarity_get(item_obj);
    bool first = true;
    int pos = 0;
    buf[0] = '\0';

    for (int slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
        if (rarity == ITEM_RARITY_UNIQUE && slot == UNIQUE_ID_SLOT) {
            continue;
        }
        int affix_id = item_affix_get(item_obj, slot);
        if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
            continue;
        }
        const AffixDef* def = &affix_table[affix_id];
        if (def->equip_stat >= 0 && def->equip_stat < (int)(sizeof(stat_names) / sizeof(stat_names[0]))) {
            if (!first) {
                pos += snprintf(buf + pos, buf_size - pos, " | ");
            }
            pos += snprintf(buf + pos, buf_size - pos, "%+d %s",
                def->equip_stat_val, stat_names[def->equip_stat]);
            first = false;
        }
    }
    if (!first && pos < buf_size - 12) {
        snprintf(buf + pos, buf_size - pos, " (equipped)");
    }
}

// ---------------------------------------------------------------------------
// Display rarity inference (read-only, never written to object)
// ---------------------------------------------------------------------------

ItemRarity item_rarity_infer(int64_t item_obj)
{
    ItemRarity stored = item_rarity_get(item_obj);
    if (stored > ITEM_RARITY_COMMON) {
        return stored;
    }

    int complexity = obj_field_int32_get(item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
    if (complexity <= 0) {
        return ITEM_RARITY_COMMON;
    }
    if (complexity <= 25) {
        return ITEM_RARITY_UNCOMMON;
    }
    if (complexity <= 60) {
        return ITEM_RARITY_RARE;
    }
    return ITEM_RARITY_EPIC;
}

// ---------------------------------------------------------------------------
// UI color
// ---------------------------------------------------------------------------

tig_color_t item_rarity_color(ItemRarity rarity)
{
    switch (rarity) {
    case ITEM_RARITY_UNCOMMON: return tig_color_make(100, 220, 100);  // green
    case ITEM_RARITY_RARE:     return tig_color_make(100, 180, 255);  // blue
    case ITEM_RARITY_EPIC:     return tig_color_make(190, 100, 255);  // purple
    case ITEM_RARITY_UNIQUE:   return tig_color_make(255, 180,  50);  // gold
    case ITEM_RARITY_CURSED:   return tig_color_make(210,  40,  40);  // dark crimson
    case ITEM_RARITY_SET:      return tig_color_make( 80, 220, 200);  // teal
    default:                   return tig_color_make(255, 255, 255);  // white
    }
}

// ---------------------------------------------------------------------------
// Crafting operations
// ---------------------------------------------------------------------------

static void reforge_impl(int64_t item_obj, ItemRarity target_rarity)
{
    int obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    int v;
    int slot;
    int affix_id;

    for (slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
        affix_id = item_affix_get(item_obj, slot);
        if (affix_id > ITEM_AFFIX_NONE && affix_id < ITEM_AFFIX_COUNT) {
            unbake_affix(item_obj, obj_type, &affix_table[affix_id]);
        }
        item_affix_set(item_obj, slot, ITEM_AFFIX_NONE);
    }

    // Clear rarity + cursed-bound, preserve only identified bit.
    v = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    v = (v & (int)ITEM_RARITY_IDENTIFIED_BIT) | ITEM_RARITY_NONE;
    obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, v);

    item_rarity_roll_forced(item_obj, target_rarity);
    item_rarity_identify(item_obj);
}

void item_rarity_reforge(int64_t item_obj)
{
    ItemRarity rarity = item_rarity_get(item_obj);
    if (rarity <= ITEM_RARITY_COMMON || rarity == ITEM_RARITY_UNIQUE || rarity == ITEM_RARITY_SET) {
        return;
    }
    reforge_impl(item_obj, rarity);
}

bool item_rarity_ascend(int64_t item_obj)
{
    ItemRarity rarity = item_rarity_get(item_obj);
    ItemRarity new_rarity;

    switch (rarity) {
    case ITEM_RARITY_UNCOMMON: new_rarity = ITEM_RARITY_RARE; break;
    case ITEM_RARITY_RARE:     new_rarity = ITEM_RARITY_EPIC; break;
    default: return false;
    }

    reforge_impl(item_obj, new_rarity);
    return true;
}

void item_rarity_cleanse(int64_t item_obj)
{
    if (item_rarity_get(item_obj) != ITEM_RARITY_CURSED) {
        return;
    }
    reforge_impl(item_obj, ITEM_RARITY_RARE);
}

void item_rarity_annul(int64_t item_obj)
{
    int obj_type;
    int slot;
    int affix_id;

    if (item_rarity_get(item_obj) <= ITEM_RARITY_COMMON) {
        return;
    }

    obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    for (slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
        affix_id = item_affix_get(item_obj, slot);
        if (affix_id > ITEM_AFFIX_NONE && affix_id < ITEM_AFFIX_COUNT) {
            unbake_affix(item_obj, obj_type, &affix_table[affix_id]);
        }
        item_affix_set(item_obj, slot, ITEM_AFFIX_NONE);
    }
    obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, (int)ITEM_RARITY_COMMON);
}

void item_rarity_awaken(int64_t item_obj)
{
    reforge_impl(item_obj, ITEM_RARITY_UNCOMMON);
}

bool item_rarity_augment(int64_t item_obj)
{
    int obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    int free_slot = -1;
    int chosen[ITEM_RARITY_MAX_AFFIXES];
    int chosen_cnt = 0;
    int s;

    for (s = 0; s < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
        int a = item_affix_get(item_obj, s);
        if (a == ITEM_AFFIX_NONE) {
            if (free_slot == -1) {
                free_slot = s;
            }
        } else {
            chosen[chosen_cnt++] = a;
        }
    }

    if (free_slot == -1) {
        return false;
    }

    const int* pool;
    int pool_size;
    if (random_between(0, 1) == 0) {
        pool = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool : armor_prefix_pool;
        pool_size = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool_size : armor_prefix_pool_size;
    } else {
        pool = suffix_pool;
        pool_size = suffix_pool_size;
    }

    int new_affix = pick_affix(pool, pool_size, chosen, chosen_cnt);
    item_affix_set(item_obj, free_slot, new_affix);
    if (affix_table[new_affix].prefix != NULL) {
        bake_affix(item_obj, obj_type, &affix_table[new_affix]);
    }
    item_rarity_identify(item_obj);
    return true;
}

int item_rarity_corrupt(int64_t item_obj)
{
    ItemRarity rarity = item_rarity_get(item_obj);
    int obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    int roll = random_between(1, 100);

    if (roll <= 25) {
        ItemRarity new_rarity = (rarity == ITEM_RARITY_UNCOMMON) ? ITEM_RARITY_RARE : ITEM_RARITY_EPIC;
        reforge_impl(item_obj, new_rarity);
        return 0;
    } else if (roll <= 45) {
        reforge_impl(item_obj, ITEM_RARITY_CURSED);
        return 1;
    } else if (roll <= 65) {
        if (!item_rarity_augment(item_obj)) {
            return 4;
        }
        return 2;
    } else if (roll <= 85) {
        int filled[ITEM_RARITY_MAX_AFFIXES];
        int filled_cnt = 0;
        int s;
        for (s = 0; s < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
            int a = item_affix_get(item_obj, s);
            if (a != ITEM_AFFIX_NONE) {
                filled[filled_cnt++] = s;
            }
        }
        if (filled_cnt > 0) {
            int target_slot = filled[random_between(0, filled_cnt - 1)];
            int affix_id = item_affix_get(item_obj, target_slot);
            if (affix_id > ITEM_AFFIX_NONE && affix_id < ITEM_AFFIX_COUNT) {
                unbake_affix(item_obj, obj_type, &affix_table[affix_id]);
            }
            item_affix_set(item_obj, target_slot, ITEM_AFFIX_NONE);
        }
        item_rarity_identify(item_obj);
        return 3;
    } else {
        return 4;
    }
}

void item_rarity_entropy(int64_t item_obj)
{
    int obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    int slot;

    for (slot = 0; slot < ITEM_RARITY_MAX_AFFIXES - 1; slot++) {
        int affix_id = item_affix_get(item_obj, slot);
        if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
            continue;
        }

        const AffixDef* def = &affix_table[affix_id];
        const int* pool = NULL;
        int pool_size = 0;

        if (def->prefix != NULL) {
            pool = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool : armor_prefix_pool;
            pool_size = (obj_type == OBJ_TYPE_WEAPON) ? weapon_prefix_pool_size : armor_prefix_pool_size;
        } else if (def->suffix != NULL) {
            pool = suffix_pool;
            pool_size = suffix_pool_size;
        }

        if (pool == NULL || pool_size == 0) {
            continue;
        }

        if (def->prefix != NULL) {
            unbake_affix(item_obj, obj_type, def);
        }

        int chosen[ITEM_RARITY_MAX_AFFIXES];
        int chosen_cnt = 0;
        int s;
        for (s = 0; s < ITEM_RARITY_MAX_AFFIXES - 1; s++) {
            if (s != slot) {
                int a = item_affix_get(item_obj, s);
                if (a != ITEM_AFFIX_NONE) {
                    chosen[chosen_cnt++] = a;
                }
            }
        }

        int new_affix = pick_affix(pool, pool_size, chosen, chosen_cnt);
        item_affix_set(item_obj, slot, new_affix);
        if (affix_table[new_affix].prefix != NULL) {
            bake_affix(item_obj, obj_type, &affix_table[new_affix]);
        }
    }

    item_rarity_identify(item_obj);
}

// ---------------------------------------------------------------------------
// Tooltip affix list
// ---------------------------------------------------------------------------

void item_rarity_format_tooltip_affixes(int64_t item_obj, char* buf, int buf_size)
{
    static const char* stat_names[] = { "STR", "DEX", "CON", "BEA", "INT", "PER", "WIL", "CHA" };
    ItemRarity rarity = item_rarity_get(item_obj);
    int pos = 0;
    int slot;

    buf[0] = '\0';

    for (slot = 0; slot < ITEM_RARITY_MAX_AFFIXES; slot++) {
        int affix_id;
        const AffixDef* def;

        if (rarity == ITEM_RARITY_UNIQUE && slot == UNIQUE_ID_SLOT) {
            continue;
        }

        affix_id = item_affix_get(item_obj, slot);
        if (affix_id <= ITEM_AFFIX_NONE || affix_id >= ITEM_AFFIX_COUNT) {
            continue;
        }

        def = &affix_table[affix_id];

        if (def->desc != NULL) {
            pos += snprintf(buf + pos, buf_size - pos, "%s\n", def->desc);
        } else if (def->equip_stat >= 0
            && def->equip_stat < (int)(sizeof(stat_names) / sizeof(stat_names[0]))) {
            pos += snprintf(buf + pos, buf_size - pos, "%+d %s (equipped)\n",
                def->equip_stat_val, stat_names[def->equip_stat]);
        }

        if (pos >= buf_size - 1) {
            break;
        }
    }

    // Trim trailing newline.
    if (pos > 0 && buf[pos - 1] == '\n') {
        buf[pos - 1] = '\0';
    }
}
