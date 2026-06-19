#include "game/item_orb.h"

#include <stdio.h>

#include "game/damage_type.h"
#include "game/endgame_map.h"
#include "game/item.h"
#include "game/item_rarity.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/object.h"
#include "game/random.h"
#include "game/ui.h"
#include "ui/inven_ui.h"
#include "tig/art.h"

// Imbue rune effect strengths (one rune per weapon, endgame-tier reward).
// Elemental runes (fire/poison/electrical/physical) CONVERT the weapon's whole base
// damage to their element AND add this flat bonus of that type.
#define ELEM_IMBUE_AMOUNT 10
// The Void rune is deliberately weak: a small fatigue drain only, NO conversion and
// NO big number — otherwise a player trivially knocks NPCs unconscious (they stop
// reacting) and wins for free. Keep this tiny.
#define VOID_FATIGUE_AMOUNT 3

OrbType item_orb_roll_type(void)
{
    int roll = random_between(1, 100);
    if      (roll <= 35) return ORB_IDENTIFICATION;
    else if (roll <= 58) return ORB_AWAKENING;
    else if (roll <= 74) return ORB_REFORGING;
    else if (roll <= 84) return ORB_ANNULMENT;
    else if (roll <= 91) return ORB_AUGMENTATION;
    else if (roll <= 95) return ORB_CLEANSING;
    else if (roll <= 98) return ORB_ASCENSION;
    else if (roll <= 99) return ORB_ENTROPY;
    else                 return ORB_CORRUPTION;
}

const char* item_orb_display_name(OrbType type)
{
    static const char* const names[] = {
        NULL,
        "Orb of Reforging",
        "Orb of Ascension",
        "Orb of Cleansing",
        "Orb of Annulment",
        "Orb of Awakening",
        "Orb of Augmentation",
        "Orb of Corruption",
        "Orb of Entropy",
        "Scroll of Identification",
        "Map of the Void",
        "Void Portal Stone",
        "Rune of Fire",
        "Rune of Venom",
        "Rune of Storms",
        "Rune of Force",
        "Rune of the Void",
    };
    if (type > ORB_NONE && (int)type < ORB_COUNT) {
        return names[type];
    }
    return NULL;
}

const char* item_orb_description(OrbType type)
{
    static const char* const descs[] = {
        NULL,
        "Randomizes the magical affixes on an item, preserving its rarity.",
        "Upgrades a magical item to the next tier of rarity.",
        "Removes a curse from a magical item, reforging it as Rare.",
        "Removes all magical properties from an item, reducing it to Common.",
        "Awakens latent power in a mundane item, granting it Uncommon rarity.",
        "Adds one random magical property to an item with an open affix slot.",
        "Unleashes chaotic energy upon an item. The result is unpredictable.",
        "Reshuffles each magical property within its own category, preserving rarity and affix count.",
        "Reveals the hidden magical properties of an unidentified item.",
        "Opens a rift to a pocket of the Void, filled with powerful creatures and rich rewards.",
        "Right-click to collapse the rift and return to the mortal realm.",
        "Recasts a weapon's entire damage as fire and adds a potent fire bonus. One rune per weapon.",
        "Recasts a weapon's entire damage as poison and adds a potent poison bonus. One rune per weapon.",
        "Recasts a weapon's entire damage as electrical and adds a potent electrical bonus. One rune per weapon.",
        "Recasts a weapon's entire damage as raw force and adds a potent normal-damage bonus. One rune per weapon.",
        "Lends a weapon a faint Void edge that saps a little of the foe's fatigue with each strike. One rune per weapon.",
    };
    if (type > ORB_NONE && (int)type < ORB_COUNT) {
        return descs[type];
    }
    return NULL;
}

int item_orb_stack_count_get(int64_t item_obj)
{
    int cnt = obj_arrayfield_int32_get(item_obj, OBJ_F_GENERIC_PAD_IAS_1, 0);
    return cnt <= 0 ? 1 : cnt;
}

void item_orb_stack_count_set(int64_t item_obj, int count)
{
    obj_arrayfield_int32_set(item_obj, OBJ_F_GENERIC_PAD_IAS_1, 0, count);
}

