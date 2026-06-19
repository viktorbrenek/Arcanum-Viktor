#include "game/endgame_map.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "game/critter.h"
#include "game/critter_rarity.h"
#include "game/item_rarity.h"
#include "game/descriptions.h"
#include "game/location.h"
#include "game/map.h"
#include "game/mp_utils.h"
#include "game/ng_plus.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/player.h"
#include "game/random.h"
#include "game/script.h"
#include "game/item.h"
#include "game/item_orb.h"
#include "game/light.h"
#include "game/settings.h"
#include "game/teleport.h"
#include "game/ui.h"
#include "ui/inven_ui.h"
#include "tig/debug.h"
#include "tig/art.h"
#include "tig/file.h"

#define ENDGAME_MAP_NAME  "endgame_dungeon"
#define ENDGAME_SAVE_DIR  "Save\\Current\\maps\\" ENDGAME_MAP_NAME
#define ENDGAME_CFG_PATH  "Save\\Current\\endgame.cfg"

// Sector 0 of the endgame_dungeon map is 64x64 tiles (0-63).
// Place start near centre so scatter offsets stay in-bounds.
#define ENDGAME_START_X 32
#define ENDGAME_START_Y 32

#define ENDGAME_CRITTERS_BASE 6  // +2 per NG+ level

// Viktor's "Stolen Master Rune" quest (see player_house.c, dlg\30710viktor.dlg).
// Progress is tracked in global variable 1900: 0 = not started, 1 = accepted,
// 2 = done. Using a global var (not the quest-state machine) keeps the gate
// reliable across save/load.
#define VIKTOR_QUEST_VAR         1900
#define VIKTOR_QUEST_ACCEPTED    1

// Viktor's follow-up "Five Sigils" quest (see dlg\30710viktor.dlg, player_house.c).
// SIGILS_VAR is a bitmask: bit N is set the first time the player CLEARS a rift of
// RiftType N. When all RIFT_COUNT bits are set (== SIGILS_ALL_MASK) the dialog lets
// the player turn the quest in; that sets VIKTOR_RUNES_VAR = 2, which unlocks the
// elemental imbue runes in Viktor's shop. Tracked always, even before quest accept.
#define SIGILS_VAR               1901
#define SIGILS_ALL_MASK          ((1 << RIFT_COUNT) - 1)
#define VIKTOR_RUNES_VAR         1902   // 0 = none, 1 = accepted, 2 = done (runes unlocked)
#define MASTER_RUNE_PROTO        BP_GEODE  // reskinned generic item (proto 15186)
#define MASTER_RUNE_DESCRIPTION  3101      // description.mes -> "Master Rune"
#define MASTER_RUNE_NAME         15186     // OBJ_F_NAME the dialog "in15186" matches

static const int endgame_critter_pool[] = {
    BP_GREATER_DEMON_1,
    BP_GREATER_DEMON_2,
    BP_DEMI_LICHE,
    BP_SLIME_DEMON,
    BP_DEMON,
    BP_LESSER_DEMON,
};
#define ENDGAME_POOL_SIZE ((int)(sizeof(endgame_critter_pool) / sizeof(endgame_critter_pool[0])))

// Scatter pattern: (dx, dy) offsets from start tile.
// Keep 12-20 tile radius so critters don't spawn on top of the PC (map is 64x64).
static const int scatter_offsets[][2] = {
    { 15,  0 }, {-15,  0 }, {  0, 15 }, {  0,-15 },
    { 11, 11 }, {-11, 11 }, { 11,-11 }, {-11,-11 },
    { 18,  5 }, {-18,  5 }, {  5, 18 }, { -5, 18 },
    { 18, -5 }, {-18, -5 }, {  5,-18 }, { -5,-18 },
};
#define SCATTER_COUNT ((int)(sizeof(scatter_offsets) / sizeof(scatter_offsets[0])))

static int endgame_map_id = -1;
static int endgame_critters_remaining = 0;
static bool endgame_spawn_pending = false;

static Settings endgame_cfg;
static bool endgame_cfg_initialized = false;

#define CFG_KEY_RETURN_MAP "return_map"

// ---- helpers ----------------------------------------------------------------

