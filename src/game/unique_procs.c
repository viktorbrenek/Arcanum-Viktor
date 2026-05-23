#include "game/unique_procs.h"

#include "game/anim.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/item_rarity.h"
#include "game/item_tech_props.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/obj_find.h"
#include "game/object.h"
#include "game/location.h"
#include "game/random.h"
#include "game/sector.h"
#include "game/stat.h"
#include "game/tb.h"
#include "game/timeevent.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static void deal_unresisted(int64_t attacker, int64_t target, int dtype, int amount)
{
    CombatContext ctx;
    sub_4B2210(attacker, target, &ctx);
    ctx.dam[dtype] = amount;
    ctx.dam_flags |= CDF_IGNORE_RESISTANCE;
    combat_dmg(&ctx);
}

static void apply_stun_flagged(int64_t target, const char* msg)
{
    unsigned int flags = (unsigned int)obj_field_int32_get(target, OBJ_F_CRITTER_FLAGS);
    if ((flags & OCF_STUNNED) != 0) {
        return;
    }
    if (!anim_goal_animate_stunned(target)) {
        return;
    }
    obj_field_int32_set(target, OBJ_F_CRITTER_FLAGS, (int)(flags | OCF_STUNNED));
    tb_add(target, TB_TYPE_RED, msg);
}

static void aoe_fire_burst(int64_t attacker, int64_t center, int dam, int radius)
{
    int64_t src_loc = obj_field_int64_get(center, OBJ_F_LOCATION);
    int64_t sec_id = sector_id_from_loc(src_loc);
    int64_t obj;
    FindNode* iter;

    deal_unresisted(attacker, center, DAMAGE_TYPE_FIRE, dam);
    tb_add(center, TB_TYPE_RED, "Combustion!");

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        if (obj == center || obj == attacker) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        int type = obj_field_int32_get(obj, OBJ_F_TYPE);
        if (type != OBJ_TYPE_NPC && type != OBJ_TYPE_PC) {
            continue;
        }
        int64_t obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > radius) {
            continue;
        }
        deal_unresisted(attacker, obj, DAMAGE_TYPE_FIRE, dam);
    } while (obj_find_walk_next(&obj, &iter));
}

static void aoe_poison_nova(int64_t attacker, int64_t center, int radius)
{
    int64_t src_loc = obj_field_int64_get(center, OBJ_F_LOCATION);
    int64_t sec_id = sector_id_from_loc(src_loc);
    int64_t obj;
    FindNode* iter;

    tb_add(center, TB_TYPE_GREEN, "Bramble Nova!");

    if (!obj_find_walk_first(sec_id, &obj, &iter)) {
        return;
    }
    do {
        if (obj == attacker) {
            continue;
        }
        if (obj_field_int32_get(obj, OBJ_F_FLAGS) & (int)OF_INVENTORY) {
            continue;
        }
        int type = obj_field_int32_get(obj, OBJ_F_TYPE);
        if (type != OBJ_TYPE_NPC && type != OBJ_TYPE_PC) {
            continue;
        }
        int64_t obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        if (location_dist(src_loc, obj_loc) > radius) {
            continue;
        }
        apply_poison_weapon_dot(attacker, obj, 5, 5);
    } while (obj_find_walk_next(&obj, &iter));
}

// ── state ────────────────────────────────────────────────────────────────────

static int64_t galatea_elec_ready = OBJ_HANDLE_NULL;
static int64_t cogsworth_last_attacker = OBJ_HANDLE_NULL;
static int     cogsworth_hit_count = 0;

// ── public API ────────────────────────────────────────────────────────────────

void unique_procs_on_hit(int64_t attacker, int64_t target, int dam, bool was_concealed)
{
    // Blade of Alberich: 30% shockwave = +15 normal unresisted + stun
    if (item_rarity_has_unique(attacker, UNIQUE_BLADE_OF_ALBERICH)
        && random_between(1, 100) <= 30) {
        deal_unresisted(attacker, target, DAMAGE_TYPE_NORMAL, 15);
        apply_stun_flagged(target, "Shockwave!");
    }

    // Veil of Shadows: was concealed → 2× backstab (extra dam unresisted)
    if (was_concealed && dam > 0
        && item_rarity_has_unique(attacker, UNIQUE_VEIL_OF_SHADOWS)) {
        deal_unresisted(attacker, target, DAMAGE_TYPE_NORMAL, dam);
        tb_add(target, TB_TYPE_RED, "Backstab!");
    }

    // Wyrmfang: target poisoned/burning → fire explosion 15 dmg radius 2
    if (item_rarity_has_unique(attacker, UNIQUE_WYRMFANG)
        && (dot_has_active(target, TIMEEVENT_TYPE_FIRE_DOT)
            || dot_has_active(target, TIMEEVENT_TYPE_POISON_WEAPON_DOT)
            || stat_base_get(target, STAT_POISON_LEVEL) > 0)) {
        aoe_fire_burst(attacker, target, 15, 2);
    }

    // Cogsworth Repeater: every 4th hit → extra dam unresisted (Overclock)
    if (item_rarity_has_unique(attacker, UNIQUE_COGSWORTH_REPEATER)) {
        if (cogsworth_last_attacker != attacker) {
            cogsworth_last_attacker = attacker;
            cogsworth_hit_count = 0;
        }
        cogsworth_hit_count++;
        if (cogsworth_hit_count >= 4) {
            cogsworth_hit_count = 0;
            if (dam > 0) {
                deal_unresisted(attacker, target, DAMAGE_TYPE_NORMAL, dam);
                tb_add(attacker, TB_TYPE_WHITE, "Overclock!");
            }
        }
    }

    // Galatea Mirror: discharge stored electrical bonus
    if (galatea_elec_ready == attacker) {
        galatea_elec_ready = OBJ_HANDLE_NULL;
        deal_unresisted(attacker, target, DAMAGE_TYPE_ELECTRICAL, 20);
        tb_add(attacker, TB_TYPE_WHITE, "Arcane Discharge!");
    }
}

