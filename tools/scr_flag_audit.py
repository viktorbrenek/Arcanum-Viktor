#!/usr/bin/env python3
"""
scr_flag_audit.py - Arcanum compiled-script (.scr) decoder + global-flag barrier audit.

WHY THIS EXISTS
---------------
Arcanum gates many location entrances behind a global flag: a teleporter/portal
USE script does `if (global_flag N == 1) TELEPORT else "you can't pass"`. If
nothing ever sets flag N, the location is unreachable (the Qintarra "magickal
ward" bug, 2026-06-29). This tool finds such gates and tells you whether each
gate flag actually has a setter anywhere in the game data.

KEY FACT (cost me hours - do not forget):
Arcanum sets a global flag via the **SAT_ASSIGN_NUM** action (opcode 10) whose
operand op_type is **SVT_GL_FLAG (4)** -- i.e. "assign a number into a global
flag". It does NOT generally use SAT_SET_GLOBAL_FLAG (opcode 100). An audit that
only looks at opcode 100 reports massive false positives (every reachable
location looks "broken"). Count opcode 10 with a GL_FLAG operand as a setter.
Dialogs set flags via the result-field code `gfN V` (V != 0); the result field
is the LAST brace group on the dialog line, not a fixed index.

.scr BINARY FORMAT (from CE loader, src/game/script.c script_file_load_code):
  0x00  ScriptHeader: flags(u32), counters(u32)              # 8 bytes
  0x08  description[40]
  0x30  ScriptFile.flags(u32)
  0x34  num_entries(u32)
  0x38  max_entries(u32)
  0x3C  (skip 4 - the `entries` pointer slot; holds runtime garbage on disk)
  0x40  num_entries x ScriptCondition (132 bytes each)
File size == 64 + num_entries*132 exactly.

ScriptCondition (0x84=132):  type(i32) op_type[8](u8) op_value[8](i32)
                             action(ScriptAction 0x2C) els(ScriptAction 0x2C)
ScriptAction   (0x2C=44):    type(i32) op_type[8](u8) op_value[8](i32)

NOTE: unused operand slots contain uninitialised stack/heap (live pointers like
0x77xxxxxx). Only read as many operands as an opcode needs; ignore the garbage.

USAGE
-----
  python scr_flag_audit.py audit  <DATA_DIR>
        Scan <DATA_DIR>/scr/*.scr + <DATA_DIR>/dlg/*.dlg. Report every teleporter
        script gated on a global flag, whether that flag has a setter, and (loudly)
        any gate flag that has NO setter anywhere = a real broken barrier.

  python scr_flag_audit.py decode <FILE.scr>
        Pretty-print one compiled script: every line's condition + THEN/ELSE
        actions with named opcodes and operands.

  DATA_DIR example (Windows Python wants a drive letter, not /d/):
    D:/SteamLibrary/steamapps/common/Arcanum/Arcanum/data
"""
import struct, glob, os, re, sys

