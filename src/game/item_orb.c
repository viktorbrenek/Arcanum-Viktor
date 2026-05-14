#include "game/item_orb.h"

#include "game/item.h"
#include "game/item_rarity.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/object.h"
#include "game/ui.h"
#include "ui/inven_ui.h"
#include "tig/art.h"

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

    // Add flags so it can be right-clicked and targeted
    int item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
    item_flags |= 0x00000004u | 0x00000040u; // OIF_CAN_USE_BOX | OIF_NEEDS_TARGET
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

    // Set custom ground art
    tig_art_id_t ground_aid;
    tig_art_item_id_create(
        ORB_ART_NUM_BASE + (int)type - 1,
        TIG_ART_ITEM_DISPOSITION_GROUND,
        0, 0, 0,
        TIG_ART_ITEM_TYPE_GENERIC,
        0, 0,
        &ground_aid);
    obj_field_int32_set(item_obj, OBJ_F_AID, (int)ground_aid);
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

    default:
        return false;
    }

    if (consumed) {
        int64_t parent_obj;
        if (item_parent(item_obj, &parent_obj)) {
            item_remove(item_obj);
        }
        object_destroy(item_obj);
    }

    return true;
}
