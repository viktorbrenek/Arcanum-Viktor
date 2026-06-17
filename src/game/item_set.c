#include "game/item_set.h"

#include <string.h>

#include "game/combat.h"
#include "game/item.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/obj_find.h"
#include "game/obj_flags.h"
#include "game/random.h"
#include "game/sector.h"
#include "game/stat.h"
#include "game/magictech.h"
#include "game/tb.h"
#include "game/timeevent.h"

// SpellEyeCandy fx = spell_idx*10 + eye_candy_type (type 2 = DESTINATION, plays once)
#define FX_NECROTIC  112  // spell 11 Poison Vapours, destination
#define FX_HEAL      602  // spell 60 Minor Healing, destination
#define FX_SHADOW    692  // spell 69 Invisibility, destination
#define FX_REND      312  // spell 31 Jolt, destination
#define FX_ARC       332  // spell 33 Bolt of Lightning, destination

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
static const int num_wear_slots = (int)(sizeof(wear_slots) / sizeof(wear_slots[0]));

SetId item_set_get(int64_t item_obj)
{
    int32_t pad = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    return (SetId)((pad >> 8) & 0xFFFF);
}

void item_set_set(int64_t item_obj, SetId id)
{
    int32_t pad = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    pad = (pad & ~(0xFFFF << 8)) | ((int32_t)(id & 0xFFFF) << 8);
    obj_field_int32_set(item_obj, OBJ_F_ITEM_PAD_I_1, pad);
}

// CE perf: true if a stored inventory location is one of the worn equipment slots.
static bool item_set_loc_is_worn(int loc)
{
    for (int s = 0; s < num_wear_slots; s++) {
        if (loc == wear_slots[s]) {
            return true;
        }
    }
    return false;
}

int item_set_count_equipped(int64_t critter_obj, SetId id)
{
    // CE perf: single inventory pass. The old version called item_wield_get
    // (O(inventory)) once per worn slot (9x), and this runs 6x in
    // item_set_adjust_stat on every stat_level_get -> ~54x O(inventory) per stat
    // query, which collapsed FPS for NPCs with large inventories (King's Inn).
    int count = 0;
    int cnt = obj_field_int32_get(critter_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (int i = 0; i < cnt; i++) {
        int64_t item = obj_arrayfield_handle_get(critter_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, i);
        if (item == OBJ_HANDLE_NULL) {
            continue;
        }
        if (item_set_loc_is_worn(item_inventory_location_get(item))
            && item_set_get(item) == id) {
            count++;
        }
    }
    return count;
}

int item_set_active_tier(int64_t critter_obj, SetId id)
{
    int count = item_set_count_equipped(critter_obj, id);
    if (count >= 3) return 2;
    if (count >= 2) return 1;
    return 0;
}

int item_set_adjust_stat(int64_t critter_obj, int stat, int value)
{
    int tier;

    tier = item_set_active_tier(critter_obj, SET_DREAD_GUARD);
    if (tier >= 1) {
        if (stat == STAT_AC_ADJUSTMENT) value += 12;
        if (stat == STAT_STRENGTH)      value += 3;
    }

    tier = item_set_active_tier(critter_obj, SET_SHROUD_OF_UNSEEN);
    if (tier >= 1) {
        if (stat == STAT_DEXTERITY) value += 4;
    }

    tier = item_set_active_tier(critter_obj, SET_VENDIGROTHIAN_CONFLUENCE);
    if (tier >= 1) {
        if (stat == STAT_INTELLIGENCE) value += 2;
        if (stat == STAT_WILLPOWER)    value += 2;
    }

    tier = item_set_active_tier(critter_obj, SET_IRON_BROTHERHOOD);
    if (tier >= 1) {
        if (stat == STAT_AC_ADJUSTMENT) value += 8;
        if (stat == STAT_CONSTITUTION)  value += 2;
    }

    tier = item_set_active_tier(critter_obj, SET_GNOMISH_MASTERWORK);
    if (tier >= 1) {
        if (stat == STAT_INTELLIGENCE) value += 2;
        if (stat == STAT_DEXTERITY)    value += 2;
    }

    tier = item_set_active_tier(critter_obj, SET_BLESSED_COVENANT);
    if (tier >= 1) {
        if (stat == STAT_WILLPOWER) value += 2;
        if (stat == STAT_CHARISMA)  value += 2;
    }

    return value;
}

void item_set_on_equip(int64_t item_obj, int64_t critter_obj)
{
    (void)item_obj;
    (void)critter_obj;
}

void item_set_on_unequip(int64_t item_obj, int64_t critter_obj)
{
    (void)item_obj;
    (void)critter_obj;
}

// ---------------------------------------------------------------------------
// Timeevent callback — restores a stat and/or clears spell flags after proc debuff expires.
// params[0] = target object, [1] = stat (-1 = no stat restore), [2] = restore value,
// [3] = spell flags to clear (0 = none).
// ---------------------------------------------------------------------------

bool item_set_proc_effect_end_timeevent_process(TimeEvent* timeevent)
{
    int64_t obj = timeevent->params[0].object_value;
    int stat = timeevent->params[1].integer_value;
    int restore_val = timeevent->params[2].integer_value;
    unsigned int clear_sf = (unsigned int)timeevent->params[3].integer_value;

    if (obj == OBJ_HANDLE_NULL) {
        return true;
    }
    if (stat >= 0 && stat < STAT_COUNT) {
        stat_base_set(obj, stat, restore_val);
    }
    if (clear_sf != 0) {
        unsigned int sf = (unsigned int)obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);
        sf &= ~clear_sf;
        obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, (int)sf);
    }
    return true;
}

