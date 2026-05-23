#include "game/invensource.h"

#include <stdio.h>

#include "game/descriptions.h"
#include "game/mes.h"
#include "game/obj_private.h"
#include "game/proto.h"
#include "tig/debug.h"

#define MAX_INVEN_SOURCE_SET_NAME 70

typedef struct NamedInvenSourceSet {
    /* 0000 */ char name[MAX_INVEN_SOURCE_SET_NAME];
    /* 0048 */ InvenSourceSet set;
} NamedInvenSourceSet;

static bool parse_set_data(mes_file_handle_t invensource_mes_file, mes_file_handle_t invensourcebuy_mes_file);
static bool parse_invensource_entry(MesFileEntry* mes_file_entry, char* str);
static bool parse_invensourcebuy_entry(MesFileEntry* mes_file_entry, char* str);
static void show_error(const char* msg);
static void invensource_inject_orb_entries(void);
static void invensource_inject_unarmed_entries(void);

/**
 * Path to the inventory source message file.
 *
 * 0x5B64A8
 */
static const char* invensource_mes_file_name = "Rules\\InvenSource.mes";

/**
 * Path to the buy inventory source message file.
 *
 * 0x5B64AC
 */
static const char* invensourcebuy_mes_file_name = "Rules\\InvenSourceBuy.mes";

/**
 * Inventory source sets.
 *
 * 0x5FC520
 */
static NamedInvenSourceSet* invensource_sets;

/**
 * The number of items in `invensource_sets` array.
 *
 * 0x5FC524
 */
static int invensource_num_sets;

/**
 * Flag indicating whether the invensource system is in editor mode.
 *
 * 0x5FC528
 */
static bool invensource_editor;

/**
 * Buffer for storing error messages.
 *
 * 0x5FC52C
 */
static char invensource_error[256];

/**
 * 0x5FC62C
 */
static bool dword_5FC62C;

/**
 * Flag indicating whether the inventory source system is initialized.
 *
 * 0x5FC630
 */
static bool invensource_initialized;

/**
 * Flag indicating whether the invensourcebuy message file was loaded
 * successfully.
 *
 * 0x5FC634
 */
static bool invensource_have_buy;

/**
 * Called when the game is initialized.
 *
 * 0x4BF390
 */
bool invensource_init(GameInitInfo* init_info)
{
    mes_file_handle_t invensource_mes_file;
    mes_file_handle_t invensourcebuy_mes_file;

    if (!invensource_initialized) {
        invensource_editor = init_info->editor;

        // Load inventory source message file (required).
        if (!mes_load(invensource_mes_file_name, &invensource_mes_file)) {
            sprintf(invensource_error, "Can't load message file [%s].", invensource_mes_file_name);
            show_error(invensource_error);
            return false;
        }

        // Load buy inventory source message file (optional, but triggers error
        // message in the editor).
        if (mes_load(invensourcebuy_mes_file_name, &invensourcebuy_mes_file)) {
            invensource_have_buy = true;
        } else {
            invensource_have_buy = false;
            sprintf(invensource_error, "Can't load message file [%s].", invensourcebuy_mes_file_name);
            show_error(invensource_error);
        }

        // Retrieve the number of entries in the primary message file.
        invensource_num_sets = mes_entries_count(invensource_mes_file);
        if (invensource_num_sets < 1) {
            sprintf(invensource_error, "No sets to parse in [%s].", invensource_mes_file_name);
            mes_unload(invensource_mes_file);
            mes_unload(invensourcebuy_mes_file);
            return false;
        }

        invensource_sets = MALLOC(sizeof(*invensource_sets) * invensource_num_sets);

        // Parse the message files into the sets array.
        if (!parse_set_data(invensource_mes_file, invensourcebuy_mes_file)) {
            FREE(invensource_sets); // FIX: Memory leak.
            mes_unload(invensource_mes_file);
            mes_unload(invensourcebuy_mes_file);
            return false;
        }

        // Clean up parser state.
        mes_unload(invensource_mes_file);
        mes_unload(invensourcebuy_mes_file);
        invensource_have_buy = false;

        invensource_inject_orb_entries();
        invensource_inject_unarmed_entries();

        invensource_initialized = true;
    }

    return true;
}

/**
 * Called when the game shuts down.
 *
 * 0x4BF510
 */
void invensource_exit(void)
{
    if (invensource_initialized) {
        FREE(invensource_sets);
        invensource_initialized = false;
    }
}

/**
 * Retrieves an inventory source set by ID.
 *
 * 0x4BF5D0
 */
