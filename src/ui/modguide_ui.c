#include "ui/modguide_ui.h"

#include <stdio.h>

#include "game/hrp.h"
#include "game/ng_plus.h"
#include "tig/color.h"
#include "tig/font.h"
#include "tig/message.h"
#include "tig/rect.h"
#include "tig/window.h"

#define GUIDE_W  480
#define GUIDE_H  620
#define GUIDE_ML 14
#define GUIDE_LH 18
#define GUIDE_SH 19

static bool modguide_ui_open = false;
static tig_window_handle_t modguide_ui_window = TIG_WINDOW_HANDLE_INVALID;

static tig_font_handle_t guide_font_title;
static tig_font_handle_t guide_font_section;
static tig_font_handle_t guide_font_body;
static tig_font_handle_t guide_font_magic;
static tig_font_handle_t guide_font_rare;
static tig_font_handle_t guide_font_unique;

static bool modguide_ui_message_filter(TigMessage* msg);
static void modguide_ui_draw(void);

static void make_font(tig_font_handle_t* out, TigFontFlags flags,
    int art_num, int r, int g, int b)
{
    TigFont font;
    tig_art_interface_id_create(art_num, 0, 0, 0, &font.art_id);
    font.flags = flags;
    font.str = NULL;
    font.color = tig_color_make(r, g, b);
    font.field_10 = 0;
    font.strike_through_color = 0;
    tig_font_create(&font, out);
}

void modguide_ui_init(void)
{
    make_font(&guide_font_title,   TIG_FONT_CENTERED, 27,  255, 210,  80);
    make_font(&guide_font_section, 0,                229,  210, 165,  55);
    make_font(&guide_font_body,    0,                229,  195, 190, 180);
    make_font(&guide_font_magic,   0,                229,  100, 180, 255);
    make_font(&guide_font_rare,    0,                229,  190, 100, 255);
    make_font(&guide_font_unique,  0,                229,  255, 175,  50);
}

void modguide_ui_exit(void)
{
    if (modguide_ui_open) {
        tig_window_destroy(modguide_ui_window);
        modguide_ui_open = false;
    }
    tig_font_destroy(guide_font_title);
    tig_font_destroy(guide_font_section);
    tig_font_destroy(guide_font_body);
    tig_font_destroy(guide_font_magic);
    tig_font_destroy(guide_font_rare);
    tig_font_destroy(guide_font_unique);
}

