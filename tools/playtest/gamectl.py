"""Playtest harness: eyes and hands for the running game.

usage:
  gamectl.py shot <label>             screenshot of the game window -> shots/<n>_<label>.png
  gamectl.py keys <k1> [k2 ...]       send keys (names below, 'wait:ms' pauses, 'hold:k:ms')
  gamectl.py click <x> <y> [right]    click at client coords (1280x720 reference scaled to client)
  gamectl.py move <x> <y>
  gamectl.py info                     window pid, rect, foreground state
Every action first brings the game window to the front and verifies it.
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

import mss
from PIL import Image

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, "shots")

SC = {
    "esc": 0x01, "1": 0x02, "2": 0x03, "3": 0x04, "4": 0x05, "5": 0x06, "6": 0x07,
    "7": 0x08, "8": 0x09, "9": 0x0A, "0": 0x0B, "backspace": 0x0E, "tab": 0x0F,
    "q": 0x10, "w": 0x11, "e": 0x12, "r": 0x13, "t": 0x14, "y": 0x15, "u": 0x16,
    "i": 0x17, "o": 0x18, "p": 0x19, "enter": 0x1C, "ctrl": 0x1D, "a": 0x1E,
    "s": 0x1F, "d": 0x20, "f": 0x21, "g": 0x22, "h": 0x23, "j": 0x24, "k": 0x25,
    "l": 0x26, "shift": 0x2A, "z": 0x2C, "x": 0x2D, "c": 0x2E, "v": 0x2F,
    "b": 0x30, "n": 0x31, "m": 0x32, "space": 0x39, "alt": 0x38,
    "f1": 0x3B, "f2": 0x3C, "f3": 0x3D, "f4": 0x3E, "f5": 0x3F, "f6": 0x40,
    "f7": 0x41, "f8": 0x42, "f9": 0x43, "f10": 0x44, "f11": 0x57, "f12": 0x58,
    "home": (0x47, True), "up": (0x48, True), "pgup": (0x49, True),
    "left": (0x4B, True), "right": (0x4D, True), "end": (0x4F, True),
    "down": (0x50, True), "pgdn": (0x51, True), "del": (0x53, True),
    "rctrl": (0x1D, True),
}


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
                ("time", wt.DWORD), ("dwExtraInfo", ctypes.c_size_t)]


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wt.LONG), ("dy", wt.LONG), ("mouseData", wt.DWORD),
                ("dwFlags", wt.DWORD), ("time", wt.DWORD), ("dwExtraInfo", ctypes.c_size_t)]


class _U(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT), ("mi", MOUSEINPUT), ("pad", ctypes.c_byte * 32)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wt.DWORD), ("u", _U)]


def find_window():
    pids = set()
    import subprocess
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Game.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.strip('"').split('","')
        if len(parts) > 1 and parts[0].lower() == "game.exe":
            pids.add(int(parts[1]))
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            r = wt.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(r))
            if r.right > 200 or user32.IsIconic(hwnd):
                found.append((hwnd, pid.value))
        return True

    user32.EnumWindows(cb, 0)
    if not found:
        raise SystemExit("game window not found")
    return found[0]


def client_rect(hwnd):
    r = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y, r.right, r.bottom


def fg_pid():
    pid = wt.DWORD()
    user32.GetWindowThreadProcessId(user32.GetForegroundWindow(), ctypes.byref(pid))
    return pid.value


def focus(hwnd, pid):
    if fg_pid() == pid and not user32.IsIconic(hwnd):
        return
    # Attach to the foreground thread's input queue so SetForegroundWindow is
    # allowed from a background process; an Alt tap is the fallback.
    if user32.IsIconic(hwnd):
        user32.ShowWindow(hwnd, 9)
    fg = user32.GetForegroundWindow()
    fg_tid = user32.GetWindowThreadProcessId(fg, None)
    me = kernel32.GetCurrentThreadId()
    user32.AttachThreadInput(me, fg_tid, True)
    user32.BringWindowToTop(hwnd)
    user32.SetForegroundWindow(hwnd)
    user32.AttachThreadInput(me, fg_tid, False)
    time.sleep(0.15)
    for _ in range(5):
        if fg_pid() == pid:
            break
        user32.SwitchToThisWindow(hwnd, True)
        time.sleep(0.4)
    for _ in range(10):
        time.sleep(0.1)
        if fg_pid() == pid:
            return
    raise SystemExit("could not bring game window to front")


def send_key(name, hold_ms=50):
    code = SC[name]
    ext = False
    if isinstance(code, tuple):
        code, ext = code
    flags = 0x0008 | (0x0001 if ext else 0)
    down = INPUT(type=1)
    down.u.ki = KEYBDINPUT(0, code, flags, 0, 0)
    up = INPUT(type=1)
    up.u.ki = KEYBDINPUT(0, code, flags | 0x0002, 0, 0)
    user32.SendInput(1, ctypes.byref(down), ctypes.sizeof(INPUT))
    time.sleep(hold_ms / 1000)
    user32.SendInput(1, ctypes.byref(up), ctypes.sizeof(INPUT))


def key_event(name, up):
    code = SC[name]
    ext = False
    if isinstance(code, tuple):
        code, ext = code
    flags = 0x0008 | (0x0001 if ext else 0) | (0x0002 if up else 0)
    i = INPUT(type=1)
    i.u.ki = KEYBDINPUT(0, code, flags, 0, 0)
    user32.SendInput(1, ctypes.byref(i), ctypes.sizeof(INPUT))


def mouse(hwnd, x, y, button=None):
    cx, cy, w, h = client_rect(hwnd)
    sx = cx + int(x * w / 1280)
    sy = cy + int(y * h / 720)
    user32.SetCursorPos(sx, sy)
    time.sleep(0.05)
    if button:
        d, u = (0x0002, 0x0004) if button == "left" else (0x0008, 0x0010)
        for f in (d, u):
            i = INPUT(type=0)
            i.u.mi = MOUSEINPUT(0, 0, 0, f, 0, 0)
            user32.SendInput(1, ctypes.byref(i), ctypes.sizeof(INPUT))
            time.sleep(0.05)


def shot(hwnd, label):
    os.makedirs(SHOTS, exist_ok=True)
    n = len(os.listdir(SHOTS))
    x, y, w, h = client_rect(hwnd)
    with mss.mss() as s:
        img = s.grab({"left": x, "top": y, "width": w, "height": h})
    im = Image.frombytes("RGB", img.size, img.rgb)
    if im.width > 1280:
        im = im.resize((1280, int(im.height * 1280 / im.width)))
    path = os.path.join(SHOTS, f"{n:04d}_{label}.png")
    im.save(path)
    print(path)


def main():
    user32.SetProcessDPIAware()
    cmd = sys.argv[1]
    hwnd, pid = find_window()
    if cmd == "info":
        print(pid, client_rect(hwnd), user32.GetForegroundWindow() == hwnd)
        return
    focus(hwnd, pid)
    if cmd == "shot":
        shot(hwnd, sys.argv[2] if len(sys.argv) > 2 else "shot")
    elif cmd == "keys":
        for k in sys.argv[2:]:
            if k.startswith("wait:"):
                time.sleep(int(k[5:]) / 1000)
            elif k.startswith("hold:"):
                _, name, ms = k.split(":")
                send_key(name, int(ms))
            elif "+" in k:
                mods = k.split("+")
                for m in mods[:-1]:
                    key_event(m, False)
                time.sleep(0.03)
                send_key(mods[-1])
                for m in reversed(mods[:-1]):
                    key_event(m, True)
            else:
                send_key(k)
            time.sleep(0.12)
        print("sent", " ".join(sys.argv[2:]))
    elif cmd in ("click", "move"):
        btn = None
        if cmd == "click":
            btn = sys.argv[4] if len(sys.argv) > 4 else "left"
        mouse(hwnd, int(sys.argv[2]), int(sys.argv[3]), btn)
        print(cmd, sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    main()
