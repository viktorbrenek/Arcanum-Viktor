#include "ui/item_tooltip.h"

#include <stdio.h>
#include <string.h>

#include "game/description.h"
#include "game/hrp.h"
#include "game/item.h"
#include "game/item_rarity.h"
#include "game/item_set.h"
#include "game/obj.h"
#include "game/object.h"
#include "ui/intgame.h"
#include "game/player.h"
#include "tig/color.h"
#include "tig/font.h"
#include "tig/mouse.h"
#include "tig/window.h"

#define TOOLTIP_WIDTH   260
#define TOOLTIP_PADDING   6
#define TOOLTIP_GAP       3
#define TOOLTIP_DIV_H     1

static tig_font_handle_t tooltip_name_fonts[ITEM_RARITY_COUNT];
static tig_font_handle_t tooltip_body_font;
static tig_font_handle_t tooltip_gold_font;
static tig_window_handle_t tooltip_window = TIG_WINDOW_HANDLE_INVALID;

bool item_tooltip_init(void)
{
    TigFont font;
    int r;

    memset(&font, 0, sizeof(font));
    font.scale = 1.0f;

    tig_art_interface_id_create(27, 0, 0, 0, &font.art_id);
    for (r = 0; r < ITEM_RARITY_COUNT; r++) {
        font.color = item_rarity_color((ItemRarity)r);
        tig_font_create(&font, &tooltip_name_fonts[r]);
    }

    tig_art_interface_id_create(229, 0, 0, 0, &font.art_id);
    font.color = tig_color_make(210, 210, 210);
    tig_font_create(&font, &tooltip_body_font);

    font.color = tig_color_make(255, 215, 0);
    tig_font_create(&font, &tooltip_gold_font);

    return true;
}

void item_tooltip_exit(void)
{
    int r;
    item_tooltip_hide();
    for (r = 0; r < ITEM_RARITY_COUNT; r++) {
        if (tooltip_name_fonts[r] != TIG_FONT_HANDLE_INVALID) {
            tig_font_destroy(tooltip_name_fonts[r]);
            tooltip_name_fonts[r] = TIG_FONT_HANDLE_INVALID;
        }
    }
    if (tooltip_body_font != TIG_FONT_HANDLE_INVALID) {
        tig_font_destroy(tooltip_body_font);
        tooltip_body_font = TIG_FONT_HANDLE_INVALID;
    }
    if (tooltip_gold_font != TIG_FONT_HANDLE_INVALID) {
        tig_font_destroy(tooltip_gold_font);
        tooltip_gold_font = TIG_FONT_HANDLE_INVALID;
    }
}

void item_tooltip_hide(void)
{
    if (tooltip_window != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_destroy(tooltip_window);
        tooltip_window = TIG_WINDOW_HANDLE_INVALID;
    }
}

static int measure_h(tig_font_handle_t fnt, const char* text, int max_w)
{
    TigFont desc;
    memset(&desc, 0, sizeof(desc));
    desc.str = text;
    desc.width = max_w;
    tig_font_push(fnt);
    tig_font_measure(&desc);
    tig_font_pop();
    return desc.height > 0 ? desc.height : 1;
}

static void draw_div(tig_window_handle_t win, int y)
{
    TigRect r;
    r.x = TOOLTIP_PADDING;
    r.y = y;
    r.width = TOOLTIP_WIDTH - 2 * TOOLTIP_PADDING;
    r.height = TOOLTIP_DIV_H;
    tig_window_fill(win, &r, tig_color_make(70, 70, 95));
}

static void write_line(tig_window_handle_t win, tig_font_handle_t fnt,
                        const char* text, int y, int h)
{
    TigRect r;
    r.x = TOOLTIP_PADDING;
    r.y = y;
    r.width = TOOLTIP_WIDTH - 2 * TOOLTIP_PADDING;
    r.height = h;
    tig_font_push(fnt);
    tig_window_text_write(win, text, &r);
    tig_font_pop();
}

static const char* type_label(int obj_type)
{
    switch (obj_type) {
    case OBJ_TYPE_WEAPON:   return "Weapon";
    case OBJ_TYPE_ARMOR:    return "Armor";
    case OBJ_TYPE_AMMO:     return "Ammunition";
    case OBJ_TYPE_FOOD:     return "Food";
    case OBJ_TYPE_SCROLL:   return "Scroll";
    case OBJ_TYPE_KEY:      return "Key";
    case OBJ_TYPE_KEY_RING: return "Key Ring";
    case OBJ_TYPE_GOLD:     return "Gold";
    case OBJ_TYPE_WRITTEN:  return "Written";
    default:                return "Item";
    }
}