static void schedule_stat_restore(int64_t obj, int stat, int restore_val,
                                   unsigned int clear_sf, unsigned int ms)
{
    TimeEvent te;
    DateTime delay;

    memset(&te, 0, sizeof(te));
    te.type = TIMEEVENT_TYPE_PROC_EFFECT_END;
    te.params[0].object_value = obj;
    te.params[1].integer_value = stat;
    te.params[2].integer_value = restore_val;
    te.params[3].integer_value = (int)clear_sf;

    memset(&delay, 0, sizeof(delay));
    datetime_add_milliseconds(&delay, ms);
    timeevent_add_delay(&te, &delay);
}

// ---------------------------------------------------------------------------
// Proc effects
// ---------------------------------------------------------------------------

static void proc_necrotic_rend(int64_t attacker, int64_t target)
{
    CombatContext ctx;
    int old_str;
    int new_str;

    // 10 poison damage (necrotic)
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[DAMAGE_TYPE_POISON] = 10;
    combat_dmg(&ctx);
    magictech_fx_add(target, FX_NECROTIC);
    tb_add(attacker, TB_TYPE_RED, "Necrotic Rend!");

    // -2 STR for 20 seconds
    old_str = stat_base_get(target, STAT_STRENGTH);
    new_str = old_str - 2;
    if (new_str < 1) {
        new_str = 1;
    }
    if (new_str < old_str) {
        stat_base_set(target, STAT_STRENGTH, new_str);
        schedule_stat_restore(target, STAT_STRENGTH, old_str, 0, 20000);
    }
}

