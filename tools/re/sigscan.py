"""Check CoZ LanguageBarrier signatures (gamedef.json) against a memory dump.

usage: sigscan.py <Game_mem.bin> <gamedef.json> <runtime_base_hex>
Prints name, match count, RVA of the selected occurrence (+offset) and, for
entries with an 'expr' of the form *ptr / **ptr / *(ptr + N), the value read.
"""
import json
import re
import struct
import sys

img = open(sys.argv[1], "rb").read()
gd = json.load(open(sys.argv[2]))
base = int(sys.argv[3], 16)
TEXT = (0x1000, 0x1000 + 0xb52ea)


def compile_pat(p):
    out = b""
    for t in p.split():
        out += b"." if t == "??" else re.escape(bytes([int(t, 16)]))
    return re.compile(out, re.S)


def u32(rva):
    return struct.unpack_from("<I", img, rva)[0]


for name, s in gd["signatures"]["game"].items():
    rx = compile_pat(s["pattern"])
    hits = [m.start() for m in rx.finditer(img, TEXT[0], TEXT[1])]
    occ = s.get("occurrence", 0)
    if len(hits) <= occ:
        print(f"{name:40s} MISSING ({len(hits)} hits)")
        continue
    rva = hits[occ] + s.get("offset", 0)
    extra = ""
    e = s.get("expr")
    if e:
        e = e.replace(" ", "")
        if e == "*ptr":
            extra = f" -> *ptr = {u32(rva):#x} (rva {u32(rva) - base:#x})"
        elif e == "**ptr":
            extra = f" -> **ptr: ptr at rva {u32(rva) - base:#x}"
        else:
            m = re.match(r"\*\(ptr\+0x([0-9A-Fa-f]+)\)(?:-0x([0-9A-Fa-f]+))?$", e)
            if m:
                v = u32(rva + int(m.group(1), 16)) - (int(m.group(2), 16) if m.group(2) else 0)
                extra = f" -> {v:#x} (rva {v - base:#x})"
            else:
                extra = f" expr {e}"
    print(f"{name:40s} hits={len(hits)} rva={rva:#x}{extra}")
