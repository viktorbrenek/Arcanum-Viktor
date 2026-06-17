#include "game/spell_ce.h"

#include <string.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/damage_type.h"
#include "game/item_tech_props.h"
#include "game/location.h"
#include "game/magictech.h"
#include "game/obj.h"
#include "game/obj_find.h"
#include "game/obj_flags.h"
#include "game/random.h"
#include "game/sector.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/tb.h"
#include "game/timeevent.h"
#include "game/unique_procs.h"

#define IS_BEGIN(a)    ((a) == MAGICTECH_ACTION_BEGIN)
#define IS_MAINTAIN(a) ((a) == MAGICTECH_ACTION_MAINTAIN)

// FX IDs for secondary effects.
#define FX_ARC   332   // spell 33 Bolt of Lightning
#define FX_FIRE  222   // spell 22 Fireflash

// ─── Harm cooldown ────────────────────────────────────────────────────────────

static int64_t harm_cooldown_caster;

static bool harm_cooldown_match(TimeEvent* te)
{
    return te->params[0].object_value == harm_cooldown_caster;
}

static bool harm_caster_has_cooldown(int64_t caster)
{
    harm_cooldown_caster = caster;
    return timeevent_any(TIMEEVENT_TYPE_HARM_COOLDOWN, harm_cooldown_match);
}

static void harm_add_cooldown(int64_t caster)
{
    TimeEvent te;
    DateTime delay;

    memset(&te, 0, sizeof(te));
    te.type = TIMEEVENT_TYPE_HARM_COOLDOWN;
    te.params[0].object_value = caster;
    memset(&delay, 0, sizeof(delay));
    datetime_add_milliseconds(&delay, 3000);
    timeevent_add_delay(&te, &delay);
}

bool spell_ce_harm_cooldown_process(TimeEvent* timeevent)
{
    (void)timeevent;
    return true;
}

// ─── shared helpers ───────────────────────────────────────────────────────────

static void schedule_stat_restore_spell(int64_t obj, int stat, int restore_val, unsigned int ms)
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

static void spell_ce_apply_stun(int64_t target)
{
    if (!unique_procs_can_knockdown(target)) {
        return;
    }
    unsigned int flags = (unsigned int)obj_field_int32_get(target, OBJ_F_CRITTER_FLAGS);
    if ((flags & OCF_STUNNED) != 0) {
        return;
    }
    if (!anim_goal_animate_stunned(target)) {
        return;
    }
    flags |= OCF_STUNNED;
    obj_field_int32_set(target, OBJ_F_CRITTER_FLAGS, (int)flags);
}

static bool target_is_alive(int64_t obj)
{
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }
    return (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_OFF) == 0;
}

// ─── transform form tracking ─────────────────────────────────────────────────

static int64_t wolf_form_holder   = OBJ_HANDLE_NULL;
static int64_t bear_form_holder   = OBJ_HANDLE_NULL;
static int64_t lizard_form_holder = OBJ_HANDLE_NULL;

static bool form_holder_check(int64_t obj, int64_t* holder)
{
    if (obj != *holder) {
        return false;
    }
    if (!(obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & (int)OSF_POLYMORPHED)) {
        *holder = OBJ_HANDLE_NULL;
        return false;
    }
    return true;
}

int spell_ce_life_on_hit_bonus(int64_t attacker_obj)
{
    return form_holder_check(attacker_obj, &wolf_form_holder) ? 2 : 0;
}

int spell_ce_thorns_bonus(int64_t target_obj)
{
    return form_holder_check(target_obj, &bear_form_holder) ? 15 : 0;
}

bool spell_ce_has_wolf_form(int64_t attacker_obj)
{
    return form_holder_check(attacker_obj, &wolf_form_holder);
}

bool spell_ce_has_lizard_form(int64_t attacker_obj)
{
    return form_holder_check(attacker_obj, &lizard_form_holder);
}

// ─── per-school procs ─────────────────────────────────────────────────────────