static void proc_iron_resurgence(int64_t attacker)
{
    CombatContext ctx;

    // Self-heal 15 HP
    sub_4B2210(attacker, attacker, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = 15;
    combat_heal(&ctx);
    magictech_fx_add(attacker, FX_HEAL);
    tb_add(attacker, TB_TYPE_GREEN, "Iron Resurgence!");
}

static void proc_shadow_melt(int64_t attacker)
{
    unsigned int sf;
    int old_spd;

    // Only apply if not already invisible (avoid clobbering a spell's invisibility)
    sf = (unsigned int)obj_field_int32_get(attacker, OBJ_F_SPELL_FLAGS);
    if (sf & OSF_INVISIBLE) {
        return;
    }

    // 8 seconds of invisibility
    obj_field_int32_set(attacker, OBJ_F_SPELL_FLAGS, (int)(sf | OSF_INVISIBLE));
    magictech_fx_add(attacker, FX_SHADOW);
    tb_add(attacker, TB_TYPE_BLUE, "Shadow Melt!");

    // +2 SPEED for 8 seconds
    old_spd = stat_base_get(attacker, STAT_SPEED);
    stat_base_set(attacker, STAT_SPEED, old_spd + 2);

    schedule_stat_restore(attacker, STAT_SPEED, old_spd, OSF_INVISIBLE, 8000);
}

static void proc_rending_strike(int64_t attacker, int64_t target)
{
    int old_ac;

    (void)attacker;

    // -5 AC_ADJUSTMENT for 15 seconds (easier to hit)
    old_ac = stat_base_get(target, STAT_AC_ADJUSTMENT);
    stat_base_set(target, STAT_AC_ADJUSTMENT, old_ac - 5);
    schedule_stat_restore(target, STAT_AC_ADJUSTMENT, old_ac, 0, 15000);
    magictech_fx_add(target, FX_REND);
    tb_add(attacker, TB_TYPE_RED, "Rending Strike!");
}

static void proc_arc_discharge(int64_t attacker)
{
    int64_t src_loc;
    int64_t sec_id;
    int64_t obj;
    FindNode* iter;

    // 15 electrical damage to all NPCs within 3 tiles in same sector
    src_loc = obj_field_int64_get(attacker, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(src_loc);

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        int obj_type;
        int64_t obj_loc;
        CombatContext ctx;

        if (obj == attacker) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
        if (obj_type != OBJ_TYPE_NPC) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > 3) {
            continue;
        }
        sub_4B2210(attacker, obj, &ctx);
        ctx.dam[DAMAGE_TYPE_ELECTRICAL] = 15;
        combat_dmg(&ctx);
        magictech_fx_add(obj, FX_ARC);
    } while (obj_find_walk_next(&obj, &iter));
    tb_add(attacker, TB_TYPE_BLUE, "Arc Discharge!");
}

