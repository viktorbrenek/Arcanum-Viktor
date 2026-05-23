#include "game/ng_plus.h"

#include "game/settings.h"
#include "game/endgame_map.h"
#include "tig/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global settings — persists across saves (unlocked flag, last completed level, char transfer).
#define NG_PLUS_GLOBAL_CFG  "ng_plus.cfg"
// Per-save settings — lives in Save\Current, archived with the save.
#define NG_PLUS_SAVE_CFG    "Save\\Current\\ng_plus.cfg"

#define NG_KEY_UNLOCKED       "ng_unlocked"
#define NG_KEY_LAST_LEVEL     "ng_last_level"
#define NG_KEY_CHAR_LEVEL     "ng_char_level"
#define NG_KEY_LEVEL          "ng_level"
// Global backup of the active save's NG+ level — used as fallback if per-save cfg write fails.
#define NG_KEY_ACTIVE_LEVEL   "ng_active_level"

static Settings ng_global;
static Settings ng_save;

void ng_plus_init(void)
{
    settings_init(&ng_global, NG_PLUS_GLOBAL_CFG);
    settings_register(&ng_global, NG_KEY_UNLOCKED, "0", NULL);
    settings_register(&ng_global, NG_KEY_LAST_LEVEL, "0", NULL);
    settings_register(&ng_global, NG_KEY_CHAR_LEVEL, "0", NULL);
    settings_register(&ng_global, NG_KEY_ACTIVE_LEVEL, "0", NULL);
    settings_load(&ng_global);

    settings_init(&ng_save, NG_PLUS_SAVE_CFG);
    settings_register(&ng_save, NG_KEY_LEVEL, "0", NULL);
    settings_load(&ng_save);
}

void ng_plus_exit(void)
{
    settings_exit(&ng_global);
    settings_exit(&ng_save);
}

// Reload per-save NG+ level from Save\Current (call after save load or new game start).
// Uses raw fopen to bypass tig_find_first_file/SDL_GlobDirectory which silently
// fails on relative paths mid-session.
// File absent = old save without ng_plus.cfg → fall back to ng_active_level global
// backup (written at character creation / increment). This covers saves predating
// ng_plus_save(). New saves always have the file via ng_plus_save() in gameuilib_save.
void ng_plus_reload(void)
{
    TigFile* f;
    int level = 0;
    char buf[256];
    char* sep;

    f = tig_file_fopen(NG_PLUS_SAVE_CFG, "rt");
    if (f != NULL) {
        while (tig_file_fgets(buf, sizeof(buf), f) != NULL) {
            sep = strchr(buf, '=');
            if (sep != NULL) {
                char* end;
                *sep++ = '\0';
                end = sep + strlen(sep) - 1;
                while (end >= sep && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
                    *end-- = '\0';
                }
                if (_stricmp(buf, NG_KEY_LEVEL) == 0 && *sep != '\0') {
                    level = atoi(sep);
                    break;
                }
            }
        }
        tig_file_fclose(f);
    }

    // No fallback: old saves without ng_plus.cfg load as NG+0. A single re-save writes
    // the file into the archive and fixes them permanently. A global backup cannot
    // distinguish "old NG+0 save" from "old NG+N save" and causes cross-contamination.

    if (level < 0) level = 0;
    if (level > NG_PLUS_MAX_LEVEL) level = NG_PLUS_MAX_LEVEL;
    settings_set_value(&ng_save, NG_KEY_LEVEL, level);
}

// Flush per-save NG+ cfg to Save\Current — call before archiving the save.
// Writes directly (not via settings_save) because gamelib_mod_load() empties
// Save\Current after ng_plus_set_level() runs, clearing SETTINGS_CHANGED before
// gameuilib_save() ever calls this.
void ng_plus_save(void)
{
    TigFile* f;
    char buf[64];
    int len;

    f = tig_file_fopen(NG_PLUS_SAVE_CFG, "wt");
    if (f == NULL) return;
    len = snprintf(buf, sizeof(buf), "%s=%d\n", NG_KEY_LEVEL, ng_plus_get_level());
    tig_file_fwrite(buf, 1, len, f);
    tig_file_fclose(f);
}

int ng_plus_get_level(void)
{
    if (endgame_map_is_active()) {
        int tier = endgame_map_get_tier();
        if (tier > NG_PLUS_MAX_LEVEL) return NG_PLUS_MAX_LEVEL;
        return tier;
    }
    return 0;
}

int ng_plus_get_last_level(void)
{
    if (endgame_map_is_active()) {
        int tier = endgame_map_get_tier();
        if (tier > NG_PLUS_MAX_LEVEL) return NG_PLUS_MAX_LEVEL;
        return tier;
    }
    return 0;
}

bool ng_plus_is_unlocked(void)
{
    return settings_get_value(&ng_global, NG_KEY_UNLOCKED) != 0;
}

// Marks NG+ as globally unlocked and records the current save's level as last completed.
void ng_plus_unlock(void)
{
    settings_set_value(&ng_global, NG_KEY_UNLOCKED, 1);
    settings_set_value(&ng_global, NG_KEY_LAST_LEVEL, ng_plus_get_level());
    settings_save(&ng_global);
}

// Sets this save's NG+ level directly (used when starting a new NG+ game).
// Only updates ng_active_level global backup when level > 0 — avoids zeroing the
// backup when a new NG+0 character is created, which would corrupt a subsequent load
// of an NG+ character that relies on the fallback.
void ng_plus_set_level(int level)
{
    if (level < 0) level = 0;
    if (level > NG_PLUS_MAX_LEVEL) level = NG_PLUS_MAX_LEVEL;
    settings_set_value(&ng_save, NG_KEY_LEVEL, level);
    settings_save(&ng_save);
    if (level > 0) {
        settings_set_value(&ng_global, NG_KEY_ACTIVE_LEVEL, level);
        settings_save(&ng_global);
    }
}

// Increments this save's NG+ level (in-place upgrade during gameplay).
void ng_plus_increment(void)
{
    int level = ng_plus_get_level();
    if (level < NG_PLUS_MAX_LEVEL) {
        int new_level = level + 1;
        settings_set_value(&ng_save, NG_KEY_LEVEL, new_level);
        settings_save(&ng_save);
        settings_set_value(&ng_global, NG_KEY_ACTIVE_LEVEL, new_level);
        settings_save(&ng_global);
    }
}

void ng_plus_char_save_level(int level)
{
    if (level < 0) level = 0;
    settings_set_value(&ng_global, NG_KEY_CHAR_LEVEL, level);
    settings_save(&ng_global);
}

int ng_plus_char_get_level(void)
{
    int level = settings_get_value(&ng_global, NG_KEY_CHAR_LEVEL);
    return level > 0 ? level : 0;
}
