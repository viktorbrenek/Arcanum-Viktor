#include "game/endgame_map.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "game/critter_rarity.h"
#include "game/descriptions.h"
#include "game/location.h"
#include "game/map.h"
#include "game/mp_utils.h"
#include "game/ng_plus.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/player.h"
#include "game/random.h"
#include "game/settings.h"
#include "game/teleport.h"
#include "game/ui.h"
#include "ui/inven_ui.h"
#include "tig/debug.h"

#define ENDGAME_MAP_NAME  "endgame_dungeon"
#define ENDGAME_SAVE_DIR  "Save\\Current\\maps\\" ENDGAME_MAP_NAME
#define ENDGAME_CFG_PATH  "Save\\Current\\endgame.cfg"

// Sector 0 of the endgame_dungeon map is 64x64 tiles (0-63).
// Place start near centre so scatter offsets stay in-bounds.
#define ENDGAME_START_X 32
#define ENDGAME_START_Y 32

#define ENDGAME_CRITTERS_BASE 6  // +2 per NG+ level

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
    settings_load(&endgame_cfg);
    endgame_cfg_initialized = true;
}

// Delete all files in the dungeon save directory (forces fresh load on next entry).
static void endgame_clear_save_dir(void)
{
    WIN32_FIND_DATAA fd;
    char pattern[MAX_PATH];
    char full[MAX_PATH];
    HANDLE h;

    snprintf(pattern, sizeof(pattern), "%s\\*", ENDGAME_SAVE_DIR);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (fd.cFileName[0] == '.') {
            continue;
        }
        snprintf(full, sizeof(full), "%s\\%s", ENDGAME_SAVE_DIR, fd.cFileName);
        DeleteFileA(full);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    RemoveDirectoryA(ENDGAME_SAVE_DIR);
}

// ---- critter spawn ----------------------------------------------------------

static void endgame_spawn_critters(void)
{
    int ng = ng_plus_get_level();
    int count = ENDGAME_CRITTERS_BASE + ng * 2;
    endgame_critters_remaining = 0;

    for (int i = 0; i < count; i++) {
        int proto = endgame_critter_pool[random_between(0, ENDGAME_POOL_SIZE - 1)];
        int dx = scatter_offsets[i % SCATTER_COUNT][0];
        int dy = scatter_offsets[i % SCATTER_COUNT][1];
        int64_t loc = location_make(ENDGAME_START_X + dx, ENDGAME_START_Y + dy);
        int64_t critter_obj;
        if (mp_object_create(proto, loc, &critter_obj)) {
            critter_rarity_roll(critter_obj);
            endgame_critters_remaining++;
        }
    }

    tig_debug_printf("endgame_map: spawned %d critters (NG+%d)\n",
        endgame_critters_remaining, ng);
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

    // Save return map; PC tile gets serialised into mobile.mdy by teleport_do.
    int return_map = map_current_map();
    settings_set_value(&endgame_cfg, CFG_KEY_RETURN_MAP, return_map);
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
    endgame_cfg_ensure_init();

    int return_map = settings_get_value(&endgame_cfg, CFG_KEY_RETURN_MAP);
    endgame_critters_remaining = 0;

    if (return_map <= 0) {
        tig_debug_println("endgame_map_exit: no return map saved");
        return;
    }

    // Reset return map entry so a stale re-entry can't trigger again.
    settings_set_value(&endgame_cfg, CFG_KEY_RETURN_MAP, 0);
    settings_save(&endgame_cfg);

    // PC tile is restored automatically from the return map's sector save.
    if (!map_open_in_game(return_map, false, false)) {
        tig_debug_println("endgame_map_exit: map_open_in_game failed");
    }

    endgame_feedback("The rift spits you back into the world.");
}

void endgame_map_on_critter_killed(int64_t critter_obj)
{
    (void)critter_obj;

    if (!endgame_map_is_active() || endgame_critters_remaining <= 0) {
        return;
    }

    endgame_critters_remaining--;
    tig_debug_printf("endgame_map: %d critters remaining\n", endgame_critters_remaining);

    if (endgame_critters_remaining == 0) {
        endgame_feedback("The last enemy falls. The rift collapses — you are cast back into the world.");
        endgame_map_exit();
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
    endgame_feedback("The rift tears open — darkness swallows you whole.");
}
