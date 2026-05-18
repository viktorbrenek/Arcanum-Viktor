#include "game/critter_rarity.h"

#include <stdio.h>
#include <string.h>

#include "game/ng_plus.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/random.h"
#include "game/stat.h"

// PAD_I_1 layout:
//   bits 0-1   CritterRarity (0=Normal, 1=Magic, 2=Rare, 3=Unique)
//   bits 2-13  bonus bitmask (CRITTER_BONUS_* shifted left by BONUS_SHIFT)
//   bit  14    CRITTER_PAD_ROLLED_FLAG — set after any rarity roll, even NORMAL

#define RARITY_MASK  0x03
#define BONUS_SHIFT  2

static const int bonus_flag_table[] = {
    CRITTER_BONUS_DEXTEROUS,
    CRITTER_BONUS_DETERMINED,
    CRITTER_BONUS_ENRAGED,
    CRITTER_BONUS_SWIFT,
    CRITTER_BONUS_ARMORED,
    CRITTER_BONUS_REGENERATING,
    CRITTER_BONUS_BRUTISH,
    CRITTER_BONUS_WARY,
    CRITTER_BONUS_ROBUST,
    CRITTER_BONUS_CUNNING,
    CRITTER_BONUS_VAMPIRIC,
    CRITTER_BONUS_THORNED,
};
#define BONUS_COUNT ((int)(sizeof(bonus_flag_table) / sizeof(bonus_flag_table[0])))

CritterRarity critter_rarity_get(int64_t obj)
{
    return (CritterRarity)(obj_field_int32_get(obj, OBJ_F_CRITTER_PAD_I_1) & RARITY_MASK);
}

int critter_rarity_bonus_get(int64_t obj)
{
    return (obj_field_int32_get(obj, OBJ_F_CRITTER_PAD_I_1) >> BONUS_SHIFT) & 0xFFF;
}

tig_color_t critter_rarity_color(CritterRarity rarity)
{
    switch (rarity) {
    case CRITTER_RARITY_MAGIC:  return tig_color_make(100, 180, 255); // blue
    case CRITTER_RARITY_RARE:   return tig_color_make(190, 100, 255); // purple
    case CRITTER_RARITY_UNIQUE: return tig_color_make(255, 180,  50); // gold
    default:                    return tig_color_make(255, 255, 255); // white
    }
}

static const char* bonus_names[] = {
    "Dexterous",
    "Determined",
    "Enraged",
    "Swift",
    "Armored",
    "Regenerating",
    "Brutish",
    "Wary",
    "Robust",
    "Cunning",
    "Vampiric",
    "Thorned",
};

void critter_rarity_generate_name(int64_t obj, const char* base_name,
    char* out_buf, int buf_size)
{
    int bonuses = critter_rarity_bonus_get(obj);
    char prefix[256];
    int prefix_len = 0;
    prefix[0] = '\0';

    for (int i = 0; i < BONUS_COUNT; i++) {
        if (bonuses & bonus_flag_table[i]) {
            if (prefix_len > 0) {
                strncat(prefix, ", ", sizeof(prefix) - prefix_len - 1);
                prefix_len += 2;
            }
            strncat(prefix, bonus_names[i], sizeof(prefix) - prefix_len - 1);
            prefix_len += (int)strlen(bonus_names[i]);
        }
    }

    if (prefix_len > 0) {
        snprintf(out_buf, buf_size, "%s %s", prefix, base_name);
    } else {
        snprintf(out_buf, buf_size, "%s", base_name);
    }
}

static int pick_bonuses(int count)
{
    int pool[BONUS_COUNT];
    for (int i = 0; i < BONUS_COUNT; i++) pool[i] = i;

    int bonus = 0;
    int remaining = BONUS_COUNT;
    for (int picked = 0; picked < count && remaining > 0; picked++) {
        int idx = random_between(0, remaining - 1);
        bonus |= bonus_flag_table[pool[idx]];
        pool[idx] = pool[--remaining];
    }
    return bonus;
}