static void endgame_feedback(const char* msg)
{
    UiMessage ui_msg;
    ui_msg.type = UI_MSG_TYPE_EXCLAMATION;
    ui_msg.str = (char*)msg;
    ui_display_msg(&ui_msg);
}

static void endgame_cfg_ensure_init(void)
{
    if (endgame_cfg_initialized) {
        return;
    }
    settings_init(&endgame_cfg, ENDGAME_CFG_PATH);
    settings_register(&endgame_cfg, CFG_KEY_RETURN_MAP, "0", NULL);
    settings_register(&endgame_cfg, "return_x", "0", NULL);
    settings_register(&endgame_cfg, "return_y", "0", NULL);
    settings_register(&endgame_cfg, "rift_tier", "1", NULL);
    settings_register(&endgame_cfg, "rift_type", "0", NULL);
    settings_load(&endgame_cfg);
    endgame_cfg_initialized = true;
}

int endgame_map_get_tier(void)
{
    endgame_cfg_ensure_init();
    int tier = settings_get_value(&endgame_cfg, "rift_tier");
    if (tier < 1) tier = 1;
    return tier;
}

RiftType endgame_map_get_type(void)
{
    endgame_cfg_ensure_init();
    int rtype = settings_get_value(&endgame_cfg, "rift_type");
    if (rtype < 0 || rtype >= RIFT_COUNT) rtype = RIFT_PHYSICAL;
    return (RiftType)rtype;
}

static void endgame_clear_save_dir(void)
{
    tig_file_empty_directory(ENDGAME_SAVE_DIR);
    tig_file_rmdir(ENDGAME_SAVE_DIR);
}

// ---- critter spawn ----------------------------------------------------------

static void endgame_spawn_scenery(void)
{
    RiftType type = endgame_map_get_type();
    
    int protos[4];
    int proto_count = 0;
    
    switch (type) {
    case RIFT_PHYSICAL:
        protos[0] = BP_BIG_STONE;
        protos[1] = BP_STONE;
        protos[2] = BP_SMALL_STONE;
        protos[3] = BP_WALL_OF_STONE;
        proto_count = 4;
        break;
    case RIFT_FIRE:
        protos[0] = BP_WALL_OF_FIRE;
        protos[1] = BP_TORCH_FLAME;
        protos[2] = BP_DEAD_TREE;
        protos[3] = BP_WALL_OF_FIRE;
        proto_count = 4;
        break;
    case RIFT_POISON:
        protos[0] = BP_PLANT_SCENERY;
        protos[1] = BP_STINKING_CLOUD;
        protos[2] = BP_PLANT_SCENERY;
        protos[3] = BP_DEAD_TREE;
        proto_count = 4;
        break;
    case RIFT_MAGIC:
        protos[0] = BP_WALL_OF_FORCE;
        protos[1] = BP_LIGHT_2;
        protos[2] = BP_LIGHT_3;
        protos[3] = BP_WALL_OF_FORCE;
        proto_count = 4;
        break;
    case RIFT_VOID:
        protos[0] = BP_DEAD_TREE;
        protos[1] = BP_POOL_OF_BLOOD;
        protos[2] = BP_DEAD_TREE;
        protos[3] = BP_POOL_OF_BLOOD;
        proto_count = 4;
        break;
    default:
        break;
    }
    
    if (proto_count == 0) return;
    
    static const int scenery_offsets[][2] = {
        { 8,  8 }, { -8,  8 }, {  8, -8 }, { -8, -8 },
        { 12,  4 }, { -12,  4 }, { 12, -4 }, { -12, -4 },
        {  4, 12 }, {  -4, 12 }, {  4,-12 }, {  -4,-12 }
    };
    int count = (int)(sizeof(scenery_offsets) / sizeof(scenery_offsets[0]));
    
    for (int i = 0; i < count; i++) {
        int proto = protos[random_between(0, proto_count - 1)];
        int dx = scenery_offsets[i][0];
        int dy = scenery_offsets[i][1];
        int64_t loc = location_make(ENDGAME_START_X + dx, ENDGAME_START_Y + dy);
        int64_t sc_obj;
        mp_object_create(proto, loc, &sc_obj);
    }
}