int64_t item_orb_find_in_inventory(int64_t critter_obj, OrbType type)
{
    int cnt = obj_field_int32_get(critter_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (int i = 0; i < cnt; i++) {
        int64_t inv_item = obj_arrayfield_handle_get(critter_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, i);
        if (item_orb_get_type(inv_item) == type) {
            return inv_item;
        }
    }
    return OBJ_HANDLE_NULL;
}

// Maps an imbue rune type to the DamageType it etches. Returns -1 if not a rune.
static int rune_damage_type(OrbType rune)
{
    switch (rune) {
    case ORB_RUNE_FIRE:     return DAMAGE_TYPE_FIRE;
    case ORB_RUNE_POISON:   return DAMAGE_TYPE_POISON;
    case ORB_RUNE_ELECTRIC: return DAMAGE_TYPE_ELECTRICAL;
    case ORB_RUNE_PHYSICAL: return DAMAGE_TYPE_NORMAL;
    case ORB_RUNE_VOID:     return DAMAGE_TYPE_FATIGUE;
    default:                return -1;
    }
}

static const char* damage_type_word(int damage_type)
{
    switch (damage_type) {
    case DAMAGE_TYPE_FIRE:       return "fire";
    case DAMAGE_TYPE_POISON:     return "poison";
    case DAMAGE_TYPE_ELECTRICAL: return "electrical";
    case DAMAGE_TYPE_NORMAL:     return "normal";
    case DAMAGE_TYPE_FATIGUE:    return "fatigue";
    default:                     return "";
    }
}

// Flat bonus damage a rune adds. Void is tiny (anti-knockout-cheese); others endgame.
static int rune_imbue_amount(OrbType rune)
{
    return (rune == ORB_RUNE_VOID) ? VOID_FATIGUE_AMOUNT : ELEM_IMBUE_AMOUNT;
}

// Elemental runes convert the weapon's whole base damage to their type; the Void
// rune does not (it only drains a little fatigue).
static bool rune_converts(OrbType rune)
{
    return rune != ORB_RUNE_VOID && rune_damage_type(rune) >= 0;
}

// Fold all of a weapon's base damage (every DamageType slot) into one target type.
// Cheap array-field moves; permanent. Guarded by the one-rune-per-weapon cap so it
// can never run twice on the same weapon.
static void rune_convert_base_damage(int64_t weapon_obj, int target_type)
{
    int lower_sum = 0;
    int upper_sum = 0;
    for (int t = 0; t < DAMAGE_TYPE_COUNT; t++) {
        lower_sum += obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, t);
        upper_sum += obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, t);
        obj_arrayfield_int32_set(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, t, 0);
        obj_arrayfield_int32_set(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, t, 0);
    }
    obj_arrayfield_int32_set(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, target_type, lower_sum);
    obj_arrayfield_int32_set(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, target_type, upper_sum);
}

OrbType item_weapon_rune_get(int64_t weapon_obj)
{
    if (weapon_obj == OBJ_HANDLE_NULL) {
        return ORB_NONE;
    }
    if (obj_field_int32_get(weapon_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON) {
        return ORB_NONE;
    }
    int t = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_1);
    return (t >= ORB_RUNE_FIRE && t <= ORB_RUNE_VOID) ? (OrbType)t : ORB_NONE;
}

const char* item_weapon_rune_label(int64_t weapon_obj)
{
    static char buf[128];
    OrbType rune = item_weapon_rune_get(weapon_obj);
    if (rune == ORB_NONE) {
        return NULL;
    }
    int dtype = rune_damage_type(rune);
    const char* word = damage_type_word(dtype);
    if (rune_converts(rune)) {
        snprintf(buf, sizeof(buf), "Etched: %s (all damage becomes %s, +%d %s)",
            item_orb_display_name(rune), word, rune_imbue_amount(rune), word);
    } else {
        snprintf(buf, sizeof(buf), "Etched: %s (+%d %s damage)",
            item_orb_display_name(rune), rune_imbue_amount(rune), word);
    }
    return buf;
}

bool item_weapon_rune_art(int64_t weapon_obj, tig_art_id_t* out_aid)
{
    OrbType rune = item_weapon_rune_get(weapon_obj);
    if (rune == ORB_NONE) {
        return false;
    }
    return tig_art_item_id_create(
        ORB_ART_NUM_BASE + (int)rune - 1,
        TIG_ART_ITEM_DISPOSITION_INVENTORY,
        0, 0, 0,
        TIG_ART_ITEM_TYPE_GENERIC,
        0, 0,
        out_aid) == TIG_OK;
}