static void apply_bonus(int64_t obj, int bonus_flags)
{
    if (bonus_flags & CRITTER_BONUS_DEXTEROUS) {
        int cur = stat_base_get(obj, STAT_DEXTERITY);
        stat_base_set(obj, STAT_DEXTERITY, cur + 4);
    }
    if (bonus_flags & CRITTER_BONUS_DETERMINED) {
        int cur = stat_base_get(obj, STAT_WILLPOWER);
        stat_base_set(obj, STAT_WILLPOWER, cur + 4);
    }
    if (bonus_flags & CRITTER_BONUS_ENRAGED) {
        int cur = stat_base_get(obj, STAT_DAMAGE_BONUS);
        stat_base_set(obj, STAT_DAMAGE_BONUS, cur + 8);
    }
    if (bonus_flags & CRITTER_BONUS_SWIFT) {
        int cur = stat_base_get(obj, STAT_SPEED);
        stat_base_set(obj, STAT_SPEED, cur + 3);
    }
    if (bonus_flags & CRITTER_BONUS_ARMORED) {
        int cur = stat_base_get(obj, STAT_AC_ADJUSTMENT);
        stat_base_set(obj, STAT_AC_ADJUSTMENT, cur + 15);
    }
    if (bonus_flags & CRITTER_BONUS_REGENERATING) {
        int cur = stat_base_get(obj, STAT_HEAL_RATE);
        stat_base_set(obj, STAT_HEAL_RATE, cur + 5);
    }
    if (bonus_flags & CRITTER_BONUS_BRUTISH) {
        int cur = stat_base_get(obj, STAT_STRENGTH);
        stat_base_set(obj, STAT_STRENGTH, cur + 4);
    }
    if (bonus_flags & CRITTER_BONUS_WARY) {
        int cur = stat_base_get(obj, STAT_PERCEPTION);
        stat_base_set(obj, STAT_PERCEPTION, cur + 4);
    }
    if (bonus_flags & CRITTER_BONUS_ROBUST) {
        int cur = stat_base_get(obj, STAT_CONSTITUTION);
        stat_base_set(obj, STAT_CONSTITUTION, cur + 5);
    }
    if (bonus_flags & CRITTER_BONUS_CUNNING) {
        int cur = stat_base_get(obj, STAT_INTELLIGENCE);
        stat_base_set(obj, STAT_INTELLIGENCE, cur + 4);
    }
}

// NG+ thresholds: [level][UNIQUE, RARE, MAGIC]
static const int ng_plus_thresholds[4][3] = {
    { 98, 83, 53 },  // NG+0 (vanilla)
    { 95, 75, 40 },  // NG+1
    { 92, 67, 30 },  // NG+2
    { 88, 58, 20 },  // NG+3
};

void critter_rarity_roll(int64_t obj)
{
    int roll = random_between(1, 100);
    int ng = ng_plus_get_level();
    const int* thr = ng_plus_thresholds[ng];

    CritterRarity rarity = CRITTER_RARITY_NORMAL;
    int bonus_count = 0;

    if (roll >= thr[0]) {
        rarity = CRITTER_RARITY_UNIQUE;
        bonus_count = 3;
    } else if (roll >= thr[1]) {
        rarity = CRITTER_RARITY_RARE;
        bonus_count = 2;
    } else if (roll >= thr[2]) {
        rarity = CRITTER_RARITY_MAGIC;
        bonus_count = 1;
    }

    if (rarity != CRITTER_RARITY_NORMAL) {
        int bonus = pick_bonuses(bonus_count);

        // Boost CON — HP in Arcanum is CON-derived, HP_PTS is 0 at postprocess time.
        static const int con_bonus[] = { 0, 5, 10, 18 };
        int con = stat_base_get(obj, STAT_CONSTITUTION);
        stat_base_set(obj, STAT_CONSTITUTION, con + con_bonus[rarity]);

        apply_bonus(obj, bonus);

        obj_field_int32_set(obj, OBJ_F_CRITTER_PAD_I_1,
            (int)rarity | (bonus << BONUS_SHIFT));
    }

    // CE: NG+ baseline boost — all critters regardless of rarity.
    // HP +25% per level (via adj), STR and WIL +2 per level.
    // WIL feeds both the HP formula (1x) and fatigue formula (1x).
    if (ng > 0) {
        int base_hp = object_hp_max(obj);
        object_hp_adj_set(obj, object_hp_adj_get(obj) + base_hp * ng / 4);
        int str = stat_base_get(obj, STAT_STRENGTH);
        stat_base_set(obj, STAT_STRENGTH, str + ng * 2);
        int wil = stat_base_get(obj, STAT_WILLPOWER);
        stat_base_set(obj, STAT_WILLPOWER, wil + ng * 2);
    }

    // Mark as rolled — persisted in PAD_I_1 bit 14, survives obj diffs.
    // Guards against re-rolling on subsequent map loads (NORMAL critters have PAD_I_1 bits 0-1 = 0,
    // so without this flag the postprocess guard can't distinguish "never rolled" from "rolled NORMAL").
    {
        int pad = obj_field_int32_get(obj, OBJ_F_CRITTER_PAD_I_1);
        obj_field_int32_set(obj, OBJ_F_CRITTER_PAD_I_1, pad | CRITTER_PAD_ROLLED_FLAG);
    }
}