static void endgame_spawn_critters(void)
{
    int tier = endgame_map_get_tier();
    int count = ENDGAME_CRITTERS_BASE + 2 * tier;
    if (count > 30) count = 30;
    endgame_critters_remaining = 0;

    RiftType type = endgame_map_get_type();

    static const int pool_physical[] = {
        BP_LESSER_BOAR,
        BP_GREATER_BOAR,
        BP_EARTH_ELEMENTAL,
        BP_ENRAGED_BOAR,
        BP_RABID_BOAR,
        BP_ORE_GOLEM
    };
    static const int pool_fire[] = {
        BP_FIRE_SPIDER,
        BP_FLAMESHADE,
        BP_MOLTEN_ARACHNID,
        BP_FIRE_ELEMENTAL,
        BP_LESSER_DEMON,
        BP_DEMON
    };
    static const int pool_poison[] = {
        BP_SPIDER,
        BP_VENOM_HOUND,
        BP_GREATER_SPIDER,
        BP_DREAD_SPIDER,
        BP_SLIME_DEMON
    };
    static const int pool_magic[] = {
        BP_AIR_ELEMENTAL,
        BP_WATER_ELEMENTAL,
        BP_STORM_FURY,
        BP_EVIL_TEMPEST,
        BP_AUTOMATON_1,
        BP_AUTOMATON_2
    };
    static const int pool_void[] = {
        BP_SHADOW,
        BP_LESSER_VOID_LIZARD,
        BP_GREATER_VOID_LIZARD,
        BP_DREAD_LIZARD,
        BP_DEATH_STRIKER,
        BP_TERROR_CLAW,
        BP_FOE_MANGLER,
        BP_GREATER_DEMON_2
    };

    const int* pool = pool_physical;
    int pool_size = 6;

    switch (type) {
    case RIFT_FIRE:
        pool = pool_fire;
        pool_size = 6;
        break;
    case RIFT_POISON:
        pool = pool_poison;
        pool_size = 5;
        break;
    case RIFT_MAGIC:
        pool = pool_magic;
        pool_size = 6;
        break;
    case RIFT_VOID:
        pool = pool_void;
        pool_size = 8;
        break;
    default:
        pool = pool_physical;
        pool_size = 6;
        break;
    }

    int max_idx = tier;
    if (max_idx >= pool_size) max_idx = pool_size - 1;
    if (max_idx < 0) max_idx = 0;

    // Swarm rifts: weak melee packs (PHYSICAL = boars; POISON = venom spiders + dire/
    // venom hounds) that were too few and spawned too far -> the player picked them off
    // in a slow trickle. Flood them (~2.5x count, own cap) AND pull the spawn ring in
    // tight (below) so they arrive as a pack. Extra spawns also mean more
    // critter_rarity rolls -> more elites.
    bool swarm = (type == RIFT_PHYSICAL || type == RIFT_POISON);
    if (swarm) {
        count = count * 5 / 2;
        if (count > 60) count = 60;
    }

    // Decorate map with thematic scenery
    endgame_spawn_scenery();

    for (int i = 0; i < count; i++) {
        int proto = pool[random_between(0, max_idx)];
        int dx = scatter_offsets[i % SCATTER_COUNT][0];
        int dy = scatter_offsets[i % SCATTER_COUNT][1];
        if (swarm) {
            // Pull the spawn ring from ~11-18 tiles to ~7-11 so the pack closes on the
            // player instead of trickling in. Not tighter: the ~4-8 radius once
            // aggro-killed the freshly teleported PC on entry.
            dx = dx * 3 / 5;
            dy = dy * 3 / 5;
        }
        // Jitter each spawn: with count > SCATTER_COUNT (the horde reuses slots) many
        // critters share a scatter tile; without spread mp_object_create fails on the
        // occupied tile and the critter is silently dropped, capping the real swarm.
        dx += random_between(-3, 3);
        dy += random_between(-3, 3);
        int64_t loc = location_make(ENDGAME_START_X + dx, ENDGAME_START_Y + dy);
        int64_t critter_obj;
        if (mp_object_create(proto, loc, &critter_obj)) {
            critter_rarity_roll(critter_obj);
            endgame_critters_remaining++;
        }
    }

    tig_debug_printf("endgame_map: spawned %d critters (Tier %d, type %d)\n",
        endgame_critters_remaining, tier, (int)type);
}

// ---- public API -------------------------------------------------------------

