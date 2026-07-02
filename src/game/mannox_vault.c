#include "game/mannox_vault.h"

#include <string.h>

#include "game/location.h"
#include "game/mp_utils.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/script.h"
#include "game/tb.h"
#include "game/written.h"
#include "tig/debug.h"

// The vault door teleporter targets one of the MapList "Unused" placeholder
// slots (5080-5088), so the engine requests map "Unused". That interior was
// never built, so we intercept the load and open the "vault" right here.
#define MANNOX_VAULT_MAP_NAME "Unused"

// Plain spawnable blueprints (description.mes) used as the physical objects;
// each is re-identified via OBJ_F_NAME to the matching real relic.
#define BP_PLAIN_SWORD 6030  // description.mes {6030}{Sword}
#define BP_PLAIN_BOOK 14062  // description.mes {14062}{Book}

// OBJ_F_NAME (oname.mes) identities of the real relics — this is what the game
// uses to look up an object's displayed name.
#define ONAME_SWORD_OF_MANNOX 2005 // oname.mes {2005}{Sword of Mannox}
#define ONAME_JOURNAL_OF_MANNOX 5214 // oname.mes {5214}{The Journal of Saint Mannox}

// The journal's readable text lives in mes\gamebook.mes as a block of
// consecutive messages (appended after the vanilla 1000-1614 range). Reading the
// book opens the written UI on this range.
#define MANNOX_JOURNAL_MES_START 1620
#define MANNOX_JOURNAL_MES_END 1629

// Quest flag "discovered the truth about Mannox / found his journal". Setting it
// lets the Panarii dialogs (Gunther / Alexander) recognise the discovery, and it
// doubles as the once-only guard so the vault can't be farmed.
#define GF_FOUND_MANNOX_JOURNAL 2020

bool mannox_vault_is_target(const char* map_name)
{
    return map_name != NULL && strcmp(map_name, MANNOX_VAULT_MAP_NAME) == 0;
}

// Create one relic on the ground at `loc` and rename it to the real Mannox item.
// Left lying on the floor (not auto-added to the pack) so it visibly appears
// where the player is standing, for them to pick up. Returns the item handle.
static int64_t vault_spawn_relic(int blueprint, int oname, int64_t loc)
{
    int64_t item;

    if (!mp_object_create(blueprint, loc, &item)) {
        tig_debug_printf("mannox_vault: failed to create blueprint %d\n", blueprint);
        return OBJ_HANDLE_NULL;
    }

    obj_field_int32_set(item, OBJ_F_NAME, oname);
    return item;
}

void mannox_vault_give_reward(void)
{
    int64_t pc;
    int64_t loc;
    int64_t journal;

    pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL) {
        return;
    }

    // Grant only once.
    if (script_global_flag_get(GF_FOUND_MANNOX_JOURNAL) != 0) {
        tb_add(pc, TB_TYPE_WHITE, "The tomb is empty; you have already taken its relics.");
        return;
    }

    loc = obj_field_int64_get(pc, OBJ_F_LOCATION);

    // Drop both relics on the floor where the player is standing.
    vault_spawn_relic(BP_PLAIN_SWORD, ONAME_SWORD_OF_MANNOX, loc);

    journal = vault_spawn_relic(BP_PLAIN_BOOK, ONAME_JOURNAL_OF_MANNOX, loc);
    if (journal != OBJ_HANDLE_NULL) {
        // Point the book at Mannox's journal text in gamebook.mes.
        obj_field_int32_set(journal, OBJ_F_WRITTEN_SUBTYPE, WRITTEN_TYPE_BOOK);
        obj_field_int32_set(journal, OBJ_F_WRITTEN_TEXT_START_LINE, MANNOX_JOURNAL_MES_START);
        obj_field_int32_set(journal, OBJ_F_WRITTEN_TEXT_END_LINE, MANNOX_JOURNAL_MES_END);
    }

    // Mark the discovery so the Panarii quest can advance (Gunther / Alexander).
    script_global_flag_set(GF_FOUND_MANNOX_JOURNAL, 1);

    tb_add(pc, TB_TYPE_WHITE, "Within the tomb rest ancient remains. Mannox's ceremonial sword and his journal tumble to the ground at your feet.");
}
