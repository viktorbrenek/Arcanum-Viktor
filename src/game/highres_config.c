#include "game/highres_config.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_stdinc.h>

static void highres_config_reset(void);
static void highres_config_parse_line(char* line);
static void highres_config_trim(char** start_ptr, char** end_ptr);
static bool highres_config_parse_int(const char* str, int* value_ptr);

static HighResConfig highres_config;

void highres_config_load(void)
{
    FILE* stream;
    char line[512];

    highres_config_reset();

    stream = fopen("HighRes/config.ini", "rt");
    if (stream == NULL) {
        return;
    }

    highres_config.loaded = true;

    while (fgets(line, sizeof(line), stream) != NULL) {
        highres_config_parse_line(line);
    }

    fclose(stream);
}

const HighResConfig* highres_config_get(void)
{
    return &highres_config;
}

bool highres_config_set_int(const char* key, int value)
{
    FILE* stream;
    static char lines[128][512];
    int count = 0;
    int index;
    bool wrote = false;

    // Read the existing config (if any) into memory.
    stream = fopen("HighRes/config.ini", "rt");
    if (stream != NULL) {
        while (count < 128 && fgets(lines[count], sizeof(lines[count]), stream) != NULL) {
            count++;
        }
        fclose(stream);
    }

    stream = fopen("HighRes/config.ini", "wt");
    if (stream == NULL) {
        return false;
    }

    for (index = 0; index < count; index++) {
        char* line = lines[index];
        char* sep;
        char* comment;
        char cur_key[64];
        char* key_start;
        char* key_end;
        size_t key_len;

        sep = strchr(line, '=');
        if (sep != NULL) {
            // Extract and trim the key (text before '=').
            key_start = line;
            key_end = sep;
            highres_config_trim(&key_start, &key_end);
            key_len = (size_t)(key_end - key_start);
            if (key_len < sizeof(cur_key)) {
                memcpy(cur_key, key_start, key_len);
                cur_key[key_len] = '\0';

                if (SDL_strcasecmp(cur_key, key) == 0) {
                    // Preserve any trailing comment (including its newline).
                    comment = strstr(sep, "//");
                    if (comment != NULL) {
                        fprintf(stream, "%s = %d %s", key, value, comment);
                    } else {
                        fprintf(stream, "%s = %d\n", key, value);
                    }
                    wrote = true;
                    continue;
                }
            }
        }

        // Unchanged line - write verbatim.
        fputs(line, stream);
    }

    // Append the key if it wasn't present in the original file.
    if (!wrote) {
        fprintf(stream, "%s = %d\n", key, value);
    }

    fclose(stream);

    // Reload so the in-memory config reflects the change for option getters.
    highres_config_load();

    return true;
}

bool highres_config_set_resolution(int width, int height)
{
    bool ok = highres_config_set_int("Width", width);
    ok = highres_config_set_int("Height", height) && ok;
    return ok;
}

void highres_config_reset(void)
{
    highres_config.loaded = false;
    highres_config.width = 800;
    highres_config.height = 600;
    highres_config.windowed = false;
    highres_config.show_fps = false;
    highres_config.scroll_fps = 35;
    highres_config.scroll_dist = 10;
    highres_config.logos = true;
    highres_config.intro = true;
}

void highres_config_parse_line(char* line)
{
    char* comment;
    char* sep;
    char* key_start;
    char* key_end;
    char* value_start;
    char* value_end;
    int value;

    comment = strstr(line, "//");
    if (comment != NULL) {
        *comment = '\0';
    }

    key_start = line;
    key_end = key_start + strlen(key_start);
    highres_config_trim(&key_start, &key_end);
    *key_end = '\0';

    if (*key_start == '\0') {
        return;
    }

    sep = strchr(key_start, '=');
    if (sep == NULL) {
        return;
    }

    *sep = '\0';

    key_end = sep;
    highres_config_trim(&key_start, &key_end);
    *key_end = '\0';

    value_start = sep + 1;
    value_end = value_start + strlen(value_start);
    highres_config_trim(&value_start, &value_end);
    *value_end = '\0';

    if (!highres_config_parse_int(value_start, &value)) {
        return;
    }

    if (SDL_strcasecmp(key_start, "Width") == 0) {
        if (value >= 800) {
            highres_config.width = value;
        }
    } else if (SDL_strcasecmp(key_start, "Height") == 0) {
        if (value >= 600) {
            highres_config.height = value;
        }
    } else if (SDL_strcasecmp(key_start, "Windowed") == 0) {
        highres_config.windowed = value != 0;
    } else if (SDL_strcasecmp(key_start, "ShowFPS") == 0) {
        highres_config.show_fps = value != 0;
    } else if (SDL_strcasecmp(key_start, "ScrollFPS") == 0) {
        if (value > 0) {
            highres_config.scroll_fps = value;
        }
    } else if (SDL_strcasecmp(key_start, "ScrollDist") == 0) {
        if (value >= 0) {
            highres_config.scroll_dist = value;
        }
    } else if (SDL_strcasecmp(key_start, "Logos") == 0) {
        highres_config.logos = value != 0;
    } else if (SDL_strcasecmp(key_start, "Intro") == 0) {
        highres_config.intro = value != 0;
    }
}

void highres_config_trim(char** start_ptr, char** end_ptr)
{
    while (*start_ptr < *end_ptr && isspace((unsigned char)**start_ptr)) {
        (*start_ptr)++;
    }

    while (*end_ptr > *start_ptr && isspace((unsigned char)*((*end_ptr) - 1))) {
        (*end_ptr)--;
    }
}

bool highres_config_parse_int(const char* str, int* value_ptr)
{
    char* end;
    long value;

    if (*str == '\0') {
        return false;
    }

    value = strtol(str, &end, 10);
    if (end == str) {
        return false;
    }

    while (*end != '\0') {
        if (!isspace((unsigned char)*end)) {
            return false;
        }

        end++;
    }

    *value_ptr = (int)value;
    return true;
}
