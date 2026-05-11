#include "game/item_rarity.h"

#include <stdio.h>
#include <string.h>

#include "game/damage_type.h"
#include "game/item.h"
#include "game/obj.h"
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
};
static const int suffix_pool_size = (int)(sizeof(suffix_pool) / sizeof(suffix_pool[0]));

static const int cursed_weapon_prefix_pool[] = {
    ITEM_AFFIX_CW_FRENZIED,
    ITEM_AFFIX_CW_CORRUPTED,
    ITEM_AFFIX_CW_SOULREAVER,
    ITEM_AFFIX_CW_BLOODLUST,
};
static const int cursed_weapon_prefix_pool_size = (int)(sizeof(cursed_weapon_prefix_pool) / sizeof(cursed_weapon_prefix_pool[0]));

static const int cursed_armor_prefix_pool[] = {
    ITEM_AFFIX_CA_IRONBOUND,
    ITEM_AFFIX_CA_WRAITHFORGED,
    ITEM_AFFIX_CA_ABYSSAL,
};
static const int cursed_armor_prefix_pool_size = (int)(sizeof(cursed_armor_prefix_pool) / sizeof(cursed_armor_prefix_pool[0]));

static const int cursed_suffix_pool[] = {
    ITEM_AFFIX_CURSE_OF_FRAILTY,
    ITEM_AFFIX_CURSE_OF_WEAKNESS,
    ITEM_AFFIX_CURSE_OF_CLUMSINESS,
    ITEM_AFFIX_CURSE_OF_DIMNESS,
    ITEM_AFFIX_CURSE_OF_COWARDICE,
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
    return (v & (int)ITEM_RARITY_IDENTIFIED_BIT) != 0;
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

// Rarity probability thresholds (cumulative, out of 100):
//   CURSED   :  1%
//   UNIQUE   :  3%
//   EPIC     :  8%
//   RARE     : 20%
//   UNCOMMON : 45%
//   COMMON   : 100%
static ItemRarity roll_rarity(void)
{
    int r = random_between(1, 100);
    if (r <= 1)  return ITEM_RARITY_CURSED;
    if (r <= 3)  return ITEM_RARITY_UNIQUE;
    if (r <= 8)  return ITEM_RARITY_EPIC;
    if (r <= 20) return ITEM_RARITY_RARE;
    if (r <= 45) return ITEM_RARITY_UNCOMMON;
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
        affix_id = pick_affix(suffix_pool, suffix_pool_size, chosen, chosen_cnt);
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
        affix_id = pick_affix(suffix_pool, suffix_pool_size, chosen, chosen_cnt);
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
        "", "Common", "Uncommon", "Rare", "Epic", "Unique", "Cursed"
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
    if (stored != ITEM_RARITY_NONE) {
        return stored;
    }

    int obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    if (obj_type != OBJ_TYPE_WEAPON && obj_type != OBJ_TYPE_ARMOR) {
        return ITEM_RARITY_COMMON;
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
    default:                   return tig_color_make(255, 255, 255);  // white
    }
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