SCT = ["TRUE","DAYTIME","HAS_GOLD","LOCAL_FLAG","EQ","LE","PC_QUEST_STATE",
"GLOBAL_QUEST_STATE","OBJ_HAS_BLESS","OBJ_HAS_CURSE","OBJ_MET_PC_BEFORE",
"OBJ_HAS_BAD_ASSOCIATES","OBJ_IS_POLYMORPHED","OBJ_IS_SHRUNK","OBJ_HAS_BODY_SPELL",
"OBJ_IS_INVISIBLE","OBJ_HAS_MIRROR_IMAGE","OBJ_HAS_ITEM_NAMED","OBJ_FOLLOWING_PC",
"OBJ_IS_MONSTER_OF_TYPE","OBJ_IS_NAMED","OBJ_IS_WIELDING_ITEM","OBJ_IS_DEAD",
"OBJ_HAS_MAX_FOLLOWERS","OBJ_CAN_OPEN_CONTAINER","OBJ_HAS_SURRENDERED",
"OBJ_IS_IN_DIALOG","OBJ_IS_SWITCHED_OFF","OBJ_CAN_SEE_OBJ","OBJ_CAN_HEAR_OBJ",
"OBJ_IS_INVULNERABLE","OBJ_IS_IN_COMBAT","OBJ_IS_AT_LOCATION","OBJ_HAS_REPUTATION",
"OBJ_WITHIN_RANGE","OBJ_IS_INFLUENCED_BY_SPELL","OBJ_IS_OPEN","OBJ_IS_ANIMAL",
"OBJ_IS_UNDEAD","OBJ_JILTED","RUMOR_KNOWN","RUMOR_QUELLED","OBJ_IS_BUSTED",
"GLOBAL_FLAG","CAN_OPEN_PORTAL","SECTOR_IS_BLOCKED","MONSTERGEN_DISABLED",
"IDENTIFIED","KNOWS_SPELL","MASTERED_SPELL_COLLEGE","ITEMS_ARE_BEING_REWIELDED",
"PROWLING","WAITING_FOR_LEADER"]
SVT = ["COUNTER","GL_VAR","LC_VAR","NUMBER","GL_FLAG","PC_VAR","PC_FLAG"]
SAT = ["DO_NOTHING","RETURN_AND_SKIP_DEFAULT","RETURN_AND_RUN_DEFAULT","GOTO",
"DIALOG","REMOVE_THIS_SCRIPT","CHANGE_THIS_SCRIPT_TO_SCRIPT","CALL_SCRIPT",
"SET_LOCAL_FLAG","CLEAR_LOCAL_FLAG","ASSIGN_NUM","ADD","SUBTRACT","MULTIPLY",
"DIVIDE","ASSIGN_OBJ","SET_PC_QUEST_STATE","SET_QUEST_GLOBAL_STATE","LOOP_FOR",
"LOOP_END","LOOP_BREAK","CRITTER_FOLLOW","CRITTER_DISBAND","FLOAT_LINE",
"PRINT_LINE","ADD_BLESSING","REMOVE_BLESSING","ADD_CURSE","REMOVE_CURSE",
"GET_REACTION","SET_REACTION","ADJUST_REACTION","GET_ARMOR","GET_STAT",
"GET_OBJECT_TYPE","ADJUST_GOLD","ATTACK","RANDOM","GET_SOCIAL_CLASS","GET_ORIGIN",
"TRANSFORM_ATTACHEE_INTO_BASIC_PROTOTYPE","TRANSFER_ITEM","GET_STORY_STATE",
"SET_STORY_STATE","TELEPORT","SET_DAY_STANDPOINT","SET_NIGHT_STANDPOINT",
"GET_SKILL","CAST_SPELL","MARK_MAP_LOCATION","SET_RUMOR","QUELL_RUMOR",
"CREATE_OBJECT","SET_LOCK_STATE","CALL_SCRIPT_IN","CALL_SCRIPT_AT","TOGGLE_STATE",
"TOGGLE_INVULNERABILITY","KILL","CHANGE_ART_NUM","DAMAGE","CAST_SPELL_ON",
"ACTION_PERFORM_ANIMATION","GIVE_QUEST_XP","WRITTEN_UI_START_BOOK",
"WRITTEN_UI_START_IMAGE","CREATE_ITEM","ACTION_WAIT_FOR_LEADER","DESTROY",
"ACTION_WALK_TO","GET_WEAPON_TYPE","DISTANCE_BETWEEN","ADD_REPUTATION",
"REMOVE_REPUTATION","ACTION_RUN_TO","HEAL_HP","HEAL_FATIGUE","ADD_EFFECT",
"REMOVE_EFFECT","ACTION_USE_ITEM","GET_MAGICTECH_ADJUSTMENT","CALL_SCRIPT_EX",
"PLAY_SOUND","PLAY_SOUND_ON","GET_AREA","QUEUE_NEWSPAPER","FLOAT_NEWSPAPER_HEADLINE",
"PLAY_SOUND_SCHEME","TOGGLE_OPEN_CLOSED","GET_FACTION","GET_SCROLL_DISTANCE",
"GET_MAGICTECH_ADJUSTMENT_EX","RENAME","ACTION_BECOME_PRONE","SET_WRITTEN_START",
"GET_LOCATION","GET_DAY_SINCE_STARTUP","GET_CURRENT_HOUR","GET_CURRENT_MINUTE",
"CHANGE_SCRIPT","SET_GLOBAL_FLAG","CLEAR_GLOBAL_FLAG","FADE_AND_TELEPORT","FADE",
"PLAY_SPELL_EYE_CANDY","GET_HOURS_SINCE_STARTUP","TOGGLE_SECTOR_BLOCKED",
"GET_HIT_POINTS","GET_FATIGUE_POINTS","ACTION_STOP_ATTACKING",
"TOGGLE_MONSTER_GENERATOR","GET_ARMOR_COVERAGE","GIVE_SPELL_MASTERY_IN_COLLEGE",
"UNFOG_TOWNMAP","START_WRITTEN_UI","ACTION_TRY_TO_STEAL_100_COINS",
"STOP_SPELL_EYE_CANDY","GRANT_ONE_FATE_POINT","CAST_FREE_SPELL",
"SET_PC_QUEST_UNBOTCHED","PLAY_SCRIPT_EYE_CANDY","ACTION_CAST_UNRESISTABLE_SPELL",
"ACTION_CAST_FREE_UNRESISTABLE_SPELL","TOUCH_ART","STOP_SCRIPT_EYE_CANDY",
"REMOVE_SCRIPT_CALL","DESTROY_ITEM_NAMED","TOGGLE_ITEM_INVENTORY_DISPLAY",
"HEAL_POISON","START_SCHEMATIC_UI","STOP_SPELL","QUEUE_SLIDE",
"END_GAME_AND_PLAY_SLIDES","SET_ROTATION","SET_FACTION","DRAIN_CHARGES",
"CAST_UNRESISTABLE_SPELL","ADJUST_STAT","APPLY_UNRESISTABLE_DAMAGE",
"SET_AUTOLEVEL_SCHEME","SET_DAY_STANDPOINT_EX","SET_NIGHT_STANDPOINT_EX"]