static const char* rarity_label(ItemRarity rarity)
{
    static const char* names[] = { "", "Common", "Uncommon", "Rare", "Epic", "Unique", "Cursed", "Set" };
    if ((int)rarity >= 0 && (int)rarity < ITEM_RARITY_COUNT) {
        return names[rarity];
    }
    return "";
}

void item_tooltip_show(int64_t item_obj, const char* item_name)
{
    int obj_type;
    ItemRarity rarity;
    bool identified;
    bool is_magic;

    char subtitle[128];
    char stats_buf[512];
    char affixes_buf[512];
    char set_buf[512];
    char value_buf[64];

    bool has_stats, has_affixes, has_set;
    int text_w, total_h, y;
    int name_h, subtitle_h, stats_h, affixes_h, set_h, value_h;

    TigMouseState mouse;
    int screen_w, screen_h, wx, wy;
    TigWindowData wdata;
    TigRect bg;

    tig_font_handle_t name_font;

    char name_internal[256];
    if (item_name == NULL || item_name[0] == '\0') {
        const char* base = description_get(obj_field_int32_get(item_obj, OBJ_F_DESCRIPTION));
        ItemRarity r0 = item_rarity_get(item_obj);
        ItemRarity r_display = item_rarity_infer(item_obj);
        if (base == NULL) {
            base = "Item";
        }
        if (r_display > ITEM_RARITY_COMMON && !item_rarity_is_identified(item_obj)) {
            snprintf(name_internal, sizeof(name_internal), "%s (?)", base);
        } else if (r0 > ITEM_RARITY_COMMON) {
            item_rarity_generate_name(item_obj, base, name_internal, sizeof(name_internal));
        } else {
            snprintf(name_internal, sizeof(name_internal), "%s", base);
        }
        item_name = name_internal;
        if (item_name[0] == '\0') {
            return;
        }
    }

    if (tooltip_name_fonts[0] == TIG_FONT_HANDLE_INVALID) {
        if (!item_tooltip_init()) {
            return;
        }
    }

    item_tooltip_hide();

    obj_type   = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    rarity     = item_rarity_infer(item_obj);
    identified = item_rarity_is_identified(item_obj);
    is_magic   = (rarity > ITEM_RARITY_COMMON);

    name_font = tooltip_name_fonts[(rarity >= 0 && rarity < ITEM_RARITY_COUNT) ? (int)rarity : 0];

    // Subtitle line
    if (is_magic) {
        const char* bound = (item_rarity_get(item_obj) == ITEM_RARITY_CURSED && item_rarity_is_cursed_bound(item_obj))
            ? "  [Bound]" : "";
        snprintf(subtitle, sizeof(subtitle), "%s %s%s",
            rarity_label(rarity), type_label(obj_type), bound);
    } else {
        snprintf(subtitle, sizeof(subtitle), "%s", type_label(obj_type));
    }

    // Base stats
    stats_buf[0] = '\0';
    has_stats = false;
    if (obj_type == OBJ_TYPE_WEAPON) {
        format_weapon_stats(item_obj, stats_buf);
        has_stats = stats_buf[0] != '\0';
    } else if (obj_type == OBJ_TYPE_ARMOR) {
        format_armor_stats(item_obj, stats_buf);
        has_stats = stats_buf[0] != '\0';
    }

    // Affix descriptions
    affixes_buf[0] = '\0';
    has_affixes = false;
    if (is_magic && identified) {
        item_rarity_format_tooltip_affixes(item_obj, affixes_buf, sizeof(affixes_buf));
        has_affixes = affixes_buf[0] != '\0';
    }

    // Set info
    set_buf[0] = '\0';
    has_set = false;
    if (rarity == ITEM_RARITY_SET) {
        SetId sid = item_set_get(item_obj);
        if (sid != SET_NONE) {
            int64_t pc = player_get_local_pc_obj();
            int equipped = (pc != OBJ_HANDLE_NULL) ? item_set_count_equipped(pc, sid) : 0;
            int active_tier = (pc != OBJ_HANDLE_NULL) ? item_set_active_tier(pc, sid) : 0;
            int pos = 0;
            pos += snprintf(set_buf + pos, sizeof(set_buf) - pos,
                "%s  (%d/3 equipped)", item_set_name(sid), equipped);
            for (int t = 1; t <= 2; t++) {
                const char* bstr = item_set_tier_bonus_str(sid, t);
                if (bstr && bstr[0] != '\0') {
                    const char* active_mark = (active_tier >= t) ? " *" : "";
                    pos += snprintf(set_buf + pos, sizeof(set_buf) - pos,
                        "\n%s%s", bstr, active_mark);
                }
            }
            has_set = set_buf[0] != '\0';
        }
    }

    // Value
    snprintf(value_buf, sizeof(value_buf), "Value: %d gp", item_worth(item_obj));

    // --- Measure heights ---
    text_w = TOOLTIP_WIDTH - 2 * TOOLTIP_PADDING;

    name_h     = measure_h(name_font,         item_name,   text_w);
    subtitle_h = measure_h(tooltip_body_font,  subtitle,   text_w);
    stats_h    = has_stats   ? measure_h(tooltip_body_font, stats_buf,   text_w) : 0;
    affixes_h  = has_affixes ? measure_h(tooltip_body_font, affixes_buf, text_w) : 0;
    set_h      = has_set     ? measure_h(tooltip_name_fonts[rarity], set_buf, text_w) : 0;
    value_h    = measure_h(tooltip_body_font, value_buf, text_w);

    total_h = TOOLTIP_PADDING * 2
            + name_h     + TOOLTIP_GAP
            + subtitle_h + TOOLTIP_GAP
            + TOOLTIP_DIV_H + TOOLTIP_GAP;

    if (has_stats)   total_h += stats_h   + TOOLTIP_GAP + TOOLTIP_DIV_H + TOOLTIP_GAP;
    if (has_affixes) total_h += affixes_h + TOOLTIP_GAP + TOOLTIP_DIV_H + TOOLTIP_GAP;
    if (has_set)     total_h += set_h     + TOOLTIP_GAP + TOOLTIP_DIV_H + TOOLTIP_GAP;

    total_h += value_h;

    if (total_h < 40) {
        total_h = 40;
    }

    // --- Position near cursor ---
    tig_mouse_get_state(&mouse);
    screen_w = hrp_iso_window_width_get();
    screen_h = hrp_iso_window_height_get();

    wx = mouse.x + 18;
    wy = mouse.y + 18;
    if (wx + TOOLTIP_WIDTH > screen_w) {
        wx = mouse.x - TOOLTIP_WIDTH - 6;
    }
    if (wy + total_h > screen_h) {
        wy = screen_h - total_h - 6;
    }
    if (wx < 0) wx = 0;
    if (wy < 0) wy = 0;

    // --- Create window ---
    memset(&wdata, 0, sizeof(wdata));
    wdata.flags          = TIG_WINDOW_ALWAYS_ON_TOP;
    wdata.rect.x         = wx;
    wdata.rect.y         = wy;
    wdata.rect.width     = TOOLTIP_WIDTH;
    wdata.rect.height    = total_h;
    wdata.background_color = tig_color_make(12, 12, 18);

    if (tig_window_create(&wdata, &tooltip_window) != TIG_OK) {
        tooltip_window = TIG_WINDOW_HANDLE_INVALID;
        return;
    }

    // Background + border
    bg.x = 0;
    bg.y = 0;
    bg.width  = TOOLTIP_WIDTH;
    bg.height = total_h;
    tig_window_fill(tooltip_window, &bg, tig_color_make(12, 12, 18));
    tig_window_box(tooltip_window, &bg, tig_color_make(80, 80, 110));

    // --- Draw content ---
    y = TOOLTIP_PADDING;

    write_line(tooltip_window, name_font, item_name, y, name_h);
    y += name_h + TOOLTIP_GAP;

    write_line(tooltip_window, tooltip_body_font, subtitle, y, subtitle_h);
    y += subtitle_h + TOOLTIP_GAP;

    draw_div(tooltip_window, y);
    y += TOOLTIP_DIV_H + TOOLTIP_GAP;

    if (has_stats) {
        write_line(tooltip_window, tooltip_body_font, stats_buf, y, stats_h);
        y += stats_h + TOOLTIP_GAP;
        draw_div(tooltip_window, y);
        y += TOOLTIP_DIV_H + TOOLTIP_GAP;
    }

    if (has_affixes) {
        write_line(tooltip_window, tooltip_body_font, affixes_buf, y, affixes_h);
        y += affixes_h + TOOLTIP_GAP;
        draw_div(tooltip_window, y);
        y += TOOLTIP_DIV_H + TOOLTIP_GAP;
    }

    if (has_set) {
        write_line(tooltip_window, tooltip_name_fonts[ITEM_RARITY_SET], set_buf, y, set_h);
        y += set_h + TOOLTIP_GAP;
        draw_div(tooltip_window, y);
        y += TOOLTIP_DIV_H + TOOLTIP_GAP;
    }

    write_line(tooltip_window, tooltip_body_font, value_buf, y, value_h);
}