void unique_procs_on_kill(int64_t attacker, int64_t target)
{
    (void)target;

    // Ring of Mephistis: Soul Harvest — +10 HP, +15 fatigue, tb msg
    if (!item_rarity_has_unique(attacker, UNIQUE_RING_OF_MEPHISTIS)) {
        return;
    }

    int hp_dam = object_hp_damage_get(attacker) - 10;
    if (hp_dam < 0) {
        hp_dam = 0;
    }
    object_hp_damage_set(attacker, hp_dam);

    int fat_dam = critter_fatigue_damage_get(attacker) - 15;
    if (fat_dam < 0) {
        fat_dam = 0;
    }
    critter_fatigue_damage_set(attacker, fat_dam);

    tb_add(attacker, TB_TYPE_WHITE, "Soul Harvest!");
}

void unique_procs_on_damage_received(int64_t defender, int64_t attacker, int dam)
{
    if (dam <= 0) {
        return;
    }

    // Ironclad Cogplate: reflect 15% physical damage to attacker
    if (attacker != OBJ_HANDLE_NULL
        && item_rarity_has_unique(defender, UNIQUE_IRONCLAD_COGPLATE)) {
        int reflect = dam * 15 / 100;
        if (reflect < 1) {
            reflect = 1;
        }
        deal_unresisted(defender, attacker, DAMAGE_TYPE_NORMAL, reflect);
    }

    // Stonehide Mantle: under 40% HP → restore 15% damage (post-reduce)
    if (item_rarity_has_unique(defender, UNIQUE_STONEHIDE_MANTLE)) {
        int hp_max = object_hp_max(defender);
        int hp_cur = object_hp_current(defender);
        if (hp_max > 0 && hp_cur * 100 / hp_max < 40) {
            int restore = dam * 15 / 100;
            if (restore > 0) {
                int hp_dam = object_hp_damage_get(defender) - restore;
                if (hp_dam < 0) {
                    hp_dam = 0;
                }
                object_hp_damage_set(defender, hp_dam);
            }
        }
    }

    // Thornweave: 25% Bramble Nova on received hit
    if (attacker != OBJ_HANDLE_NULL
        && item_rarity_has_unique(defender, UNIQUE_THORNWEAVE)
        && random_between(1, 100) <= 25) {
        aoe_poison_nova(defender, defender, 2);
    }
}

bool unique_procs_on_spell_hit(int spell, int64_t caster, int64_t target)
{
    (void)spell;
    (void)target;

    // Tullian Focus: 25% Spell Echo (caller applies extra 15-25 unresisted damage)
    if (item_rarity_has_unique(caster, UNIQUE_TULLIAN_FOCUS)
        && random_between(1, 100) <= 25) {
        tb_add(caster, TB_TYPE_WHITE, "Spell Echo!");
        return true;
    }
    return false;
}

void unique_procs_on_spell_received(int64_t caster, int64_t target, int dam)
{
    (void)caster;

    // Galatea Mirror: 25% absorb → restore fatigue + prime elec next hit
    if (!item_rarity_has_unique(target, UNIQUE_GALATEA_MIRROR)) {
        return;
    }
    if (random_between(1, 100) > 25) {
        return;
    }

    int fat_restore = (dam > 0) ? (dam / 2) : 8;
    if (fat_restore < 1) {
        fat_restore = 1;
    }
    int fat_dam = critter_fatigue_damage_get(target) - fat_restore;
    if (fat_dam < 0) {
        fat_dam = 0;
    }
    critter_fatigue_damage_set(target, fat_dam);

    galatea_elec_ready = target;
    tb_add(target, TB_TYPE_WHITE, "Absorbed!");
}

bool unique_procs_can_knockdown(int64_t target)
{
    // Stonehide Mantle: immune to knockdown and stun
    return !item_rarity_has_unique(target, UNIQUE_STONEHIDE_MANTLE);
}