GL_FLAG = 4                       # SVT_GL_FLAG
COND_FLAG_TYPES = (4, 43, 5)      # SCT_EQ, SCT_GLOBAL_FLAG, SCT_LE
SETTER_ACTIONS = (10, 100, 101)   # ASSIGN_NUM, SET_GLOBAL_FLAG, CLEAR_GLOBAL_FLAG
TELEPORT_ACTIONS = (44, 102)      # SAT_TELEPORT, SAT_FADE_AND_TELEPORT

def _name(tbl, i): return tbl[i] if 0 <= i < len(tbl) else f"?{i}"

def parse_scr(data):
    """Return (description, [conditions]) or None if not a valid parseable script."""
    if len(data) < 64:
        return None
    _flags, num, _mx = struct.unpack_from("<III", data, 48)
    if num <= 0 or num > 500 or 64 + num * 132 > len(data):
        return None
    desc = data[8:48].split(b"\x00")[0].decode("latin1", "replace")
    out = []
    for i in range(num):
        o = 64 + i * 132
        ct = struct.unpack_from("<i", data, o)[0]
        cot = data[o + 4:o + 12]
        cov = struct.unpack_from("<8i", data, o + 12)
        acts = []
        for ao in (o + 44, o + 88):
            at = struct.unpack_from("<i", data, ao)[0]
            aot = data[ao + 4:ao + 12]
            aov = struct.unpack_from("<8i", data, ao + 12)
            acts.append((at, aot, aov))
        out.append((ct, cot, cov, acts[0], acts[1]))
    return desc, out