void modguide_ui_toggle(void)
{
    TigWindowData wd;

    if (modguide_ui_open) {
        tig_window_destroy(modguide_ui_window);
        modguide_ui_open = false;
        return;
    }

    wd.rect.x = 0;
    wd.rect.y = 0;
    wd.rect.width = GUIDE_W;
    wd.rect.height = GUIDE_H;
    wd.flags = TIG_WINDOW_ALWAYS_ON_TOP | TIG_WINDOW_MESSAGE_FILTER;
    wd.background_color = tig_color_make(20, 18, 14);
    wd.message_filter = modguide_ui_message_filter;
    hrp_apply(&wd.rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

    if (tig_window_create(&wd, &modguide_ui_window) != TIG_OK) {
        return;
    }

    modguide_ui_draw();
    modguide_ui_open = true;
}

static void tline(TigRect* r, tig_font_handle_t fh, const char* text)
{
    tig_font_push(fh);
    tig_window_text_write(modguide_ui_window, text, r);
    tig_font_pop();
    r->y += r->height;
}

static void hsep(TigRect* r, tig_color_t col)
{
    TigLine ln;
    ln.x1 = GUIDE_ML; ln.y1 = r->y + 2;
    ln.x2 = GUIDE_W - GUIDE_ML; ln.y2 = r->y + 2;
    tig_window_line(modguide_ui_window, &ln, col);
    r->y += 8;
}

static void modguide_ui_draw(void)
{
    TigRect full = { 0, 0, GUIDE_W, GUIDE_H };
    TigRect r;

    tig_window_fill(modguide_ui_window, &full, tig_color_make(20, 18, 14));
    tig_window_box(modguide_ui_window, &full, tig_color_make(175, 135, 50));
    TigRect inner = { 2, 2, GUIDE_W - 4, GUIDE_H - 4 };
    tig_window_box(modguide_ui_window, &inner, tig_color_make(90, 70, 25));

    r.x = 0; r.y = 10; r.width = GUIDE_W; r.height = 22;
    tline(&r, guide_font_title, "ARPG MOD GUIDE");

    r.x = GUIDE_ML; r.width = GUIDE_W - GUIDE_ML * 2;
    hsep(&r, tig_color_make(140, 110, 40));

    // --- ITEM RARITY ---
    r.height = GUIDE_SH;
    tline(&r, guide_font_section, "ITEM RARITY");
    r.height = GUIDE_LH;
    tline(&r, guide_font_body,   "White  = Common   - no special properties");
    tline(&r, guide_font_magic,  "Blue   = Magic    - 1-2 random affixes");
    tline(&r, guide_font_rare,   "Purple = Rare     - 2-3 random affixes");
    tline(&r, guide_font_unique, "Gold   = Unique   - fixed powerful affixes");

    r.y += 2;
    hsep(&r, tig_color_make(75, 60, 22));

    // --- MONSTER RARITY ---
    r.height = GUIDE_SH;
    tline(&r, guide_font_section, "MONSTER RARITY");
    r.height = GUIDE_LH;
    tline(&r, guide_font_magic,  "Blue   = Magic  (30%) - 1 bonus, 50% chance 1 orb drop");
    tline(&r, guide_font_rare,   "Purple = Rare  (15%) - 2 bonuses, always drops 1 orb");
    tline(&r, guide_font_unique, "Gold   = Unique  (3%) - 3 bonuses, always drops 2 orbs");
    tline(&r, guide_font_body,   "Hover name shows active bonuses (e.g. 'Enraged Wolf')");

    r.y += 2;
    hsep(&r, tig_color_make(75, 60, 22));

    // --- MONSTER BONUSES ---
    r.height = GUIDE_SH;
    tline(&r, guide_font_section, "MONSTER BONUSES");
    r.height = GUIDE_LH;
    tline(&r, guide_font_body, "Enraged (+8 dmg)   Armored (+15 AC)   Swift (+3 spd)");
    tline(&r, guide_font_body, "Brutish (+4 STR)   Wary (+4 PER)   Dexterous (+4 DEX)");
    tline(&r, guide_font_body, "Robust (+5 CON)   Determined (+4 WIL)   Cunning (+4 INT)");
    tline(&r, guide_font_body, "Regenerating (+5 heal rate)");

    r.y += 2;
    hsep(&r, tig_color_make(75, 60, 22));

    // --- CRAFTING ORBS ---
    r.height = GUIDE_SH;
    tline(&r, guide_font_section, "CRAFTING ORBS");
    r.height = GUIDE_LH;
    tline(&r, guide_font_body, "Annulment   - remove all affixes from item");
    tline(&r, guide_font_body, "Ascension   - add one random affix to item");
    tline(&r, guide_font_body, "Cleansing   - remove one random affix from item");
    tline(&r, guide_font_body, "Reforging   - reroll all affixes on item");
    tline(&r, guide_font_body, "Augmentation, Corruption, Entropy, Awakening - more types");
    tline(&r, guide_font_body, "Found in: rare+ monsters, chests, merchants");

    r.y += 2;
    hsep(&r, tig_color_make(75, 60, 22));

    // --- ITEM SETS ---
    r.height = GUIDE_SH;
    tline(&r, guide_font_section, "ITEM SETS");
    r.height = GUIDE_LH;
    tline(&r, guide_font_unique, "Sets = groups of themed unique items with synergy bonuses");
    tline(&r, guide_font_body,   "Equipping 2+ pieces from same set grants partial bonuses");

    r.y += 3;
    hsep(&r, tig_color_make(140, 110, 40));

    // --- NEW GAME+ STATUS ---
    {
        char ng_buf[96];
        int ng_level = ng_plus_get_level();
        int ng_last  = ng_plus_get_last_level();
        if (ng_plus_is_unlocked() && ng_level > 0) {
            snprintf(ng_buf, sizeof(ng_buf),
                "New Game+: NG+%d active  (Ctrl+F11 to increment)", ng_level);
        } else if (ng_plus_is_unlocked()) {
            snprintf(ng_buf, sizeof(ng_buf),
                "New Game+: unlocked (last NG+%d - start new game to continue)", ng_last);
        } else {
            snprintf(ng_buf, sizeof(ng_buf),
                "New Game+: locked (beat the game to unlock)");
        }
        r.height = GUIDE_LH;
        tline(&r, guide_font_body, ng_buf);
    }

    r.y += 3;
    hsep(&r, tig_color_make(140, 110, 40));

    r.height = GUIDE_LH;
    r.x = 0; r.width = GUIDE_W;
    tig_font_push(guide_font_body);
    tig_window_text_write(modguide_ui_window, "Press F9 or Escape to close", &r);
    tig_font_pop();
}

static bool modguide_ui_message_filter(TigMessage* msg)
{
    if (msg->type == TIG_MESSAGE_KEYBOARD && !msg->data.keyboard.pressed) {
        tig_window_destroy(modguide_ui_window);
        modguide_ui_open = false;
        return true;
    }
    if (msg->type == TIG_MESSAGE_MOUSE
        && msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP) {
        tig_window_destroy(modguide_ui_window);
        modguide_ui_open = false;
        return true;
    }
    return true;
}
