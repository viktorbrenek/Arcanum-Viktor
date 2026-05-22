#include "game/item_tech_props.h"

#include <string.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/damage_type.h"
#include "game/descriptions.h"
#include "game/location.h"
#include "game/magictech.h"
#include "game/obj.h"
#include "game/obj_find.h"
#include "game/obj_flags.h"
#include "game/random.h"
#include "game/sector.h"
#include "game/stat.h"
#include "game/tb.h"
#include "game/timeevent.h"

// SpellEyeCandy FX: spell_idx*10 + eye_candy_type (2 = destination, plays once).
#define FX_ARC   332  // spell 33 Bolt of Lightning
#define FX_FIRE  222  // spell 22 Fireflash

// ─── DoT query API ───────────────────────────────────────────────────────────

static int64_t dot_query_target;

static bool dot_target_match(TimeEvent* te)
{
    return te->params[0].object_value == dot_query_target;
}

bool dot_has_active(int64_t obj, int timeevent_type)
{
    dot_query_target = obj;
    return timeevent_any(timeevent_type, dot_target_match);
}

bool dot_has_any_active(int64_t obj)
{
    dot_query_target = obj;
    return timeevent_any(TIMEEVENT_TYPE_FIRE_DOT, dot_target_match)
        || timeevent_any(TIMEEVENT_TYPE_ACID_DOT, dot_target_match)
        || timeevent_any(TIMEEVENT_TYPE_BLEED_DOT, dot_target_match)
        || timeevent_any(TIMEEVENT_TYPE_POISON_WEAPON_DOT, dot_target_match);
}

// ─── DoT shared infrastructure ───────────────────────────────────────────────

// Returns false if target is gone/dead — processor should bail.
static bool dot_target_valid(int64_t target)
{
    if (target == OBJ_HANDLE_NULL) {
        return false;
    }
    return (obj_field_int32_get(target, OBJ_F_FLAGS) & (int)OF_OFF) == 0;
}

static void dot_reschedule(TimeEventType type, int64_t target, int64_t attacker,
                           int damage, int ticks)
{
    TimeEvent next;
    DateTime delay;

    memset(&next, 0, sizeof(next));
    next.type = type;
    next.params[0].object_value = target;
    next.params[1].object_value = attacker;
    next.params[2].integer_value = damage;
    next.params[3].integer_value = ticks;
    memset(&delay, 0, sizeof(delay));
    datetime_add_milliseconds(&delay, 3000);
    timeevent_add_delay(&next, &delay);
}

static void dot_schedule_first(TimeEventType type, int64_t attacker, int64_t target,
                               int dmg_per_tick, int ticks, const char* text)
{
    dot_reschedule(type, target, attacker, dmg_per_tick, ticks);
    tb_add(target, TB_TYPE_RED, text);
}

// ─── Fire DoT ────────────────────────────────────────────────────────────────

bool fire_dot_timeevent_process(TimeEvent* timeevent)
{
    int64_t target  = timeevent->params[0].object_value;
    int64_t attacker = timeevent->params[1].object_value;
    int damage = timeevent->params[2].integer_value;
    int ticks  = timeevent->params[3].integer_value;
    CombatContext ctx;

    if (!dot_target_valid(target)) {
        return true;
    }
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[DAMAGE_TYPE_FIRE] = damage;
    combat_dmg(&ctx);
    magictech_fx_add(target, FX_FIRE);

    ticks--;
    if (ticks > 0) {
        dot_reschedule(TIMEEVENT_TYPE_FIRE_DOT, target, attacker, damage, ticks);
    }
    return true;
}

void apply_fire_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks)
{
    dot_schedule_first(TIMEEVENT_TYPE_FIRE_DOT, attacker, target, dmg_per_tick, ticks, "Burning!");
}

// ─── Acid DoT ────────────────────────────────────────────────────────────────
// Uses DAMAGE_TYPE_NORMAL + CDF_IGNORE_RESISTANCE to bypass armor (corrosion).

#define FX_NECROTIC 112  // spell 11 Poison Vapours destination

bool acid_dot_timeevent_process(TimeEvent* timeevent)
{
    int64_t target  = timeevent->params[0].object_value;
    int64_t attacker = timeevent->params[1].object_value;
    int damage = timeevent->params[2].integer_value;
    int ticks  = timeevent->params[3].integer_value;
    CombatContext ctx;

    if (!dot_target_valid(target)) {
        return true;
    }
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = damage;
    ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
    combat_dmg(&ctx);
    magictech_fx_add(target, FX_NECROTIC);

    ticks--;
    if (ticks > 0) {
        dot_reschedule(TIMEEVENT_TYPE_ACID_DOT, target, attacker, damage, ticks);
    }
    return true;
}

void apply_acid_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks)
{
    dot_schedule_first(TIMEEVENT_TYPE_ACID_DOT, attacker, target, dmg_per_tick, ticks, "Corroding!");
}

// ─── Bleed DoT ───────────────────────────────────────────────────────────────
// Physical cut — DAMAGE_TYPE_NORMAL with blood splotch visual.