void endgame_map_init(void)
{
    int64_t x = ENDGAME_START_X;
    int64_t y = ENDGAME_START_Y;

    if (!map_list_info_add(ENDGAME_MAP_NAME, x, y, false)) {
        tig_debug_println("endgame_map_init: map_list_info_add failed");
        return;
    }

    int idx = map_list_info_find(ENDGAME_MAP_NAME);
    if (idx < 0) {
        tig_debug_println("endgame_map_init: map not found after add");
        return;
    }
    endgame_map_id = idx + 1;
    tig_debug_printf("endgame_map_init: registered as map ID %d\n", endgame_map_id);
}

bool endgame_map_enter(void)
{
#ifndef NDEBUG
    // DEBUG: skip NG+ gate so the map can be tested without completing the game.
    (void)ng_plus_is_unlocked;
#else
    if (!ng_plus_is_unlocked()) {
        endgame_feedback("The rift does not respond. Prove yourself first.");
        return false;
    }
#endif
    if (endgame_map_id < 0) {
        tig_debug_println("endgame_map_enter: map not registered");
        return false;
    }
    if (endgame_map_is_active()) {
        endgame_feedback("You are already inside the rift.");
        return false;
    }

    endgame_cfg_ensure_init();

    // Close inventory before map transition to avoid stale UI state.
    if (inven_ui_is_created()) {
        inven_ui_destroy();
    }

    // Roll a random Rift Type and save it
    RiftType rtype = (RiftType)random_between(0, RIFT_COUNT - 1);
    settings_set_value(&endgame_cfg, "rift_type", (int)rtype);

    // Save return map; PC tile gets serialised into mobile.mdy by teleport_do.
    int return_map = map_current_map();
    settings_set_value(&endgame_cfg, CFG_KEY_RETURN_MAP, return_map);
    int64_t pc = player_get_local_pc_obj();
    if (pc != OBJ_HANDLE_NULL) {
        int64_t return_loc = obj_field_int64_get(pc, OBJ_F_LOCATION);
        int64_t rx = location_get_x(return_loc);
        int64_t ry = location_get_y(return_loc);
        settings_set_value(&endgame_cfg, "return_x", (int)rx);
        settings_set_value(&endgame_cfg, "return_y", (int)ry);
    }
    settings_save(&endgame_cfg);

    // Wipe dungeon save so the sector data is fresh.
    endgame_clear_save_dir();

    // teleport_do writes the PC into the destination map's mobile.mdy then loads
    // the map — this properly places the PC in the sector system at the start tile.
    // It is async: the actual map load happens in teleport_ping() on the next frame.
    // Critter spawn is deferred to endgame_map_on_map_opened().
    TeleportData td;
    memset(&td, 0, sizeof(td));
    td.flags = 0;
    td.obj = player_get_local_pc_obj();
    td.loc = location_make(ENDGAME_START_X, ENDGAME_START_Y);
    td.map = endgame_map_id;

    if (!teleport_do(&td)) {
        tig_debug_println("endgame_map_enter: teleport_do failed");
        return false;
    }

    endgame_spawn_pending = true;
    return true;
}

void endgame_map_exit(void)
{
    if (inven_ui_is_created()) {
        inven_ui_destroy();
    }

    endgame_cfg_ensure_init();

    int return_map = settings_get_value(&endgame_cfg, CFG_KEY_RETURN_MAP);
    int rx = settings_get_value(&endgame_cfg, "return_x");
    int ry = settings_get_value(&endgame_cfg, "return_y");
    endgame_critters_remaining = 0;

    if (return_map <= 0) {
        tig_debug_println("endgame_map_exit: no return map saved");
        return;
    }

    // Reset return map entry so a stale re-entry can't trigger again.
    settings_set_value(&endgame_cfg, CFG_KEY_RETURN_MAP, 0);
    settings_save(&endgame_cfg);

    int64_t pc = player_get_local_pc_obj();
    if (pc != OBJ_HANDLE_NULL) {
        if (rx <= 0 || ry <= 0) {
            int64_t sx, sy;
            if (map_get_starting_location(return_map, &sx, &sy)) {
                rx = (int)sx;
                ry = (int)sy;
            }
        }

        TeleportData td;
        memset(&td, 0, sizeof(td));
        td.flags = 0;
        td.obj = pc;
        td.loc = location_make(rx, ry);
        td.map = return_map;

        if (!teleport_do(&td)) {
            tig_debug_println("endgame_map_exit: teleport_do failed");
        }
    }

    endgame_feedback("The rift spits you back into the world.");
}

