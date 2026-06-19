#include "game/player_house.h"

#include <string.h>

#include "game/critter.h"
#include "game/descriptions.h"
#include "game/item.h"
#include "game/item_orb.h"
#include "game/location.h"
#include "game/map.h"
#include "game/mp_utils.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/script.h"
#include "game/teleport.h"
#include "game/ui.h"
#include "ui/inven_ui.h"
#include "tig/debug.h"

#define PLAYER_HOUSE_MAP_NAME "player_house"

// --- Viktor, the Void runesmith (vendor + quest giver) -----------------------
// Viktor is spawned once when the house is first built. He sells crafting orbs
// ("runes") and offers the "Stolen Master Rune" quest (see dlg\30710viktor.dlg).
#define VIKTOR_DESCRIPTION   3100   // description.mes entry -> displayed name "Viktor"
#define VIKTOR_DIALOG_NUM    30710  // dlg\30710viktor.dlg (SAP_DIALOG script num)
#define VIKTOR_INVEN_SOURCE  125    // rules\InvenSource.mes set (curated orbs)
#define VIKTOR_RUNE_SOURCE   ORB_RUNE_VENDOR_SOURCE // 127 — orbs + elemental imbue runes
#define VIKTOR_RUNES_VAR     1902   // global var: 2 = Five Sigils done -> runes unlocked
#define VIKTOR_RUNES_DONE    2
#define VIKTOR_PORTRAIT      1005   // generic human male portrait

// Inventory source for Viktor right now: the rune-stocked set once the Five Sigils
// quest is complete, otherwise the basic orb set.
static int viktor_inven_source(void)
{
    return (script_global_var_get(VIKTOR_RUNES_VAR) == VIKTOR_RUNES_DONE)
        ? VIKTOR_RUNE_SOURCE : VIKTOR_INVEN_SOURCE;
}

// Start tile near the centre of the (64x64) sector.
#define HOUSE_START_X 32
#define HOUSE_START_Y 32

// Half-size of the walled room (so the room is (2*R+1) tiles per side).
#define HOUSE_RADIUS 4

// Return state and the "built" flag are stored in unused PC object fields so
// they persist with the savegame (an external .cfg in Save\Current does not
// survive a game save/load reliably).
#define HOUSE_F_RETURN_MAP OBJ_F_PC_PAD_I_1   // int: map the PC came from
#define HOUSE_F_BUILT      OBJ_F_PC_PAD_I_2   // int: 1 once the room is built
#define HOUSE_F_RETURN_LOC OBJ_F_PC_PAD_I64AS_1 // int64[0]: return location

static int player_house_map_id = -1;
static bool house_enter_pending = false;

static void house_feedback(const char* msg)
{
    UiMessage ui_msg;
    ui_msg.type = UI_MSG_TYPE_EXCLAMATION;
    ui_msg.str = (char*)msg;
    ui_display_msg(&ui_msg);
}

void player_house_init(void)
{
    int idx;

    if (!map_list_info_add(PLAYER_HOUSE_MAP_NAME, HOUSE_START_X, HOUSE_START_Y, false)) {
        tig_debug_println("player_house_init: map_list_info_add failed");
        return;
    }

    idx = map_list_info_find(PLAYER_HOUSE_MAP_NAME);
    if (idx < 0) {
        tig_debug_println("player_house_init: map not found after add");
        return;
    }

    player_house_map_id = idx + 1;
    tig_debug_printf("player_house_init: registered as map ID %d\n", player_house_map_id);
}

void player_house_reset(void)
{
    house_enter_pending = false;
}

bool player_house_is_active(void)
{
    return player_house_map_id >= 0 && map_current_map() == player_house_map_id;
}

static void house_enter(void)
{
    TeleportData td;
    int64_t pc;

    pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL) {
        return;
    }

    if (inven_ui_is_created()) {
        inven_ui_destroy();
    }

    // Remember where we came from (stored on the PC so it persists with saves).
    obj_field_int32_set(pc, HOUSE_F_RETURN_MAP, map_current_map());
    obj_arrayfield_int64_set(pc, HOUSE_F_RETURN_LOC, 0, obj_field_int64_get(pc, OBJ_F_LOCATION));

    memset(&td, 0, sizeof(td));
    td.flags = 0;
    td.obj = pc;
    td.loc = location_make(HOUSE_START_X, HOUSE_START_Y);
    td.map = player_house_map_id;

    if (!teleport_do(&td)) {
        tig_debug_println("house_enter: teleport_do failed");
        return;
    }

    house_enter_pending = true;
}

static void house_exit(void)
{
    TeleportData td;
    int64_t pc;
    int return_map;
    int64_t return_loc;

    pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL) {
        return;
    }

    if (inven_ui_is_created()) {
        inven_ui_destroy();
    }

    return_map = obj_field_int32_get(pc, HOUSE_F_RETURN_MAP);

    if (return_map <= 0) {
        tig_debug_println("house_exit: no return map saved");
        return;
    }

    return_loc = obj_arrayfield_int64_get(pc, HOUSE_F_RETURN_LOC, 0);
    if (return_loc == 0) {
        // Fall back to the return map's start tile if the stored tile is bad.
        int64_t sx;
        int64_t sy;
        if (map_get_starting_location(return_map, &sx, &sy)) {
            return_loc = location_make(sx, sy);
        }
    }

    memset(&td, 0, sizeof(td));
    td.flags = 0;
    td.obj = pc;
    td.loc = return_loc;
    td.map = return_map;

    if (!teleport_do(&td)) {
        tig_debug_println("house_exit: teleport_do failed");
    }
}

