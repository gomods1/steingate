"""Move the Save/Load screen cursor to a slot on page 1 and press Enter.
Slots 1-4 are the left column, 5-8 the right (cursor RVA 0x1261734; Right
on the right column would turn the page, so only Left/Right between columns).
usage: slot.py <slot 1-8> [confirm]   (confirm: also press Enter on the dialog)"""
import subprocess
import sys

import frida

CTL = __file__.replace("slot.py", "gamectl.py")
target = int(sys.argv[1]) - 1
s = frida.attach("Game.exe")
sc = s.create_script("""const b = Process.getModuleByName('Game.exe').base;
rpc.exports = { r(o) { return b.add(o).readS32(); } };""")
sc.load()
cur = lambda: sc.exports_sync.r(0x1261734)


def key(k, wait=600):
    subprocess.run([sys.executable, CTL, "keys", k, f"wait:{wait}"], capture_output=True)


for _ in range(12):
    c = cur()
    if c == target:
        break
    if c // 4 != target // 4:
        key("right" if c // 4 < target // 4 else "left")
    else:
        key("down" if c % 4 < target % 4 else "up")
ok = cur() == target
s.detach()
if not ok:
    raise SystemExit(f"cursor at {cur()} not {target}")
key("enter", 2500)
if len(sys.argv) > 2:
    key("enter", 2500)
print("slot", target + 1)