static void proc_iron_retribution(int64_t victim, int64_t attacker)
{
    CombatContext ctx;

    // Reflect 10 normal damage back to attacker
    sub_4B2210(victim, attacker, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = 10;
    combat_dmg(&ctx);
    tb_add(victim, TB_TYPE_RED, "Iron Retribution!");
}

static void proc_galvanic_pulse(int64_t attacker, int64_t target)
{
    CombatContext ctx;

    // 8 electrical to single target
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[DAMAGE_TYPE_ELECTRICAL] = 8;
    combat_dmg(&ctx);
    magictech_fx_add(target, FX_ARC);
    tb_add(attacker, TB_TYPE_BLUE, "Galvanic Pulse!");
}

static void proc_sacred_mending(int64_t killer)
{
    CombatContext ctx;

    // Self-heal 25 HP
    sub_4B2210(killer, killer, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = 25;
    combat_heal(&ctx);
    magictech_fx_add(killer, FX_HEAL);
    tb_add(killer, TB_TYPE_GREEN, "Sacred Mending!");
}

// ---------------------------------------------------------------------------
// Proc dispatch — called from mt_item notify hooks
// ---------------------------------------------------------------------------

void item_set_notify_hit(int64_t attacker_obj, int64_t weapon_obj, int64_t target_obj)
{
    (void)weapon_obj;

    if (item_set_active_tier(attacker_obj, SET_DREAD_GUARD) >= 2) {
        if (random_between(1, 100) <= 25) {
            proc_necrotic_rend(attacker_obj, target_obj);
        }
    }
    if (item_set_active_tier(attacker_obj, SET_SHROUD_OF_UNSEEN) >= 2) {
        if (random_between(1, 100) <= 20) {
            proc_rending_strike(attacker_obj, target_obj);
        }
    }
    if (item_set_active_tier(attacker_obj, SET_GNOMISH_MASTERWORK) >= 2) {
        if (random_between(1, 100) <= 25) {
            proc_galvanic_pulse(attacker_obj, target_obj);
        }
    }
}

void item_set_notify_hit_taken(int64_t victim_obj, int64_t attacker_obj)
{
    if (item_set_active_tier(victim_obj, SET_DREAD_GUARD) >= 2) {
        if (random_between(1, 100) <= 15) {
            proc_iron_resurgence(victim_obj);
        }
    }
    if (item_set_active_tier(victim_obj, SET_VENDIGROTHIAN_CONFLUENCE) >= 2) {
        if (random_between(1, 100) <= 30) {
            proc_arc_discharge(victim_obj);
        }
    }
    if (item_set_active_tier(victim_obj, SET_IRON_BROTHERHOOD) >= 2) {
        if (random_between(1, 100) <= 20) {
            proc_iron_retribution(victim_obj, attacker_obj);
        }
    }
}

void item_set_notify_kill(int64_t killer_obj, int64_t victim_obj)
{
    (void)victim_obj;

    if (item_set_active_tier(killer_obj, SET_SHROUD_OF_UNSEEN) >= 2) {
        proc_shadow_melt(killer_obj);
    }
    if (item_set_active_tier(killer_obj, SET_BLESSED_COVENANT) >= 2) {
        if (random_between(1, 100) <= 40) {
            proc_sacred_mending(killer_obj);
        }
    }
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

const char* item_set_name(SetId id)
{
    switch (id) {
    case SET_DREAD_GUARD:              return "Dread Guard's Panoply";
    case SET_SHROUD_OF_UNSEEN:         return "Shroud of the Unseen";
    case SET_VENDIGROTHIAN_CONFLUENCE: return "Vendigrothian Confluence";
    case SET_IRON_BROTHERHOOD:         return "Iron Brotherhood";
    case SET_GNOMISH_MASTERWORK:       return "Gnomish Masterwork";
    case SET_BLESSED_COVENANT:         return "Blessed Covenant";
    default:                           return "";
    }
}

const char* item_set_tier_bonus_str(SetId id, int tier)
{
    switch (id) {
    case SET_DREAD_GUARD:
        if (tier == 1) return "2/3: +12 AC, +3 STR";
        if (tier == 2) return "3/3: Necrotic Rend - 25% on-hit: 10 poison dmg, -2 STR 20s\n"
                              "     Iron Resurgence - 15% on-hit-taken: heal 15 HP";
        break;
    case SET_SHROUD_OF_UNSEEN:
        if (tier == 1) return "2/3: +4 DEX";
        if (tier == 2) return "3/3: Rending Strike - 20% on-hit: -5 AC 15s\n"
                              "     Shadow Melt - on-kill: invisible + +2 SPD 8s";
        break;
    case SET_VENDIGROTHIAN_CONFLUENCE:
        if (tier == 1) return "2/3: +2 INT, +2 WIL";
        if (tier == 2) return "3/3: Arc Discharge - 30% on-hit-taken: 15 elec dmg to all in 3 tiles";
        break;
    case SET_IRON_BROTHERHOOD:
        if (tier == 1) return "2/3: +8 AC, +2 CON";
        if (tier == 2) return "3/3: Iron Retribution - 20% on-hit-taken: reflect 10 dmg to attacker";
        break;
    case SET_GNOMISH_MASTERWORK:
        if (tier == 1) return "2/3: +2 INT, +2 DEX";
        if (tier == 2) return "3/3: Galvanic Pulse - 25% on-hit: 8 elec dmg to target";
        break;
    case SET_BLESSED_COVENANT:
        if (tier == 1) return "2/3: +2 WIL, +2 CHA";
        if (tier == 2) return "3/3: Sacred Mending - 40% on-kill: heal 25 HP";
        break;
    default:
        break;
    }
    return "";
}