OrbType item_orb_get_type(int64_t item_obj)
{
    int t;

    if (item_obj == OBJ_HANDLE_NULL) {
        return ORB_NONE;
    }

    if (obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_GENERIC) {
        return ORB_NONE;
    }
    if (!((unsigned int)obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS) & OGF_IS_ORB)) {
        return ORB_NONE;
    }
    t = obj_field_int32_get(item_obj, OBJ_F_GENERIC_USAGE_BONUS);
    return (t > ORB_NONE && t < ORB_COUNT) ? (OrbType)t : ORB_NONE;
}

void item_orb_set_type(int64_t item_obj, OrbType type)
{
    tig_art_id_t inv_aid;

    unsigned int flags = (unsigned int)obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS);
    flags |= OGF_IS_ORB;
    obj_field_int32_set(item_obj, OBJ_F_GENERIC_FLAGS, (int)flags);
    obj_field_int32_set(item_obj, OBJ_F_GENERIC_USAGE_BONUS, (int)type);

    // Base worth per orb type (indexed by OrbType). The shared proto (15157) has a
    // near-zero worth, so without this every orb sold/bought at the ~2g floor
    // (item_worth clamps < 2 -> 2). Scale by power/rarity so the rune economy has weight.
    static const int orb_worth[ORB_COUNT] = {
        [ORB_NONE]           = 0,
        [ORB_REFORGING]      = 200,
        [ORB_ASCENSION]      = 400,
        [ORB_CLEANSING]      = 200,
        [ORB_ANNULMENT]      = 120,
        [ORB_AWAKENING]      = 120,
        [ORB_AUGMENTATION]   = 250,
        [ORB_CORRUPTION]     = 300,
        [ORB_ENTROPY]        = 300,
        [ORB_IDENTIFICATION] = 100,
        [ORB_MAP]            = 500,
        [ORB_EXIT_STONE]     = 50,
        // Imbue runes — premium quest-reward shop stock, priced to feel earned.
        [ORB_RUNE_FIRE]      = 350,
        [ORB_RUNE_POISON]    = 350,
        [ORB_RUNE_ELECTRIC]  = 350,
        [ORB_RUNE_PHYSICAL]  = 350,
        [ORB_RUNE_VOID]      = 400,
    };
    if (type > ORB_NONE && type < ORB_COUNT) {
        obj_field_int32_set(item_obj, OBJ_F_ITEM_WORTH, orb_worth[type]);
    }

    // OIF_CAN_USE_BOX: right-click usable. OIF_NEEDS_TARGET: only for orbs that target an item.
    int item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
    item_flags |= 0x00000004u; // OIF_CAN_USE_BOX
    if (type != ORB_MAP) {
        item_flags |= 0x00000040u; // OIF_NEEDS_TARGET
    }
    obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, item_flags);

    // Override the prototype description to prevent native UI from showing "Component 1"
    obj_field_int32_set(item_obj, OBJ_F_DESCRIPTION, 30000); // 30000 usually maps to 'Unknown' or is empty

    // Set custom inventory art (a_name_item_aid_to_fname intercepts these art IDs).
    tig_art_item_id_create(
        ORB_ART_NUM_BASE + (int)type - 1,
        TIG_ART_ITEM_DISPOSITION_INVENTORY,
        0, 0, 0,
        TIG_ART_ITEM_TYPE_GENERIC,
        0, 0,
        &inv_aid);
    obj_field_int32_set(item_obj, OBJ_F_ITEM_INV_AID, (int)inv_aid);

    // Set custom ground art (OBJ_F_AID = base, OBJ_F_CURRENT_AID = rendered; both must match).
    tig_art_id_t ground_aid;
    tig_art_item_id_create(
        ORB_ART_NUM_BASE + (int)type - 1,
        TIG_ART_ITEM_DISPOSITION_GROUND,
        0, 0, 0,
        TIG_ART_ITEM_TYPE_GENERIC,
        0, 0,
        &ground_aid);
    obj_field_int32_set(item_obj, OBJ_F_AID, (int)ground_aid);
    obj_field_int32_set(item_obj, OBJ_F_CURRENT_AID, (int)ground_aid);
}

static void orb_feedback(const char* msg)
{
    UiMessage ui_msg;
    ui_msg.type = UI_MSG_TYPE_EXCLAMATION;
    ui_msg.str = (char*)msg;
    ui_display_msg(&ui_msg);
}

static int64_t resolve_target(int64_t source_obj, int64_t target_obj)
{
    if (target_obj == source_obj || target_obj == OBJ_HANDLE_NULL) {
        int64_t hovered = inven_ui_hovered_item_get();
        if (hovered != OBJ_HANDLE_NULL) {
            return hovered;
        }
    }
    return target_obj;
}

