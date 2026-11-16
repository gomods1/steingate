"""Drive the in-game phone by its state (ScrWork 6801 slide, 6802 screen,
6812 menu option: 0 mail, 1 address book, 3 settings; 6821 address book row).
usage: phone.py call <row>     open the phone and call address-book row (0 = first)
       phone.py readmail       open the phone and open the newest mail (menu > mail > inbox > first row)
       phone.py close          put the phone away"""
import subprocess
import sys
import time

import frida

CTL = __file__.replace("phone.py", "gamectl.py")
s = frida.attach("Game.exe")
sc = s.create_script("""const b = Process.getModuleByName('Game.exe').base;
rpc.exports = { w(i) { return b.add(0x2303238).readPointer().add(i * 4).readS32(); } };""")
sc.load()
w = sc.exports_sync.w


def key(k, wait=500):
    subprocess.run([sys.executable, CTL, "keys", k, f"wait:{wait}"], capture_output=True)


def wait_for(pred, secs=6):
    end = time.time() + secs
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def to_menu():
    if w(6801) < 20:
        key("z", 1500)
    for _ in range(6):
        if w(6802) == 20:
            return True
        key("backspace" if w(6802) not in (0,) else "enter", 900)
    return w(6802) == 20


cmd = sys.argv[1]
if cmd == "call":
    row = int(sys.argv[2])
    if not to_menu():
        raise SystemExit(f"phone menu not reached (screen {w(6802)})")
    for _ in range(4):
        if w(6812) == 1:
            break
        key("right" if w(6812) in (0, 2) else "left", 700)
    key("enter", 1200)
    wait_for(lambda: w(6802) == 6)
    for _ in range(10):
        if w(6821) == row:
            break
        key("down" if w(6821) < row else "up", 500)
    key("enter", 1200)
    key("enter", 1500)
    print("called row", row, "screen", w(6802))
elif cmd == "readmail":
    # Phone Trigger waits ("Right. Now, where did I put my phone...") need the
    # phone opened and the new mail read before the story goes on.
    if w(6801) < 20:
        key("z", 1500)
    for _ in range(12):
        scr = w(6802)
        if scr == 5:
            break
        if scr == 20:
            for _ in range(4):
                if w(6812) == 0:
                    break
                key("left", 700)
            key("enter", 1200)
        elif scr in (0, 1, 3):
            key("enter", 1200)
        else:
            key("backspace", 900)
    print("phone screen", w(6802), "mail", w(6807))
elif cmd == "close":
    for _ in range(6):
        if w(6801) == 0:
            break
        key("backspace", 800)
    print("phone slide", w(6801))
s.detach()
