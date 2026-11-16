"""Find where an SC3-encoded string lives in a .data snapshot (memdiff.py snap).
usage: findsc3.py <snapshot> <text>   (text matched as the game's full-width glyphs)"""
import os
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
charset = open(os.path.join(ROOT, "re", "sc3tools", "resources", "sghd", "charset.utf8"), encoding="utf-8").read()
snap = open(os.path.join(ROOT, "re", "snaps", sys.argv[1] + ".bin"), "rb").read()
text = sys.argv[2]


def enc(s, full):
    out = bytearray()
    for ch in s:
        c = chr(ord(ch) + 0xFEE0) if full and 0x21 <= ord(ch) <= 0x7E else ("　" if full and ch == " " else ch)
        i = charset.find(c)
        if i < 0:
            i = charset.find(ch)
        if i < 0:
            raise SystemExit(f"char {ch!r} not in charset")
        out += bytes([0x80 | (i >> 8), i & 0xFF])
    return bytes(out)


for full in (True, False):
    needle = enc(text, full)
    pos = 0
    while True:
        i = snap.find(needle, pos)
        if i < 0:
            break
        print(("full" if full else "half"), hex(0x12A000 + i))
        pos = i + 1