void invensource_get_id_list(int id, InvenSourceSet* set)
{
    if (invensource_initialized) {
        // TODO: Since `id` is 1-based, `id < invensource_num_sets` looks wrong.
        if (id > 0 && id < invensource_num_sets) {
            *set = invensource_sets[id - 1].set;
            return;
        }

        tig_debug_printf("Range error in invensource_get_id_list, set: %d\n", id);
    }

    // Return an empty set for invalid cases.
    set->cnt = 0;
    set->buy_cnt = 0;
}

/**
 * Parses inventory source data from message files into the global sets array.
 *
 * 0x4BF640
 */
bool parse_set_data(mes_file_handle_t invensource_mes_file, mes_file_handle_t invensourcebuy_mes_file)
{
    MesFileEntry mes_file_entry1;
    MesFileEntry mes_file_entry2;
    char str[MAX_STRING];
    int index;
    NamedInvenSourceSet* named_set;

    index = 0;
    mes_file_entry1.num = 0;

    // Iterate through all entries in the primary message file.
    while (mes_find_next(invensource_mes_file, &mes_file_entry1)) {
        // Check for discontinuous entry numbers.
        if (mes_file_entry1.num != ++index) {
            sprintf(invensource_error,
                "[%s] discontinuous at line: %d.  Skipping remaining sets.",
                invensource_mes_file_name,
                mes_file_entry1.num);
            show_error(invensource_error);

            // As the error above states, the remaining sets are skipped.
            return true;
        }

        named_set = &(invensource_sets[mes_file_entry1.num - 1]);

        // Parse the primary inventory source data.
        if (!parse_invensource_entry(&mes_file_entry1, str)) {
            sprintf(invensource_error,
                "Error parsing [%s] line %d.  Emptying set.",
                invensource_mes_file_name,
                mes_file_entry1.num);
            show_error(invensource_error);
            named_set->set.cnt = 0;
            named_set->set.buy_cnt = 0;

            continue;
        }

        // Parse buy data if available.
        named_set->set.buy_cnt = 0;
        if (invensource_have_buy) {
            mes_file_entry2.num = mes_file_entry1.num;
            if (mes_search(invensourcebuy_mes_file, &mes_file_entry2)) {
                mes_get_msg(invensourcebuy_mes_file, &mes_file_entry2);
                if (!parse_invensourcebuy_entry(&mes_file_entry2, str)) {
                    sprintf(invensource_error,
                        "Error parsing [%s] line %d.  Emptying set.",
                        invensourcebuy_mes_file_name,
                        mes_file_entry1.num);
                    show_error(invensource_error);

                    named_set->set.cnt = 0;
                    named_set->set.buy_cnt = 0;
                }
            }
        }
    }

    if (index != invensource_num_sets) {
        show_error("Set count is off in InvenSource, parse_set_data.");
    }

    return true;
}

/**
 * Parses a single inventory source set from a message file entry.
 *
 * 0x4BF7B0
 */
