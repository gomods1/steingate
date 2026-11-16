"""Map each Config row to the config field it changes: for every row press
Right, then Left, and report config-block cells that changed and came back.
The Config screen must be open. Config block: RVA 0x2303240..0x2303440
(defaults routine at RVA 0x708C0). Page RVA 0x1243878, per-page cursor
RVA 0x1243880 + 4*page.  usage: configmap.py"""
import os
import struct
import subprocess
import sys
import time

import frida

HERE = os.path.dirname(os.path.abspath(__file__))
CTL = os.path.join(HERE, "..", "playtest", "gamectl.py")
LO, HI = 0x2303240, 0x2303440
PAGES = [8, 6, 12]
SKIP = {(0, 3), (0, 4)}  # fullscreen, resolution: would change the window
ONLY = None
KEY = "right"
if len(sys.argv) > 1:  # e.g. configmap.py enter 0:0,0:1,0:2,0:7,1:4,1:5
    KEY = sys.argv[1]
    ONLY = {tuple(int(x) for x in p.split(":")) for p in sys.argv[2].split(",")}

s = frida.attach("Game.exe")
sc = s.create_script("""
const base = Process.getModuleByName('Game.exe').base;
rpc.exports = { rd(off, n) { return Array.from(new Int32Array(base.add(off).readByteArray(n * 4))); } };
""")
sc.load()
ex = sc.exports_sync


def keys(*k):
    subprocess.run([sys.executable, CTL, "keys", *k], capture_output=True)


def block():
    return ex.rd(LO, (HI - LO) // 4)


def state():
    page = ex.rd(0x1243878, 1)[0]
    return page, ex.rd(0x1243880 + 4 * page, 1)[0]


for page, count in enumerate(PAGES):
    # go to the page
    for _ in range(4):
        if state()[0] == page:
            break
        keys("e", "wait:600")
    for item in range(count):
        for _ in range(16):
            p, c = state()
            if c == item:
                break
            keys("down" if c < item else "up", "wait:250")
        if (page, item) in SKIP or (ONLY is not None and (page, item) not in ONLY):
            print(page, item, "skipped")
            continue
        a = block()
        keys(KEY, "wait:500")
        b = block()
        keys(KEY if KEY != "right" else "left", "wait:500")
        c = block()
        hits = [(LO + i * 4, a[i], b[i]) for i in range(len(a)) if a[i] == c[i] and a[i] != b[i]]
        changed = [(LO + i * 4, a[i], c[i]) for i in range(len(a)) if a[i] != c[i]]
        print(page, item, "toggle:", [(hex(h[0]), h[1], h[2]) for h in hits],
              "net-changed:", [(hex(h[0]), h[1], h[2]) for h in changed], flush=True)
s.detach()