bool bleed_dot_timeevent_process(TimeEvent* timeevent)
{
    int64_t target  = timeevent->params[0].object_value;
    int64_t attacker = timeevent->params[1].object_value;
    int damage = timeevent->params[2].integer_value;
    int ticks  = timeevent->params[3].integer_value;
    CombatContext ctx;

    if (!dot_target_valid(target)) {
        return true;
    }
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = damage;
    combat_dmg(&ctx);
    anim_play_blood_splotch_fx(target, BLOOD_SPLOTCH_TYPE_NORMAL, DAMAGE_TYPE_NORMAL, &ctx);

    ticks--;
    if (ticks > 0) {
        dot_reschedule(TIMEEVENT_TYPE_BLEED_DOT, target, attacker, damage, ticks);
    }
    return true;
}

void apply_bleed_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks)
{
    dot_schedule_first(TIMEEVENT_TYPE_BLEED_DOT, attacker, target, dmg_per_tick, ticks, "Bleeding!");
}

// ─── Poison Weapon DoT ───────────────────────────────────────────────────────
// Short-burst weapon poison — DAMAGE_TYPE_POISON, distinct from STAT_POISON_LEVEL.

bool poison_weapon_dot_timeevent_process(TimeEvent* timeevent)
{
    int64_t target  = timeevent->params[0].object_value;
    int64_t attacker = timeevent->params[1].object_value;
    int damage = timeevent->params[2].integer_value;
    int ticks  = timeevent->params[3].integer_value;
    CombatContext ctx;

    if (!dot_target_valid(target)) {
        return true;
    }
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[DAMAGE_TYPE_POISON] = damage;
    combat_dmg(&ctx);
    magictech_fx_add(target, FX_NECROTIC);
    anim_play_blood_splotch_fx(target, BLOOD_SPLOTCH_TYPE_POISON, DAMAGE_TYPE_POISON, &ctx);

    ticks--;
    if (ticks > 0) {
        dot_reschedule(TIMEEVENT_TYPE_POISON_WEAPON_DOT, target, attacker, damage, ticks);
    }
    return true;
}

void apply_poison_weapon_dot(int64_t attacker, int64_t target, int dmg_per_tick, int ticks)
{
    dot_schedule_first(TIMEEVENT_TYPE_POISON_WEAPON_DOT, attacker, target, dmg_per_tick, ticks, "Poisoned!");
}

// ─── shared helpers ───────────────────────────────────────────────────────────

static void apply_stun(int64_t target_obj)
{
    unsigned int flags = (unsigned int)obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS);
    if ((flags & OCF_STUNNED) != 0) {
        return;
    }
    flags |= OCF_STUNNED;
    if (!anim_goal_animate_stunned(target_obj)) {
        return;
    }
    obj_field_int32_set(target_obj, OBJ_F_CRITTER_FLAGS, (int)flags);
}

static void schedule_stat_restore_tech(int64_t obj, int stat, int restore_val, unsigned int ms)
{
    TimeEvent te;
    DateTime delay;

    memset(&te, 0, sizeof(te));
    te.type = TIMEEVENT_TYPE_PROC_EFFECT_END;
    te.params[0].object_value = obj;
    te.params[1].integer_value = stat;
    te.params[2].integer_value = restore_val;
    te.params[3].integer_value = 0;

    memset(&delay, 0, sizeof(delay));
    datetime_add_milliseconds(&delay, ms);
    timeevent_add_delay(&te, &delay);
}

// ─── proc implementations ─────────────────────────────────────────────────────

// 5% stun / 15% prone — mutually exclusive, one d100 roll.
static void proc_knockback(int64_t target_obj)
{
    int roll = random_between(0, 99);
    if (roll < 5) {
        apply_stun(target_obj);
    } else if (roll < 20) {
        anim_goal_make_knockdown(target_obj);
    }
}

// 40% chance — chain 12 electrical damage to nearest NPC within 2 tiles of target.
static void proc_tesla_chain(int64_t attacker_obj, int64_t target_obj)
{
    int64_t src_loc;
    int64_t sec_id;
    int64_t obj;
    int64_t chain_target;
    FindNode* iter;

    if (random_between(0, 99) >= 40) {
        return;
    }

    src_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(src_loc);
    chain_target = OBJ_HANDLE_NULL;

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        int64_t obj_loc;
        if (obj == target_obj || obj == attacker_obj) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > 2) {
            continue;
        }
        chain_target = obj;
        break;
    } while (obj_find_walk_next(&obj, &iter));

    if (chain_target == OBJ_HANDLE_NULL) {
        return;
    }

    CombatContext ctx;
    sub_4B2210(attacker_obj, chain_target, &ctx);
    ctx.dam[DAMAGE_TYPE_ELECTRICAL] = 12;
    combat_dmg(&ctx);
    magictech_fx_add(chain_target, FX_ARC);
    tb_add(attacker_obj, TB_TYPE_BLUE, "Tesla Chain!");
}