static void endgame_spawn_reward_chest(int64_t loc, int tier)
{
    int64_t cx = location_get_x(loc);
    int64_t cy = location_get_y(loc);
    int64_t chest_loc = location_make(cx + 2, cy);

    // Try the preferred tile first, then a ring of nearby tiles. The last enemy
    // can fall on a blocked/edge tile where chest creation fails; without this
    // the reward chest (and its Rift Exit Stone) would silently never appear,
    // stranding the player in the rift.
    static const int chest_offsets[][2] = {
        { 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 },
        { 2, 2 }, { -2, 2 }, { 2, -2 }, { -2, -2 }, { 0, 0 }
    };
    int64_t chest_obj = OBJ_HANDLE_NULL;
    for (int ci = 0; ci < (int)(sizeof(chest_offsets) / sizeof(chest_offsets[0])); ci++) {
        chest_loc = location_make(cx + chest_offsets[ci][0], cy + chest_offsets[ci][1]);
        if (mp_object_create(BP_CHEST_1, chest_loc, &chest_obj)) {
            break;
        }
        chest_obj = OBJ_HANDLE_NULL;
    }
    if (chest_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // 1. Spawning gold (scales with tier)
    int gold_amount = 1000 * tier + random_between(0, 500 * tier);
    int64_t gold_obj = item_gold_create(gold_amount, chest_loc);
    if (gold_obj != OBJ_HANDLE_NULL) {
        if (!item_transfer(gold_obj, chest_obj)) {
            object_destroy(gold_obj);
        }
    }

    // 2. Spawning Map of the Void (for next tier)
    int64_t map_obj;
    if (mp_object_create(BP_COMPONENT_1, chest_loc, &map_obj)) {
        item_orb_set_type(map_obj, ORB_MAP);
        if (!item_transfer(map_obj, chest_obj)) {
            object_destroy(map_obj);
        }
    }

    // 3. Spawning Void Portal Stone (Rift Exit Stone)
    int64_t exit_stone_obj;
    if (mp_object_create(BP_COMPONENT_1, chest_loc, &exit_stone_obj)) {
        item_orb_set_type(exit_stone_obj, ORB_EXIT_STONE);
        if (!item_transfer(exit_stone_obj, chest_obj)) {
            object_destroy(exit_stone_obj);
        }
    }

    // 4. Spawning random crafting orbs (1-3 orbs based on tier)
    int orb_count = random_between(1, 2 + tier / 2);
    if (orb_count > 5) orb_count = 5;
    for (int i = 0; i < orb_count; i++) {
        int64_t orb_obj;
        if (mp_object_create(BP_COMPONENT_1, chest_loc, &orb_obj)) {
            OrbType rolled_type = item_orb_roll_type();
            if (rolled_type == ORB_MAP || rolled_type == ORB_NONE) {
                rolled_type = ORB_IDENTIFICATION;
            }
            item_orb_set_type(orb_obj, rolled_type);
            if (!item_transfer(orb_obj, chest_obj)) {
                object_destroy(orb_obj);
            }
        }
    }

    // 5. Spawning random scaling gear (weapons & armors)
    static const int base_item_pool[] = {
        BP_QUALITY_BROADSWORD, BP_CALADON_ELITE_SWORD, BP_CLAYMORE, BP_AXE,
        BP_MACE, BP_BOW, BP_RIFLE, BP_STAFF, BP_STUDDED_LEATHER, BP_CHAINMAIL,
        BP_MACHINED_PLATEMAIL, BP_DRAGON_SKIN_LEATHER, BP_WOODEN_SHIELD,
        BP_HELMET, BP_GAUNTLETS, BP_BOOTS, BP_CLAW, BP_BOXER, BP_THORNFIST,
        BP_STEAMCLAW, BP_RUNEFIST, BP_PINGLOVES, BP_THORNVEST,
        BP_THORNYBULWARK, BP_CROWNOFTHORNS, BP_BRAMBLEDSHOES
    };
    int pool_size = (int)(sizeof(base_item_pool) / sizeof(base_item_pool[0]));

    int equip_count = random_between(2, 4);
    for (int i = 0; i < equip_count; i++) {
        int proto = base_item_pool[random_between(0, pool_size - 1)];
        int64_t gear_obj;
        if (mp_object_create(proto, chest_loc, &gear_obj)) {
            int roll = random_between(1, 100) + tier * 10;
            ItemRarity forced_rarity = ITEM_RARITY_UNCOMMON;
            if (roll > 105) {
                forced_rarity = ITEM_RARITY_SET;
            } else if (roll > 95) {
                forced_rarity = ITEM_RARITY_UNIQUE;
            } else if (roll > 75) {
                forced_rarity = ITEM_RARITY_EPIC;
            } else if (roll > 45) {
                forced_rarity = ITEM_RARITY_RARE;
            }

            item_rarity_roll_forced(gear_obj, forced_rarity);
            if (!item_transfer(gear_obj, chest_obj)) {
                object_destroy(gear_obj);
            }
        }
    }


}

void endgame_map_on_critter_killed(int64_t critter_obj)
{
    if (!endgame_map_is_active()) {
        return;
    }

    // Count hostile critters still alive on the rift map, rather than decrement a
    // static counter — that counter is lost on save/load, so killing the last monster
    // after a reload left it at 0 and the reward chest never spawned. Recounting is
    // robust because the rift map holds only the spawned monsters (+ the PC).
    int alive = 0;
    int64_t obj;
    int iter;
    if (obj_inst_first(&obj, &iter)) {
        do {
            if (obj == critter_obj) {
                continue; // the one just killed (may not be flagged dead yet)
            }
            if (!obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
                continue;
            }
            // PC, followers and summons don't gate completion.
            if (player_is_pc_obj(obj) || critter_pc_leader_get(obj) != OBJ_HANDLE_NULL) {
                continue;
            }
            if (critter_is_dead(obj)) {
                continue;
            }
            alive++;
        } while (obj_inst_next(&obj, &iter));
    }
    endgame_critters_remaining = alive;
    tig_debug_printf("endgame_map: %d critters remaining\n", alive);

    if (alive == 0) {
        int tier = endgame_map_get_tier();
        tier++;
        settings_set_value(&endgame_cfg, "rift_tier", tier);
        settings_save(&endgame_cfg);

        // Five Sigils quest: mark this rift type as cleared (bit in SIGILS_VAR).
        int sigils = script_global_var_get(SIGILS_VAR);
        int sigil_bit = 1 << (int)endgame_map_get_type();
        if ((sigils & sigil_bit) == 0) {
            // Newly cleared nature — record it and tell the player the progress so the
            // Five Sigils quest has visible feedback ("how many maps am I done with?").
            sigils |= sigil_bit;
            script_global_var_set(SIGILS_VAR, sigils);

            int done = 0;
            for (int b = 0; b < RIFT_COUNT; b++) {
                if (sigils & (1 << b)) {
                    done++;
                }
            }
            char sigil_msg[160];
            if (done >= RIFT_COUNT) {
                snprintf(sigil_msg, sizeof(sigil_msg),
                    "All five rift natures are silenced! Return to Viktor to claim your reward.");
            } else {
                snprintf(sigil_msg, sizeof(sigil_msg),
                    "This rift's nature is silenced -- %d of %d gathered for Viktor.",
                    done, RIFT_COUNT);
            }
            endgame_feedback(sigil_msg);
        }

        // Get critter location
        int64_t spawn_loc = OBJ_HANDLE_NULL;
        if (critter_obj != OBJ_HANDLE_NULL) {
            spawn_loc = obj_field_int64_get(critter_obj, OBJ_F_LOCATION);
        }
        if (spawn_loc == OBJ_HANDLE_NULL) {
            int64_t pc = player_get_local_pc_obj();
            if (pc != OBJ_HANDLE_NULL) {
                spawn_loc = obj_field_int64_get(pc, OBJ_F_LOCATION);
            } else {
                spawn_loc = location_make(ENDGAME_START_X, ENDGAME_START_Y);
            }
        }

        // Spawn chest with rewards and exit portal
        endgame_spawn_reward_chest(spawn_loc, tier - 1);

        char msg_buf[256];
        snprintf(msg_buf, sizeof(msg_buf),
            "The last enemy falls. A Rift Reward Chest and Portal have appeared!\n"
            "Rift Tier increased to %d!", tier);
        endgame_feedback(msg_buf);
    }
}

bool endgame_map_is_active(void)
{
    return endgame_map_id >= 0 && map_current_map() == endgame_map_id;
}

void endgame_map_on_map_opened(int map_id)
{
    if (map_id != endgame_map_id || !endgame_spawn_pending) {
        return;
    }
    endgame_spawn_pending = false;
    endgame_spawn_critters();

    RiftType type = endgame_map_get_type();
    char msg_buf[256];
    switch (type) {
    case RIFT_FIRE:
        snprintf(msg_buf, sizeof(msg_buf), "The Rift of Fire tears open! Monsters deal bonus fire damage.");
        break;
    case RIFT_POISON:
        snprintf(msg_buf, sizeof(msg_buf), "The Rift of Poison tears open! Monsters deal bonus poison damage.");
        break;
    case RIFT_MAGIC:
        snprintf(msg_buf, sizeof(msg_buf), "The Rift of Magic tears open! Monsters deal bonus shock damage.");
        break;
    case RIFT_VOID:
        snprintf(msg_buf, sizeof(msg_buf), "The Void Rift tears open! Enemies are extremely deadly but yield the richest rewards.");
        break;
    default:
        snprintf(msg_buf, sizeof(msg_buf), "The Rift of Stone tears open! Monsters deal bonus physical damage.");
        break;
    }
    endgame_feedback(msg_buf);

    // Viktor's stolen Master Rune: while his quest is accepted, the rune the
    // thief carried into the Void lies here on the ground. Spawned on entry so
    // it does not depend on clearing the rift or on the reward chest.
    // See player_house.c and dlg\30710viktor.dlg (quest 51).
    {
        int64_t pc = player_get_local_pc_obj();
        // Only while the quest is ACCEPTED (not done) AND the player isn't already
        // carrying a rune — otherwise every rift entry spawned another Master Rune
        // (duplicates / the quest looking repeatable).
        if (pc != OBJ_HANDLE_NULL
            && script_global_var_get(VIKTOR_QUEST_VAR) == VIKTOR_QUEST_ACCEPTED
            && item_find_by_name(pc, MASTER_RUNE_NAME) == OBJ_HANDLE_NULL) {
            int64_t rune_obj;
            int64_t rune_loc = location_make(ENDGAME_START_X + 1, ENDGAME_START_Y + 1);
            if (mp_object_create(MASTER_RUNE_PROTO, rune_loc, &rune_obj)) {
                obj_field_int32_set(rune_obj, OBJ_F_DESCRIPTION, MASTER_RUNE_DESCRIPTION);
                obj_field_int32_set(rune_obj, OBJ_F_ITEM_DESCRIPTION_UNKNOWN, MASTER_RUNE_DESCRIPTION);
                // The dialog "in" opcode matches OBJ_F_NAME (not the proto id),
                // so stamp a unique quest-item name the turn-in line checks for.
                obj_field_int32_set(rune_obj, OBJ_F_NAME, MASTER_RUNE_NAME);
                endgame_feedback("Viktor's Master Rune lies here in the dust -- the thief's trail ends in the Void. Take it back to him.");
            }
        }
    }

    // Five Sigils: show progress on every rift entry while the quest is accepted, so
    // the player always knows how many natures they've silenced (visible signal).
    if (script_global_var_get(VIKTOR_RUNES_VAR) == 1) {
        int sg = script_global_var_get(SIGILS_VAR);
        int done = 0;
        for (int b = 0; b < RIFT_COUNT; b++) {
            if (sg & (1 << b)) {
                done++;
            }
        }
        char pbuf[160];
        if (done >= RIFT_COUNT) {
            snprintf(pbuf, sizeof(pbuf),
                "Five Sigils: all %d rift natures silenced -- return to Viktor!", RIFT_COUNT);
        } else {
            snprintf(pbuf, sizeof(pbuf),
                "Five Sigils: %d of %d rift natures silenced so far.", done, RIFT_COUNT);
        }
        endgame_feedback(pbuf);
    }
}