def _operands(ot, ov):
    parts = [f"{_name(SVT, ot[k])}={ov[k]}" for k in range(8) if not (ot[k] == 0 and ov[k] == 0)]
    return ", ".join(parts) if parts else "-"

def cmd_decode(path):
    desc, conds = parse_scr(open(path, "rb").read())
    print(f"== {os.path.basename(path)}  desc='{desc}'  lines={len(conds)}")
    for i, (ct, cot, cov, then, els) in enumerate(conds):
        print(f"\n  LINE {i}: COND {_name(SCT, ct)} [{_operands(cot, cov)}]")
        for tag, (at, aot, aov) in (("THEN", then), ("ELSE", els)):
            print(f"    {tag}: {_name(SAT, at)} [{_operands(aot, aov)}]")

def collect_flag_setters(scr_dir, dlg_dir):
    """flag -> set of source names that SET it (script ASSIGN_NUM/SET_GF, or dialog gfN V!=0)."""
    setters = {}
    for f in glob.glob(os.path.join(scr_dir, "*.scr")):
        parsed = parse_scr(open(f, "rb").read())
        if not parsed:
            continue
        nm = os.path.basename(f)
        for (_ct, _cot, _cov, then, els) in parsed[1]:
            for (at, aot, aov) in (then, els):
                if at in SETTER_ACTIONS:
                    for k in range(8):
                        if aot[k] == GL_FLAG and 0 < aov[k] < 6000:
                            setters.setdefault(aov[k], set()).add(nm)
    fre = re.compile(r"\{([^{}]*)\}")
    gfre = re.compile(r"\bgf(\d+)\s+(\d+)")
    for f in glob.glob(os.path.join(dlg_dir, "*.dlg")):
        for ln in open(f, encoding="latin1").read().splitlines():
            fs = fre.findall(ln)
            if len(fs) < 6:
                continue
            for m in gfre.finditer(fs[-1]):          # result = LAST brace group
                if int(m.group(2)) != 0:
                    setters.setdefault(int(m.group(1)), set()).add(os.path.basename(f))
    return setters

def cmd_audit(data_dir):
    scr_dir = os.path.join(data_dir, "scr")
    dlg_dir = os.path.join(data_dir, "dlg")
    setters = collect_flag_setters(scr_dir, dlg_dir)
    gates = []   # (script, flag)
    for f in glob.glob(os.path.join(scr_dir, "*.scr")):
        parsed = parse_scr(open(f, "rb").read())
        if not parsed:
            continue
        tested, teleports = set(), False
        for (ct, cot, cov, then, els) in parsed[1]:
            if ct in COND_FLAG_TYPES:
                for k in range(8):
                    if cot[k] == GL_FLAG and 0 < cov[k] < 6000:
                        tested.add(cov[k])
            for (at, _aot, _aov) in (then, els):
                if at in TELEPORT_ACTIONS:
                    teleports = True
        if teleports:
            for fl in sorted(tested):
                gates.append((os.path.basename(f), fl))
    print(f"distinct flags with a setter: {len(setters)}\n")
    print("=== teleporter scripts gated on a global flag ===")
    broken = []
    for nm, fl in gates:
        src = setters.get(fl)
        if src:
            who = ",".join(sorted(src)[:2])
            print(f"   ok      {nm:42s} gf{fl}  set by {who}")
        else:
            broken.append((nm, fl))
    print(f"\n=== !!! gate flags with NO SETTER ANYWHERE (real broken barriers) !!! ===")
    if not broken:
        print("   (none - every gated teleporter has a setter)")
    for nm, fl in broken:
        print(f"   BROKEN   {nm:42s} gf{fl}")

def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "audit":
        cmd_audit(sys.argv[2])
    elif len(sys.argv) >= 3 and sys.argv[1] == "decode":
        cmd_decode(sys.argv[2])
    else:
        print(__doc__)
        sys.exit(1)

if __name__ == "__main__":
    main()
