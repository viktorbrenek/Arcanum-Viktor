#include "ui/ng_hud_ui.h"

#include "game/hrp.h"
#include "game/ng_plus.h"
#include "game/endgame_map.h"
#include "tig/color.h"
#include "tig/window.h"
#include "ui/intgame.h"

#include <stdio.h>

#define NG_HUD_W 64
#define NG_HUD_H 18

static tig_window_handle_t ng_hud_window = TIG_WINDOW_HANDLE_INVALID;

void ng_hud_ui_show(void)
{
    TigWindowData wd;

    if (ng_plus_get_level() <= 0) return;

    if (ng_hud_window != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_destroy(ng_hud_window);
        ng_hud_window = TIG_WINDOW_HANDLE_INVALID;
    }

    wd.rect.x = 5;
    wd.rect.y = 43;
    wd.rect.width = NG_HUD_W;
    wd.rect.height = NG_HUD_H;
    wd.flags = TIG_WINDOW_ALWAYS_ON_TOP;
    wd.background_color = tig_color_make(20, 15, 8);
    wd.message_filter = NULL;
    hrp_apply(&wd.rect, GRAVITY_LEFT | GRAVITY_TOP);

    if (tig_window_create(&wd, &ng_hud_window) != TIG_OK) return;

    ng_hud_ui_refresh();
}

void ng_hud_ui_hide(void)
{
    if (ng_hud_window != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_destroy(ng_hud_window);
        ng_hud_window = TIG_WINDOW_HANDLE_INVALID;
    }
}

void ng_hud_ui_refresh(void)
{
    TigRect full = { 0, 0, NG_HUD_W, NG_HUD_H };
    char buf[16];

    if (ng_plus_get_level() <= 0) {
        ng_hud_ui_hide();
        return;
    }

    if (ng_hud_window == TIG_WINDOW_HANDLE_INVALID) {
        ng_hud_ui_show();
        return;
    }

    tig_window_fill(ng_hud_window, &full, tig_color_make(20, 15, 8));
    tig_window_box(ng_hud_window, &full, tig_color_make(140, 110, 40));

    if (endgame_map_is_active()) {
        snprintf(buf, sizeof(buf), "Tier %d", ng_plus_get_level());
    } else {
        snprintf(buf, sizeof(buf), "NG+%d", ng_plus_get_level());
    }
    tig_font_push(intgame_morph15_gold_font);
    tig_window_text_write(ng_hud_window, buf, &full);
    tig_font_pop();
}