// Force — BOLT_OF_LIGHTNING: chain 10-20 elec to nearest NPC within 3 tiles of target.
static void proc_lightning_chain(int64_t caster, int64_t primary_target)
{
    int64_t src_loc;
    int64_t sec_id;
    int64_t obj;
    int64_t obj_loc;
    int64_t chain_target;
    FindNode* iter;
    CombatContext ctx;

    src_loc = obj_field_int64_get(primary_target, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(src_loc);
    chain_target = OBJ_HANDLE_NULL;

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        if (obj == primary_target || obj == caster) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > 3) {
            continue;
        }
        chain_target = obj;
        break;
    } while (obj_find_walk_next(&obj, &iter));

    if (chain_target == OBJ_HANDLE_NULL) {
        return;
    }

    sub_4B2210(caster, chain_target, &ctx);
    ctx.dam[DAMAGE_TYPE_ELECTRICAL] = random_between(10, 20);
    combat_dmg(&ctx);
    magictech_fx_add(chain_target, FX_ARC);
    tb_add(chain_target, TB_TYPE_WHITE, "Chained!");
}

// Phantasm — FLASH: 8-14 normal dmg + 40% stun (terror)
static void proc_terror(int64_t caster, int64_t target)
{
    CombatContext ctx;

    sub_4B2210(caster, target, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(8, 14);
    combat_dmg(&ctx);
    if (random_between(0, 99) < 40) {
        spell_ce_apply_stun(target);
        tb_add(target, TB_TYPE_RED, "Terrified!");
    }
}

// White Necro — MAJOR_HEALING: find nearest NPC enemy within 3 tiles of heal-target, deal 8-15 unresisted damage.
static void proc_radiant_burst(int64_t caster, int64_t heal_target)
{
    int64_t src_loc;
    int64_t sec_id;
    int64_t obj;
    int64_t obj_loc;
    int64_t enemy;
    FindNode* iter;
    CombatContext ctx;

    src_loc = obj_field_int64_get(heal_target, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(src_loc);
    enemy = OBJ_HANDLE_NULL;

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        if (obj == heal_target || obj == caster) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > 3) {
            continue;
        }
        enemy = obj;
        break;
    } while (obj_find_walk_next(&obj, &iter));

    if (enemy == OBJ_HANDLE_NULL) {
        return;
    }

    sub_4B2210(caster, enemy, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(8, 15);
    ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
    combat_dmg(&ctx);
    tb_add(enemy, TB_TYPE_WHITE, "Radiant!");
}

// Meta — DISPERSE_MAGICK: 12-20 unresisted "dispel backlash" damage.
static void proc_dispel_blast(int64_t caster, int64_t target)
{
    CombatContext ctx;

    sub_4B2210(caster, target, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(12, 20);
    ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
    combat_dmg(&ctx);
    tb_add(target, TB_TYPE_RED, "Dispelled!");
}

// Conveyance — UNSEEN_FORCE: 8-15 normal damage + 40% knockdown.
static void proc_unseen_force(int64_t caster, int64_t target)
{
    CombatContext ctx;

    sub_4B2210(caster, target, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(8, 15);
    combat_dmg(&ctx);
    if (random_between(0, 99) < 40 && unique_procs_can_knockdown(target)) {
        anim_goal_make_knockdown(target);
        tb_add(target, TB_TYPE_RED, "Hurled!");
    }
}

// Air — CALL_WINDS: 10-18 normal damage + 25% knockdown.
static void proc_wind_strike(int64_t caster, int64_t target)
{
    CombatContext ctx;

    sub_4B2210(caster, target, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(10, 18);
    combat_dmg(&ctx);
    if (random_between(0, 99) < 25 && unique_procs_can_knockdown(target)) {
        anim_goal_make_knockdown(target);
        tb_add(target, TB_TYPE_RED, "Blown Back!");
    }
}

// ─── public hooks ─────────────────────────────────────────────────────────────

// Blood Magic: Black Necro spells cost HP instead of fatigue.
// Called once per cast on BEGIN. Returns HP to drain from caster (0 = not blood magic).
static int blood_magic_hp_cost(int spell)
{
    switch (spell) {
    case SPELL_HARM:           return 7;
    case SPELL_CONJURE_SPIRIT: return 25;
    // SPELL_SUMMON_UNDEAD: no upfront cost — 5 HP per 10s tick via on_target MAINTAIN
    case SPELL_CREATE_UNDEAD:  return 60;   // Lifetaker
    case SPELL_QUENCH_LIFE:    return 70;   // Finger of Death
    default:                   return 0;
    }
}

void spell_ce_blood_magic_pay(int64_t caster_obj, int hp_cost)
{
    CombatContext ctx;

    sub_4B2210(caster_obj, caster_obj, &ctx);
    ctx.dam[DAMAGE_TYPE_NORMAL] = hp_cost;
    ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
    combat_dmg(&ctx);
    tb_add(caster_obj, TB_TYPE_RED, "Blood cost!");
}

// Lifetaker drain radius (tiles around the caster). Tunable.
#define LIFETAKER_DRAIN_RADIUS 8

void spell_ce_lifetaker_drain(int64_t caster_obj)
{
    int64_t caster_loc;
    int64_t sec_id;
    int64_t obj;
    int64_t obj_loc;
    FindNode* iter;
    CombatContext ctx;
    int total_drained = 0;
    int amount;
    int cur_hp;
    int max_hp;
    int new_hp;

    if (caster_obj == OBJ_HANDLE_NULL) {
        return;
    }

    caster_loc = obj_field_int64_get(caster_obj, OBJ_F_LOCATION);
    sec_id = sector_id_from_loc(caster_loc);

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        if (obj == caster_obj) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
            continue;
        }
        if (!target_is_alive(obj) || critter_is_dead(obj)) {
            continue;
        }
        if (critter_party_same(caster_obj, obj)) {
            continue;
        }
        obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(caster_loc, obj_loc) > LIFETAKER_DRAIN_RADIUS) {
            continue;
        }

        // Drain a few HP from this enemy (unresistable).
        amount = random_between(3, 6);
        sub_4B2210(caster_obj, obj, &ctx);
        ctx.dam[DAMAGE_TYPE_NORMAL] = amount;
        ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
        combat_dmg(&ctx);
        total_drained += amount;
        tb_add(obj, TB_TYPE_RED, "Siphoned!");
    } while (obj_find_walk_next(&obj, &iter));

    // Heal the caster for the total amount drained this tick.
    if (total_drained > 0) {
        cur_hp = object_hp_current(caster_obj);
        max_hp = object_hp_max(caster_obj);
        if (cur_hp < max_hp) {
            new_hp = cur_hp + total_drained;
            if (new_hp > max_hp) {
                new_hp = max_hp;
            }
            object_hp_damage_set(caster_obj, max_hp - new_hp);
        }
        tb_add(caster_obj, TB_TYPE_WHITE, "Life Drained!");
    }
}

void spell_ce_pre_begin(int spell, int64_t caster_obj, int* aptitude_ptr)
{
    int hp_cost = blood_magic_hp_cost(spell);
    if (hp_cost > 0) {
        spell_ce_blood_magic_pay(caster_obj, hp_cost);
    }

    if (spell != SPELL_HARM) {
        return;
    }
    // Cap aptitude: prevents over-scaling at very high WP.
    if (*aptitude_ptr > 100) {
        *aptitude_ptr = 100;
    }
    // Cooldown penalty: spamming Harm deals minimum damage.
    if (harm_caster_has_cooldown(caster_obj)) {
        *aptitude_ptr = 0;
    }
}

void spell_ce_on_target(int spell, int action, int64_t caster_obj, int64_t target_obj)
{
    if (!target_is_alive(target_obj)) {
        return;
    }

    switch (spell) {
    case SPELL_HARM:
        if (IS_BEGIN(action)) {
            harm_add_cooldown(caster_obj);
        }
        break;
    case SPELL_FIREFLASH:
        // Fire DoT: apply on BEGIN; refresh on MAINTAIN only if not already active.
        if (IS_BEGIN(action) || !dot_has_active(target_obj, TIMEEVENT_TYPE_FIRE_DOT)) {
            apply_fire_dot(caster_obj, target_obj, 4, 3);
        }
        break;
    // SPELL_SQUALL_OF_ICE slot replaced by Acid Rain (mod) — damage handled by SpellList, no hook needed.
    case SPELL_BOLT_OF_LIGHTNING:
        if (IS_BEGIN(action)) {
            proc_lightning_chain(caster_obj, target_obj);
        }
        break;
    case SPELL_STONE_THROW:
        // Poison DoT: apply on BEGIN; refresh on MAINTAIN only if not already active.
        if (IS_BEGIN(action) || !dot_has_active(target_obj, TIMEEVENT_TYPE_POISON_WEAPON_DOT)) {
            apply_poison_weapon_dot(caster_obj, target_obj, 3, 3);
        }
        break;
    case SPELL_DRAIN_WILL:
        // Maintained — stun chance fires each MAINTAIN tick (not just BEGIN).
        if (random_between(0, 99) < 30) {
            spell_ce_apply_stun(target_obj);
            tb_add(target_obj, TB_TYPE_RED, "Confused!");
        }
        break;
    case SPELL_FLASH:
        if (IS_BEGIN(action)) {
            proc_terror(caster_obj, target_obj);
        }
        break;
    case SPELL_CONGEAL_TIME:
        // Acid DoT: refresh only when not active; stun on BEGIN only.
        if (IS_BEGIN(action) || !dot_has_active(target_obj, TIMEEVENT_TYPE_ACID_DOT)) {
            apply_acid_dot(caster_obj, target_obj, 3, 3);
        }
        if (IS_BEGIN(action)) {
            spell_ce_apply_stun(target_obj);
            tb_add(target_obj, TB_TYPE_RED, "Time-Burned!");
        }
        break;
    case SPELL_ENTANGLE:
        // Bleed DoT: refresh only when not active.
        if (IS_BEGIN(action) || !dot_has_active(target_obj, TIMEEVENT_TYPE_BLEED_DOT)) {
            apply_bleed_dot(caster_obj, target_obj, 3, 3);
        }
        break;
    case SPELL_MAJOR_HEALING:
        if (IS_BEGIN(action)) {
            proc_radiant_burst(caster_obj, target_obj);
        }
        break;
    case SPELL_DISPERSE_MAGICK:
        if (IS_BEGIN(action)) {
            proc_dispel_blast(caster_obj, target_obj);
        }
        break;
    // SPELL_WEAKEN slot replaced by Soften (mod) — DR reduction handled by SpellList Effect 154, no hook needed.
    case SPELL_PLAGUE_OF_INSECTS:
        // Bleed DoT: refresh only when not active.
        if (IS_BEGIN(action) || !dot_has_active(target_obj, TIMEEVENT_TYPE_BLEED_DOT)) {
            apply_bleed_dot(caster_obj, target_obj, 2, 5);
        }
        break;
    case SPELL_UNSEEN_FORCE:
        if (IS_BEGIN(action)) {
            proc_unseen_force(caster_obj, target_obj);
        }
        break;
    case SPELL_CALL_WINDS:
        if (IS_BEGIN(action)) {
            proc_wind_strike(caster_obj, target_obj);
        }
        break;

    // SPELL_CREATE_UNDEAD (Lifetaker): drain+heal moved to per-tick maintenance
    // hook spell_ce_lifetaker_drain() (called from magictech sub_4532F0), so it
    // fires reliably every maintain tick like Summon Undead's blood cost.
    case SPELL_CHARM:  // Meditation: spend 15 HP to restore 25 fatigue
        if (IS_BEGIN(action)) {
            CombatContext med_ctx;
            int cur_fat_dmg;
            int new_fat_dmg;

            // HP cost — unresistable self-damage.
            sub_4B2210(caster_obj, caster_obj, &med_ctx);
            med_ctx.dam[DAMAGE_TYPE_NORMAL] = 15;
            med_ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
            combat_dmg(&med_ctx);

            // Fatigue restore — reduce fatigue damage by 50, floor at 0.
            cur_fat_dmg = critter_fatigue_damage_get(caster_obj);
            new_fat_dmg = cur_fat_dmg - 25;
            if (new_fat_dmg < 0) {
                new_fat_dmg = 0;
            }
            critter_fatigue_damage_set(caster_obj, new_fat_dmg);

            tb_add(caster_obj, TB_TYPE_WHITE, "Focus!");
        }
        break;
    case SPELL_CHARM_BEAST:  // Wolf Form (Lycanthropy)
        if (IS_BEGIN(action)) {
            wolf_form_holder = caster_obj;
        }
        break;
    case SPELL_CONTROL_BEAST:  // Lizard Form
        if (IS_BEGIN(action)) {
            lizard_form_holder = caster_obj;
        }
        break;
    case SPELL_REGENERATE:  // Bear God Form
        if (IS_BEGIN(action)) {
            bear_form_holder = caster_obj;
        }
        break;
    default:
        break;
    }

    if (IS_BEGIN(action) && caster_obj != target_obj) {
        if (unique_procs_on_spell_hit(spell, caster_obj, target_obj)) {
            CombatContext echo_ctx;
            sub_4B2210(caster_obj, target_obj, &echo_ctx);
            echo_ctx.dam[DAMAGE_TYPE_NORMAL] = random_between(15, 25);
            echo_ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
            combat_dmg(&echo_ctx);
        }
        unique_procs_on_spell_received(caster_obj, target_obj, 0);
    }
}
