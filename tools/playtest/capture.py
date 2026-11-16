"""Capture the game window every <interval> seconds for <secs> seconds into
shots/<label>/<t_ms>.png (t from capture start). Used to transcribe burned-in
movie subtitles.  usage: capture.py <label> <secs> [interval]"""
import os
import sys
import time

import mss
from PIL import Image

import gamectl as g

label, secs = sys.argv[1], float(sys.argv[2])
interval = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
g.user32.SetProcessDPIAware()
hwnd, pid = g.find_window()
out = os.path.join(g.SHOTS, label)
os.makedirs(out, exist_ok=True)
t0 = time.time()
with mss.MSS() as s:
    while time.time() - t0 < secs:
        x, y, w, h = g.client_rect(hwnd)
        img = s.grab({"left": x, "top": y, "width": w, "height": h})
        im = Image.frombytes("RGB", img.size, img.rgb).resize((640, 360))
        im.save(os.path.join(out, f"{int((time.time() - t0) * 1000):06d}.png"))
        time.sleep(max(0, interval - 0.05))
print("done", out)
