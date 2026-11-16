"""Step through the in-game System menu (cursor ScrWork 3338) and save a sheet
of the left column for each position, to read the item labels.
usage: sysmenu_labels.py <count>"""
import os
import subprocess
import sys

import frida
from PIL import Image

import gamectl as g

n = int(sys.argv[1]) if len(sys.argv) > 1 else 12
s = frida.attach("Game.exe")
sc = s.create_script("""const b = Process.getModuleByName('Game.exe').base;
rpc.exports = { w(i) { return b.add(0x2303238).readPointer().add(i * 4).readS32(); } };""")
sc.load()
crops = []
for _ in range(n):
    cur = sc.exports_sync.w(3338)
    out = subprocess.run([sys.executable, g.__file__, "shot", f"sys{cur}"], capture_output=True, text=True).stdout.strip().splitlines()[-1]
    im = Image.open(out).crop((60, 100, 420, 600))
    crops.append((cur, im))
    subprocess.run([sys.executable, g.__file__, "keys", "down", "wait:700"], capture_output=True)
s.detach()
sheet = Image.new("RGB", (360 * 6, 500 * ((len(crops) + 5) // 6)))
for i, (cur, im) in enumerate(crops):
    sheet.paste(im, ((i % 6) * 360, (i // 6) * 500))
path = os.path.join(os.path.dirname(__file__), "..", "..", "re", "sysmenu_sheet.png")
sheet.save(path)
print([c for c, _ in crops], path)