// 30% chance — stun target with electrical effect.
static void proc_electric_stun(int64_t target_obj)
{
    if (random_between(0, 99) >= 30) {
        return;
    }
    apply_stun(target_obj);
    magictech_fx_add(target_obj, FX_ARC);
    tb_add(target_obj, TB_TYPE_BLUE, "Stunned!");
}

// Always — hit all NPCs within 1 tile of primary target for 6-12 normal damage.
static void proc_shotgun_spread(int64_t attacker_obj, int64_t target_obj)
{
    int64_t src_loc;
    int64_t sec_id;
    int64_t obj;
    FindNode* iter;

    src_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(src_loc);

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        int64_t obj_loc;
        CombatContext ctx;
        if (obj == target_obj) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > 1) {
            continue;
        }
        sub_4B2210(attacker_obj, obj, &ctx);
        ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(6, 12);
        combat_dmg(&ctx);
    } while (obj_find_walk_next(&obj, &iter));
}

// Always — deal 2 extra normal damage hits to target (burst fire identity).
static void proc_mechanized_burst(int64_t attacker_obj, int64_t target_obj)
{
    for (int i = 0; i < 2; i++) {
        CombatContext ctx;
        sub_4B2210(attacker_obj, target_obj, &ctx);
        ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(8, 14);
        combat_dmg(&ctx);
    }
}

// On crit only — -2 SPEED for 8 seconds.
static void proc_sniper_slow(int64_t target_obj)
{
    int old_spd;
    int new_spd;

    old_spd = stat_base_get(target_obj, STAT_SPEED);
    new_spd = old_spd - 2;
    if (new_spd < 1) {
        new_spd = 1;
    }
    if (new_spd >= old_spd) {
        return;
    }
    stat_base_set(target_obj, STAT_SPEED, new_spd);
    schedule_stat_restore_tech(target_obj, STAT_SPEED, old_spd, 8000);
    tb_add(target_obj, TB_TYPE_RED, "Slowed!");
}

// Always — 8-15 fire damage to primary target + all NPCs within 1 tile + 2-tick fire DoT.
static void proc_flamethrower_aoe(int64_t attacker_obj, int64_t target_obj)
{
    int64_t src_loc;
    int64_t sec_id;
    int64_t obj;
    FindNode* iter;
    CombatContext ctx;

    sub_4B2210(attacker_obj, target_obj, &ctx);
    ctx.dam[DAMAGE_TYPE_FIRE] = random_between(8, 15);
    combat_dmg(&ctx);
    magictech_fx_add(target_obj, FX_FIRE);
    apply_fire_dot(attacker_obj, target_obj, 4, 2);

    src_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(src_loc);

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        int64_t obj_loc;
        if (obj == target_obj) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > 1) {
            continue;
        }
        sub_4B2210(attacker_obj, obj, &ctx);
        ctx.dam[DAMAGE_TYPE_FIRE] = random_between(8, 15);
        combat_dmg(&ctx);
        magictech_fx_add(obj, FX_FIRE);
        apply_fire_dot(attacker_obj, obj, 4, 2);
    } while (obj_find_walk_next(&obj, &iter));
}

// Always — 6-10 extra fire damage to target + 1-tick fire DoT.
static void proc_pyrotechnic_fire(int64_t attacker_obj, int64_t target_obj)
{
    CombatContext ctx;
    sub_4B2210(attacker_obj, target_obj, &ctx);
    ctx.dam[DAMAGE_TYPE_FIRE] = random_between(6, 10);
    combat_dmg(&ctx);
    magictech_fx_add(target_obj, FX_FIRE);
    apply_fire_dot(attacker_obj, target_obj, 3, 1);
}

// ─── public ───────────────────────────────────────────────────────────────────

void tech_weapon_on_hit(int64_t attacker_obj, int64_t weapon_obj, int64_t target_obj,
                        unsigned int combat_flags)
{
    int bp;

    if (weapon_obj == OBJ_HANDLE_NULL || target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    bp = obj_field_int32_get(weapon_obj, OBJ_F_DESCRIPTION);

    switch (bp) {
    case BP_ELEPHANT_GUN:
    case BP_MACHINED_HAMMER:
        proc_knockback(target_obj);
        break;
    case BP_TESLA_GUN:
        proc_tesla_chain(attacker_obj, target_obj);
        break;
    case BP_TESLA_ROD:
        proc_electric_stun(target_obj);
        break;
    case BP_SHOTGUN:
        proc_shotgun_spread(attacker_obj, target_obj);
        break;
    case BP_MECHANIZED_GUN:
        proc_mechanized_burst(attacker_obj, target_obj);
        break;
    case BP_LOOKING_GLASS_RIFLE:
        if ((combat_flags & CF_CRITICAL) != 0) {
            proc_sniper_slow(target_obj);
        }
        break;
    case BP_FLAME_THROWER:
        proc_flamethrower_aoe(attacker_obj, target_obj);
        break;
    case BP_PYROTECHNIC_GUN:
    case BP_PYROTECHNIC_AXE:
        proc_pyrotechnic_fire(attacker_obj, target_obj);
        break;
    default:
        break;
    }
}
