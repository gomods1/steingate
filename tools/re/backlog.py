"""Decode the backlog's laid-out glyphs (RVAs from the backlog draw code at
RVA 0x46C90): count 0x1343D04, entry->line 0x14FB470, line->glyph start
0x13D64D0, line->glyph count 0x1407210, 4 bytes per glyph at 0x138D0E8.
Hypothesis tested here: glyph = byte1 * 64 + byte0 (64-glyph font rows)."""
import os

import frida

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
charset = open(os.path.join(ROOT, "re", "sc3tools", "resources", "sghd", "charset.utf8"), encoding="utf-8").read()
s = frida.attach("Game.exe")
sc = s.create_script("""const b = Process.getModuleByName('Game.exe').base;
rpc.exports = { i32(r) { return b.add(r).readS32(); },
  bytes(r, n) { return Array.from(new Uint8Array(b.add(r).readByteArray(n))); } };""")
sc.load()
ex = sc.exports_sync
count = ex.i32(0x1343D04)
print("entries", count)
for e in range(count):
    line = ex.i32(0x14FB470 + 4 * e)
    start = ex.i32(0x13D64D0 + 4 * line)
    n = ex.i32(0x1407210 + 4 * line)
    raw = ex.bytes(0x138D0E8 + 4 * start, 4 * n)
    quads = [raw[i:i + 4] for i in range(0, len(raw), 4)]
    print(e, line, start, n, quads[:6])
    sh = ex.bytes(0x135C3A8 + 4 * start, 4 * n)
    shorts = [int.from_bytes(bytes(sh[i:i + 2]), "little", signed=True) for i in range(0, len(sh), 2)]
    print("  shorts", shorts[:24])
    gw = ex.bytes(0x1343D08 + 2 * start, 2 * n)
    gl = [int.from_bytes(bytes(gw[i:i + 2]), "little") for i in range(0, len(gw), 2)]
    print("  GLYPHS:", "".join(("<%d>" % (x & 0x7FFF)) if x & 0x8000 else (charset[x] if x < len(charset) else "?") for x in gl))
    for g in (shorts[0::2], shorts[1::2]):
        print("  as glyph:", "".join(charset[x] if 0 <= x < len(charset) else "?" for x in g)[:60])
    for a, b2 in ((0, 1), (1, 0)):
        txt = "".join(charset[q[b2] * 64 + q[a]] if q[b2] * 64 + q[a] < len(charset) else "?" for q in quads)
        print("  try", a, b2, txt[:80])
s.detach()
