"""Type a number on the in-game dial pad (phone screen 23) by moving the
cursor (ScrWork 6831, 3 columns: 1-9, *, 0, #) and pressing Enter.
usage: dial.py 120#"""
import subprocess
import sys

import frida

KEYS = "123456789*0#"
CTL = __file__.replace("dial.py", "gamectl.py")
s = frida.attach("Game.exe")
sc = s.create_script("""const b = Process.getModuleByName('Game.exe').base;
rpc.exports = { w(i) { return b.add(0x2303238).readPointer().add(i * 4).readS32(); } };""")
sc.load()
cur = lambda: sc.exports_sync.w(6831)


def key(k):
    subprocess.run([sys.executable, CTL, "keys", k, "wait:450"], capture_output=True)


for ch in sys.argv[1]:
    target = KEYS.index(ch)
    for _ in range(12):
        c = cur()
        if c == target:
            break
        if c // 3 != target // 3:
            key("down" if c // 3 < target // 3 else "up")
        else:
            key("right" if c % 3 < target % 3 else "left")
    key("enter")
    key("wait:300") if False else None
s.detach()
print("dialed", sys.argv[1])