bool parse_invensource_entry(MesFileEntry* mes_file_entry, char* str)
{
    NamedInvenSourceSet* named_set;
    char* tok;
    int cnt;
    int rate;
    int basic_prototype;
    bool is_first;
    int64_t obj;

    // Check if the entry string exceeds maximum length.
    if (strlen(mes_file_entry->str) >= MAX_STRING) {
        sprintf(invensource_error,
            "Line %d in [%s] exceeds the maximum line length for message files.",
            mes_file_entry->num,
            invensource_mes_file_name);
        show_error(invensource_error);
        return false;
    }

    // Copy the entry string to the working buffer.
    strcpy(str, mes_file_entry->str);
    named_set = &(invensource_sets[mes_file_entry->num - 1]);

    // Parse the set name (before the colon).
    tok = strtok(str, ":");
    if (tok == NULL) {
        sprintf(invensource_error, "Line %d in [%s] is empty.\n", mes_file_entry->num, invensource_mes_file_name);
        show_error(invensource_error);
        return false;
    }

    // Validate the set name length.
    if (strlen(tok) >= MAX_INVEN_SOURCE_SET_NAME) {
        sprintf(invensource_error,
            "Line %d in [%s] has too long of a name.\n  Name length should be kept under %d characters.\n",
            mes_file_entry->num,
            invensource_mes_file_name,
            MAX_INVEN_SOURCE_SET_NAME);
        show_error(invensource_error);
        return false;
    }

    strcpy(named_set->name, tok);

    // Parse the first pair.
    tok = strtok(NULL, ",");
    if (tok == NULL) {
        // No data - this should not normally happen, inven source is probably
        // malformed.
        named_set->set.cnt = 0;
        named_set->set.min_coins = 0;
        named_set->set.max_coins = 0;
        return true;
    }

    cnt = 0;
    is_first = true;
    while (tok != NULL) {
        // Check for item count exceeding maximum.
        if (cnt >= INVEN_SOURCE_SET_SIZE) {
            sprintf(invensource_error,
                "Line %d in [%s] has too many items.\n  Item total should not exceed %d.\n",
                mes_file_entry->num,
                invensource_mes_file_name,
                INVEN_SOURCE_SET_SIZE);
            show_error(invensource_error);
            return false;
        }

        rate = atoi(tok);

        tok = strtok(NULL, " ");
        if (tok == NULL) {
            sprintf(invensource_error,
                "Error: Line %d in [%s] has an incomplete rate,bp pair at entry %d.\n",
                mes_file_entry->num,
                invensource_mes_file_name,
                cnt + 1);
            show_error(invensource_error);
            return false;
        }

        basic_prototype = atoi(tok);

        if (is_first) {
            // The first pair is min/max coins to generate.
            named_set->set.min_coins = rate;
            named_set->set.max_coins = basic_prototype;
            is_first = false;
        } else {
            // Validate basic prototype.
            if (!proto_is_valid(basic_prototype)) {
                sprintf(invensource_error,
                    "Error: Invalid prototype in [%s]: set %d, entry %d, basic prototype %d\n",
                    invensource_mes_file_name,
                    mes_file_entry->num,
                    cnt + 1,
                    basic_prototype);
                show_error(invensource_error);
                return false;
            }

            // Validate prototype object.
            obj = sub_4685A0(basic_prototype);
            if (!obj_handle_is_valid(obj)) {
                sprintf(invensource_error,
                    "Error: Can't get valid handle for prototype in [%s]: set %d, entry %d, basic prototype %d\n",
                    invensource_mes_file_name,
                    mes_file_entry->num,
                    cnt + 1,
                    basic_prototype);
                show_error(invensource_error);
                return false;
            }

            named_set->set.rate[cnt] = rate;
            named_set->set.basic_prototype[cnt] = basic_prototype;
            cnt++;
        }

        tok = strtok(NULL, ",");
    }

    named_set->set.cnt = cnt;
    return true;
}

/**
 * Parses a single buy inventory source set from a message file entry.
 *
 * 0x4BFAA0
 */
bool parse_invensourcebuy_entry(MesFileEntry* mes_file_entry, char* str)
{
    NamedInvenSourceSet* named_set;
    char* tok;
    int cnt;
    int basic_prototype;
    int64_t obj;

    // Check if the entry string exceeds maximum length.
    if (strlen(mes_file_entry->str) >= MAX_STRING) {
        sprintf(invensource_error,
            "Line %d in [%s] exceeds the maximum line length for message files.",
            mes_file_entry->num,
            invensourcebuy_mes_file_name);
        show_error(invensource_error);
        return false;
    }

    // Copy the entry string to the working buffer.
    strcpy(str, mes_file_entry->str);
    named_set = &(invensource_sets[mes_file_entry->num - 1]);

    // Parse the first token.
    tok = strtok(str, " ");
    if (tok == NULL) {
        // No data - this inven source won't buy anything.
        named_set->set.buy_all = false;
        named_set->set.buy_cnt = 0;
        return true;
    }

    // Special case - "ALL" keyword enables buying all items.
    if (strcmp(SDL_strupr(tok), "ALL") == 0) {
        named_set->set.buy_all = true;
        named_set->set.buy_cnt = 0;
        return true;
    }

    named_set->set.buy_all = false;

    // Parse space-separated basic prototypes.
    cnt = 0;
    while (tok != NULL) {
        // Check for item count exceeding maximum.
        if (cnt >= INVEN_SOURCE_SET_SIZE) {
            sprintf(invensource_error,
                "Line %d in [%s] has too many items.\n  Item total should not exceed %d.\n",
                mes_file_entry->num,
                invensourcebuy_mes_file_name,
                INVEN_SOURCE_SET_SIZE);
            show_error(invensource_error);
            return false;
        }

        basic_prototype = atoi(tok);

        // Validate basic prototype.
        if (!proto_is_valid(basic_prototype)) {
            sprintf(invensource_error,
                "Error: Invalid prototype in [%s]: set %d, entry %d, basic prototype %d\n",
                invensourcebuy_mes_file_name,
                mes_file_entry->num,
                cnt + 1,
                basic_prototype);
            show_error(invensource_error);
            return false;
        }

        // Validate prototype object.
        obj = sub_4685A0(basic_prototype);
        if (!obj_handle_is_valid(obj)) {
            sprintf(invensource_error,
                "Error: Can't get valid handle for prototype in [%s]: set %d, entry %d, basic prototype %d\n",
                invensourcebuy_mes_file_name,
                mes_file_entry->num,
                cnt + 1,
                basic_prototype);
            show_error(invensource_error);
            return false;
        }

        named_set->set.buy_basic_prototype[cnt++] = basic_prototype;

        tok = strtok(NULL, " ");
    }

    named_set->set.buy_cnt = cnt;
    return true;
}