void player_house_toggle(void)
{
    if (player_house_map_id < 0) {
        tig_debug_println("player_house_toggle: map not registered");
        return;
    }

    if (player_house_is_active()) {
        house_exit();
    } else {
        house_enter();
    }
}

// Turn a freshly created NPC into Viktor: name, portrait, dialog and the
// merchant stock (built from InvenSourceBuy.mes by sub_463E20, exactly like
// player.c does for the PC).
static void house_setup_viktor(int64_t viktor_obj)
{
    Script scr;

    obj_field_int32_set(viktor_obj, OBJ_F_DESCRIPTION, VIKTOR_DESCRIPTION);
    obj_field_int32_set(viktor_obj, OBJ_F_CRITTER_DESCRIPTION_UNKNOWN, VIKTOR_DESCRIPTION);
    obj_field_int32_set(viktor_obj, OBJ_F_CRITTER_PORTRAIT, VIKTOR_PORTRAIT);

    scr.num = VIKTOR_DIALOG_NUM;
    obj_arrayfield_script_set(viktor_obj, OBJ_F_SCRIPTS_IDX, SAP_DIALOG, &scr);

    obj_field_int32_set(viktor_obj, OBJ_F_CRITTER_INVENTORY_SOURCE, viktor_inven_source());
    sub_463E20(viktor_obj); // build the for-sale substitute inventory (the runes)
}

// On house entry, sync Viktor's shop to the current quest state: once the Five Sigils
// quest is done, swap him to the rune-stocked source and restock. He was spawned once
// (possibly before the quest existed), so this catches the unlock on a later visit.
static void house_refresh_viktor_shop(void)
{
    int64_t obj;
    int iter;

    if (script_global_var_get(VIKTOR_RUNES_VAR) != VIKTOR_RUNES_DONE) {
        return;
    }
    if (!obj_inst_first(&obj, &iter)) {
        return;
    }
    do {
        if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC
            && obj_field_int32_get(obj, OBJ_F_DESCRIPTION) == VIKTOR_DESCRIPTION
            && obj_field_int32_get(obj, OBJ_F_CRITTER_INVENTORY_SOURCE) != VIKTOR_RUNE_SOURCE) {
            obj_field_int32_set(obj, OBJ_F_CRITTER_INVENTORY_SOURCE, VIKTOR_RUNE_SOURCE);
            sub_463E20(obj); // rebuild stock so the runes appear immediately
            break;
        }
    } while (obj_inst_next(&obj, &iter));
}

static void house_spawn_viktor(void)
{
    int64_t viktor_loc;
    int64_t viktor_obj;

    viktor_loc = location_make(HOUSE_START_X - 2, HOUSE_START_Y);
    if (!mp_object_create(BP_BASIC_MALE_NPC, viktor_loc, &viktor_obj)) {
        tig_debug_println("house_spawn_viktor: mp_object_create failed");
        return;
    }
    house_setup_viktor(viktor_obj);
}

static void house_build_room(void)
{
    int min_x = HOUSE_START_X - HOUSE_RADIUS;
    int max_x = HOUSE_START_X + HOUSE_RADIUS;
    int min_y = HOUSE_START_Y - HOUSE_RADIUS;
    int max_y = HOUSE_START_Y + HOUSE_RADIUS;
    int x;
    int y;
    int64_t chest_loc;
    int64_t chest_obj;

    // Build a solid wall border around the room. Exit is via the hotkey, so the
    // room can be fully enclosed.
    for (x = min_x; x <= max_x; x++) {
        for (y = min_y; y <= max_y; y++) {
            if (x == min_x || x == max_x || y == min_y || y == max_y) {
                int64_t wall_obj;
                mp_object_create(BP_WALL_OF_STONE, location_make(x, y), &wall_obj);
            }
        }
    }

    // Personal storage chest next to the arrival tile.
    chest_loc = location_make(HOUSE_START_X + 1, HOUSE_START_Y);
    mp_object_create(BP_CHEST_1, chest_loc, &chest_obj);

    // Viktor the runesmith stands across the room: vendor + quest giver.
    house_spawn_viktor();
}

void player_house_on_map_opened(int map_id)
{
    int64_t pc;
    int built;

    if (map_id != player_house_map_id || !house_enter_pending) {
        return;
    }
    house_enter_pending = false;

    pc = player_get_local_pc_obj();
    built = (pc != OBJ_HANDLE_NULL) ? obj_field_int32_get(pc, HOUSE_F_BUILT) : 0;

    // Build the room + chest only on the first ever visit; afterwards the map
    // data (and the chest's contents) persist inside the savegame.
    if (built == 0) {
        house_build_room();
        if (pc != OBJ_HANDLE_NULL) {
            obj_field_int32_set(pc, HOUSE_F_BUILT, 1);
        }
        house_feedback("You step into your sanctuary in the Void.");
    } else {
        house_feedback("You return to your sanctuary in the Void.");
    }

    // Unlock Viktor's rune stock if the Five Sigils quest has since been completed.
    house_refresh_viktor_shop();
}
