#!/usr/bin/env python3
"""Generate a minimal "open the dialog" Arcanum CE script (.scr) file.

Clicking an NPC executes its SAP_DIALOG script bytecode, which is what actually
opens the dialog window. The dialog *filename* is also derived from the script
number via an index table the engine builds at startup by scanning scr\\*.scr.
So a custom dialog NPC needs BOTH a dlg\\<num><name>.dlg AND this scr stub --
without the .scr the number never registers and clicking the NPC does nothing.

This stub is a single entry: SCT_TRUE -> SAT_DIALOG(start node 1). The dialog
it opens is resolved from the runtime script number, so one generic stub works
for any NPC; only the file *name* (its number) matters.

Usage:
    python tools/make_dialog_scr.py <num> <name> [out_dir]

Example:
    python tools/make_dialog_scr.py 30711 grok \
        "D:/SteamLibrary/steamapps/common/Arcanum/Arcanum/data/scr"
    -> writes 30711grok.scr  (NPC dialog then lives in dlg\\30711grok.dlg)

On-disk format (see script.c script_file_load_hdr / script_file_load_code):
    ScriptHeader : u32 flags, u32 counters            (8)
    description[40]                                    (40)
    flags(i32), num_entries(i32), max_entries(i32)     (12)
    4-byte skip (entries-ptr placeholder)              (4)   -> 64-byte header
    num_entries x ScriptCondition (132 bytes each)
ScriptCondition(0x84): int type; u8 op_type[8]; int op_value[8]; action; els
ScriptAction(0x2C)   : int type; u8 op_type[8]; int op_value[8]
Enums: SCT_TRUE=0; SAT_DO_NOTHING=0, SAT_DIALOG=4; SVT_NUMBER=3
"""
import struct
import sys
import os

SCT_TRUE = 0
SAT_DO_NOTHING = 0
SAT_DIALOG = 4
SVT_NUMBER = 3
START_NODE = 1   # dialog node to begin at (matches the {1} greeting node)


def build_scr(description: str = "dialog stub") -> bytes:
    data = b''
    data += struct.pack('<II', 0, 0)                       # ScriptHeader: flags, counters
    data += description.encode('ascii', 'replace')[:40].ljust(40, b'\x00')
    data += struct.pack('<iii', 0, 1, 1)                   # flags, num_entries, max_entries
    data += struct.pack('<i', 0)                           # entries-ptr placeholder (skipped)
    # ScriptCondition #0 : type = SCT_TRUE
    data += struct.pack('<i', SCT_TRUE)
    data += bytes(8)                                       # cond op_type[8]
    data += struct.pack('<8i', *([0] * 8))                 # cond op_value[8]
    # action = SAT_DIALOG, op_type[0]=SVT_NUMBER, op_value[0]=START_NODE
    data += struct.pack('<i', SAT_DIALOG)
    data += bytes([SVT_NUMBER, 0, 0, 0, 0, 0, 0, 0])
    data += struct.pack('<8i', START_NODE, 0, 0, 0, 0, 0, 0, 0)
    # els = SAT_DO_NOTHING
    data += struct.pack('<i', SAT_DO_NOTHING)
    data += bytes(8)
    data += struct.pack('<8i', *([0] * 8))
    assert len(data) == 196, len(data)
    return data


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    num = int(argv[1])
    name = argv[2]
    out_dir = argv[3] if len(argv) > 3 else "."
    data = build_scr(f"{name} dialog")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, f"{num:05d}{name}.scr")
    with open(path, 'wb') as f:
        f.write(data)
    print(f"wrote {len(data)} bytes -> {path}")
    print(f"  dialog file expected at: dlg/{num:05d}{name}.dlg")
    print("  remember: fully RESTART the game (script index builds at startup)")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
