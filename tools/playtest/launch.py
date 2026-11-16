"""Start the game through Steam and click the Launcher's Start Game button.
Prints the Game.exe PID."""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import time

import gamectl as g

u = ctypes.windll.user32
u.SetProcessDPIAware()
os.startfile("steam://rungameid/412830")
h = 0
for _ in range(60):
    h = u.FindWindowW(None, "Steins;Gate Launcher")
    if h:
        break
    time.sleep(0.5)
if not h:
    raise SystemExit("launcher did not appear")
time.sleep(1.5)
u.SwitchToThisWindow(h, True)
time.sleep(0.5)
r = wt.RECT()
u.GetWindowRect(h, ctypes.byref(r))
u.SetCursorPos(r.left + 100, r.top + 73)  # "Start Game" (first button)
time.sleep(0.2)
for f in (2, 4):
    i = g.INPUT(type=0)
    i.u.mi = g.MOUSEINPUT(0, 0, 0, f, 0, 0)
    u.SendInput(1, ctypes.byref(i), ctypes.sizeof(g.INPUT))
    time.sleep(0.05)
for _ in range(60):
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Game.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    if "Game.exe" in out:
        print("Game.exe pid", out.split('","')[1])
        break
    time.sleep(0.5)