bool item_orb_try_apply(int64_t source_obj, int64_t item_obj, int64_t target_obj)
{
    OrbType orb_type = item_orb_get_type(item_obj);
    if (orb_type == ORB_NONE) {
        return false;
    }

    if (orb_type == ORB_MAP) {
        if (inven_ui_drag_item_obj_get() == item_obj) {
            return false;
        }
        if (endgame_map_is_active()) {
            endgame_map_exit();
            return true;
        }
        if (endgame_map_enter()) {
            int cnt = item_orb_stack_count_get(item_obj);
            if (cnt > 1) {
                item_orb_stack_count_set(item_obj, cnt - 1);
            } else {
                int64_t parent_obj;
                if (item_parent(item_obj, &parent_obj)) {
                    item_remove(item_obj);
                }
                object_destroy(item_obj);
            }
        }
        return true;
    }

    if (orb_type == ORB_EXIT_STONE) {
        if (inven_ui_drag_item_obj_get() == item_obj) {
            return false;
        }
        if (endgame_map_is_active()) {
            endgame_map_exit();
            int cnt = item_orb_stack_count_get(item_obj);
            if (cnt > 1) {
                item_orb_stack_count_set(item_obj, cnt - 1);
            } else {
                int64_t parent_obj;
                if (item_parent(item_obj, &parent_obj)) {
                    item_remove(item_obj);
                }
                object_destroy(item_obj);
            }
        } else {
            orb_feedback("This stone can only be used inside the rift.");
        }
        return true;
    }

    int64_t actual_target = resolve_target(source_obj, target_obj);
    if (actual_target == OBJ_HANDLE_NULL || actual_target == source_obj) {
        orb_feedback("Hover an item in your inventory, then use the orb.");
        return true;
    }

    int target_type = obj_field_int32_get(actual_target, OBJ_F_TYPE);
    if (target_type != OBJ_TYPE_WEAPON && target_type != OBJ_TYPE_ARMOR) {
        orb_feedback("Orbs can only be applied to weapons and armor.");
        return true;
    }

    ItemRarity rarity = item_rarity_get(actual_target);
    bool consumed = false;

    switch (orb_type) {
    case ORB_REFORGING:
        if (rarity <= ITEM_RARITY_COMMON || rarity == ITEM_RARITY_UNIQUE || rarity == ITEM_RARITY_SET) {
            orb_feedback("The orb flares and fades — this item cannot be reforged.");
        } else {
            item_rarity_reforge(actual_target);
            orb_feedback("The item shimmers — its enchantments have been reforged.");
            consumed = true;
        }
        break;

    case ORB_ASCENSION:
        if (!item_rarity_ascend(actual_target)) {
            orb_feedback("The orb dims — this item cannot be ascended further.");
        } else {
            orb_feedback("A glow suffuses the item — its power has ascended.");
            consumed = true;
        }
        break;

    case ORB_CLEANSING:
        if (rarity != ITEM_RARITY_CURSED) {
            orb_feedback("The orb finds no curse to cleanse.");
        } else {
            item_rarity_cleanse(actual_target);
            orb_feedback("The dark taint lifts — the curse has been broken.");
            consumed = true;
        }
        break;

    case ORB_ANNULMENT:
        if (rarity <= ITEM_RARITY_COMMON) {
            orb_feedback("There is nothing to annul.");
        } else {
            item_rarity_annul(actual_target);
            orb_feedback("The magic drains away — the item is now mundane.");
            consumed = true;
        }
        break;

    case ORB_AWAKENING:
        if (rarity > ITEM_RARITY_COMMON) {
            orb_feedback("The orb recoils — this item already holds power.");
        } else {
            item_rarity_awaken(actual_target);
            orb_feedback("A spark ignites — latent power stirs within the item.");
            consumed = true;
        }
        break;

    case ORB_AUGMENTATION:
        if (rarity < ITEM_RARITY_UNCOMMON || rarity == ITEM_RARITY_CURSED
                || rarity == ITEM_RARITY_UNIQUE || rarity == ITEM_RARITY_SET) {
            orb_feedback("The orb finds no suitable weave to augment.");
        } else if (!item_rarity_augment(actual_target)) {
            orb_feedback("The item's enchantments are already at their limit.");
        } else {
            orb_feedback("The weave expands — a new property has taken hold.");
            consumed = true;
        }
        break;

    case ORB_CORRUPTION: {
        if (rarity < ITEM_RARITY_UNCOMMON || rarity == ITEM_RARITY_UNIQUE
                || rarity == ITEM_RARITY_SET) {
            orb_feedback("The orb recoils — this item cannot be corrupted.");
        } else {
            int outcome = item_rarity_corrupt(actual_target);
            switch (outcome) {
            case 0: orb_feedback("Chaotic power surges — the item transcends!"); break;
            case 1: orb_feedback("Dark energy seeps in — the item is cursed."); break;
            case 2: orb_feedback("Wild magic coalesces — a new enchantment takes hold."); break;
            case 3: orb_feedback("Entropy gnaws at the weave — a property is unmade."); break;
            default: orb_feedback("The orb shudders and goes dark. Nothing changes."); break;
            }
            consumed = true;
        }
        break;
    }

    case ORB_ENTROPY:
        if (rarity < ITEM_RARITY_UNCOMMON || rarity == ITEM_RARITY_CURSED
                || rarity == ITEM_RARITY_UNIQUE || rarity == ITEM_RARITY_SET) {
            orb_feedback("The orb finds no suitable enchantments to unravel.");
        } else {
            item_rarity_entropy(actual_target);
            orb_feedback("The enchantments blur and reform — familiar yet changed.");
            consumed = true;
        }
        break;

    case ORB_IDENTIFICATION:
        if (item_rarity_is_identified(actual_target)) {
            orb_feedback("This item holds no secrets left to reveal.");
        } else {
            item_rarity_identify(actual_target);
            orb_feedback("The scroll dissolves in light — the item's true nature is revealed.");
            consumed = true;
        }
        break;

    case ORB_RUNE_FIRE:
    case ORB_RUNE_POISON:
    case ORB_RUNE_ELECTRIC:
    case ORB_RUNE_PHYSICAL:
    case ORB_RUNE_VOID: {
        // Imbue runes etch permanent bonus damage of one type into a weapon. The slot
        // is OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX[damage_type], the same field rarity
        // affixes and proto magic weapons use (read in item.c damage calc via
        // item_adjust_magic — full value on neutral items, aptitude-scaled on magical).
        if (target_type != OBJ_TYPE_WEAPON) {
            orb_feedback("Runes can only be etched into weapons.");
            break;
        }
        // One rune per weapon: the weave will not hold a second sigil. This caps
        // stacking (no 50 fire runes on one blade) and gives the tooltip a single
        // socketed rune to show. Marker lives in OBJ_F_WEAPON_PAD_I_1.
        if (item_weapon_rune_get(actual_target) != ORB_NONE) {
            orb_feedback("This weapon already bears a rune — its weave will hold no other.");
            break;
        }
        const char* msg;
        switch (orb_type) {
        case ORB_RUNE_FIRE:
            msg = "Flames engulf the blade — all its damage now burns as fire.";
            break;
        case ORB_RUNE_POISON:
            msg = "A venomous sheen swallows the edge — all its damage turns to venom.";
            break;
        case ORB_RUNE_ELECTRIC:
            msg = "Lightning wreathes the weapon — all its damage crackles as storm.";
            break;
        case ORB_RUNE_PHYSICAL:
            msg = "The weapon's edge hardens — all its damage drives home as raw force.";
            break;
        case ORB_RUNE_VOID:
        default:
            msg = "A hollow chill seeps in — the weapon now saps a little of the foe's vigor.";
            break;
        }
        int dtype = rune_damage_type(orb_type);
        // Elemental runes recast the weapon's whole base damage to their type; the Void
        // rune deliberately does not (small fatigue drain only — see VOID_FATIGUE_AMOUNT).
        if (rune_converts(orb_type)) {
            rune_convert_base_damage(actual_target, dtype);
        }
        int cur = obj_arrayfield_int32_get(actual_target, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, dtype);
        obj_arrayfield_int32_set(actual_target, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, dtype, cur + rune_imbue_amount(orb_type));
        obj_field_int32_set(actual_target, OBJ_F_WEAPON_PAD_I_1, (int)orb_type); // socket marker
        orb_feedback(msg);
        consumed = true;
        break;
    }

    default:
        return false;
    }

    if (consumed) {
        int cnt = item_orb_stack_count_get(item_obj);
        if (cnt > 1) {
            item_orb_stack_count_set(item_obj, cnt - 1);
        } else {
            int64_t parent_obj;
            if (item_parent(item_obj, &parent_obj)) {
                item_remove(item_obj);
            }
            object_destroy(item_obj);
        }
    }

    return true;
}