/**
 * Injects orb (BP_COMPONENT_1) entries into relevant loot sets after loading.
 * Needed because data\ directory overrides do not work for files in DAT archives.
 */
void invensource_inject_orb_entries(void)
{
    static const struct {
        int set_id;
        int rate;
    } entries[] = {
        {   1,  3 }, // General Store Rural
        {   2,  5 }, // General Store City
        {   4,  5 }, // Elven Trader
        {   7,  5 }, // Inventor
        {  12,  5 }, // Smith Magical
        {  15,  8 }, // Magic General
        {  16,  8 }, // Magic Light
        {  17,  8 }, // Magic Dark
        {  18, 10 }, // Black Market
        {  48,  3 }, // Bandit 2 Sword
        {  49,  3 }, // Bandit 2 Mace
        {  50,  5 }, // Bandit 3 Sword
        {  51,  5 }, // Bandit 3 Mace
        {  56,  5 }, // T1 Low Tech Content
        {  57,  5 }, // T2 Med-Low Tech Content
        {  58,  8 }, // T3 Medium Tech Content
        {  59,  8 }, // T4 Med-High Tech Content
        {  60, 10 }, // T5 High Tech Content
        {  61,  3 }, // Officer Padded/Swd
        {  62,  3 }, // Officer Padded/Swd/FineRevolver
        {  63,  3 }, // Officer Padded/Swd/Rifle
        {  69,  8 }, // M1 Treasure Set
        {  70, 12 }, // M2 Treasure Set
        {  71, 18 }, // M3 Treasure Set
        {  94,  5 }, // Elven Trader (alt)
        {  95,  3 }, // Human Bounty Hunter
        {  96,  3 }, // Elven Bounty Hunter
        {  97,  3 }, // Dwarven Bounty Hunter
        {  98,  3 }, // Molochean Hand Leader
        {  99,  3 }, // Half Elf Molochean Hand Leader
        { 102,  8 }, // Dwarven Chests
        { 103, 10 }, // GeneralMagicTreasure
        { 104,  6 }, // GeneralTechTreasure
        { 105,  5 }, // WheelClanSmith
        { 111,  5 }, // Gypsy
        { 112,  5 }, // Multi-General Store
        { 115,  5 }, // Multi-Inventor
        { 117,  5 }, // Multi-Smith Magical
        { 118,  5 }, // Multi-Gypsy
        { 121,  8 }, // Multi-Magic General
        { 122,  8 }, // Multi-Magic Light
        { 123,  8 }, // Multi-Magic Dark
        { 0, 0 },
    };

    for (int i = 0; entries[i].set_id != 0; i++) {
        int id = entries[i].set_id;
        if (id < 1 || id > invensource_num_sets) {
            continue;
        }
        NamedInvenSourceSet* named_set = &invensource_sets[id - 1];
        int cnt = named_set->set.cnt;
        if (cnt < INVEN_SOURCE_SET_SIZE) {
            named_set->set.rate[cnt] = entries[i].rate;
            named_set->set.basic_prototype[cnt] = BP_COMPONENT_1;
            named_set->set.cnt = cnt + 1;
        }
    }
}

/**
 * Injects unarmed weapon (CE gauntlet-slot) entries into relevant loot sets.
 */
