#ifndef GAME_HIGHRES_CONFIG_H_
#define GAME_HIGHRES_CONFIG_H_

#include <stdbool.h>

typedef struct HighResConfig {
    bool loaded;
    int width;
    int height;
    bool windowed;
    bool show_fps;
    int scroll_fps;
    int scroll_dist;
    bool logos;
    bool intro;
} HighResConfig;

void highres_config_load(void);
const HighResConfig* highres_config_get(void);

// Rewrites a single integer key in HighRes/config.ini (preserving other lines
// and comments), appending it if absent, then reloads the in-memory config.
bool highres_config_set_int(const char* key, int value);

// Rewrites Width/Height in HighRes/config.ini (preserving other lines and
// comments) and updates the in-memory config. Applies on next game restart.
bool highres_config_set_resolution(int width, int height);

#endif /* GAME_HIGHRES_CONFIG_H_ */
