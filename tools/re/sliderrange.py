"""Measure a Config slider's range: on the current row, press Right N times,
read the field, press Left 2N times, read, then restore the original value
with Right.  usage: sliderrange.py <field_rva_hex> [N]"""
import os
import subprocess
import sys

import frida

CTL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "playtest", "gamectl.py")
rva = int(sys.argv[1], 16)
n = int(sys.argv[2]) if len(sys.argv) > 2 else 60
s = frida.attach("Game.exe")
sc = s.create_script("""const base = Process.getModuleByName('Game.exe').base;
rpc.exports = { rd(off) { return base.add(off).readS32(); } };""")
sc.load()
rd = lambda: sc.exports_sync.rd(rva)


def keys(*k):
    subprocess.run([sys.executable, CTL, "keys", *k], capture_output=True)


orig = rd()
keys(*(["right", "wait:60"] * n))
hi = rd()
keys(*(["left", "wait:60"] * (2 * n)))
lo = rd()
for _ in range(200):
    if rd() >= orig:
        break
    keys("right", "wait:60")
print("orig", orig, "max", hi, "min", lo, "restored", rd())
s.detach()