void invensource_inject_unarmed_entries(void)
{
    static const struct {
        int set_id;
        int rate;
        int bp;
    } entries[] = {
        // BP_CLAW — basic, +3 unarmed
        {   1,  3, BP_CLAW },   // General Store Rural
        {   2,  4, BP_CLAW },   // General Store City
        {  48,  2, BP_CLAW },   // Bandit 2 Sword
        {  49,  2, BP_CLAW },   // Bandit 2 Mace
        {  56,  3, BP_CLAW },   // T1 Low Tech Content
        {  57,  2, BP_CLAW },   // T2 Med-Low Tech Content
        { 112,  3, BP_CLAW },   // Multi-General Store
        // BP_BOXER — medium, +5 unarmed
        {   2,  3, BP_BOXER },  // General Store City
        {  12,  3, BP_BOXER },  // Smith Magical
        {  18,  3, BP_BOXER },  // Black Market
        {  50,  3, BP_BOXER },  // Bandit 3 Sword
        {  51,  3, BP_BOXER },  // Bandit 3 Mace
        {  57,  3, BP_BOXER },  // T2 Med-Low Tech Content
        {  58,  3, BP_BOXER },  // T3 Medium Tech Content
        {  69,  2, BP_BOXER },  // M1 Treasure Set
        { 117,  3, BP_BOXER },  // Multi-Smith Magical
        // BP_THORNFIST — tech-magic, +7 unarmed
        {  12,  3, BP_THORNFIST }, // Smith Magical
        {  15,  3, BP_THORNFIST }, // Magic General
        {  18,  3, BP_THORNFIST }, // Black Market
        {  58,  3, BP_THORNFIST }, // T3 Medium Tech Content
        {  59,  4, BP_THORNFIST }, // T4 Med-High Tech Content
        {  70,  3, BP_THORNFIST }, // M2 Treasure Set
        { 103,  3, BP_THORNFIST }, // GeneralMagicTreasure
        { 121,  3, BP_THORNFIST }, // Multi-Magic General
        // BP_STEAMCLAW — high tech, +12 unarmed
        {   7,  3, BP_STEAMCLAW }, // Inventor
        {  18,  4, BP_STEAMCLAW }, // Black Market
        {  59,  3, BP_STEAMCLAW }, // T4 Med-High Tech Content
        {  60,  5, BP_STEAMCLAW }, // T5 High Tech Content
        {  71,  4, BP_STEAMCLAW }, // M3 Treasure Set
        { 104,  5, BP_STEAMCLAW }, // GeneralTechTreasure
        { 115,  3, BP_STEAMCLAW }, // Multi-Inventor
        // BP_RUNEFIST — rune/magic, fatigue steal on hit
        {  15,  3, BP_RUNEFIST },  // Magic General
        {  16,  3, BP_RUNEFIST },  // Magic Light
        {  17,  3, BP_RUNEFIST },  // Magic Dark
        {  18,  3, BP_RUNEFIST },  // Black Market
        {  70,  3, BP_RUNEFIST },  // M2 Treasure Set
        {  71,  3, BP_RUNEFIST },  // M3 Treasure Set
        { 103,  3, BP_RUNEFIST },  // GeneralMagicTreasure
        { 121,  3, BP_RUNEFIST },  // Multi-Magic General
        { 122,  2, BP_RUNEFIST },  // Multi-Magic Light
        { 123,  2, BP_RUNEFIST },  // Multi-Magic Dark
        // BP_PINGLOVES — poison on hit, dark/assassin theme
        {  17,  3, BP_PINGLOVES }, // Magic Dark
        {  18,  4, BP_PINGLOVES }, // Black Market
        {  50,  2, BP_PINGLOVES }, // Bandit 3 Sword
        {  51,  2, BP_PINGLOVES }, // Bandit 3 Mace
        {  70,  2, BP_PINGLOVES }, // M2 Treasure Set
        {  71,  3, BP_PINGLOVES }, // M3 Treasure Set
        { 103,  3, BP_PINGLOVES }, // GeneralMagicTreasure
        { 123,  3, BP_PINGLOVES }, // Multi-Magic Dark
        { 0, 0, 0 },
    };

    for (int i = 0; entries[i].set_id != 0; i++) {
        int id = entries[i].set_id;
        if (id < 1 || id > invensource_num_sets) {
            continue;
        }
        NamedInvenSourceSet* named_set = &invensource_sets[id - 1];
        int cnt = named_set->set.cnt;
        if (cnt < INVEN_SOURCE_SET_SIZE) {
            named_set->set.rate[cnt] = entries[i].rate;
            named_set->set.basic_prototype[cnt] = entries[i].bp;
            named_set->set.cnt = cnt + 1;
        }
    }
}

/**
 * Logs (and displays - in the editor mode) an error message.
 *
 * 0x4BFCA0
 */
void show_error(const char* msg)
{
    tig_debug_println(msg);

    if (!dword_5FC62C && invensource_editor) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg, NULL);
    }
}
