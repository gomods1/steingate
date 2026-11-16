"""State probe for the running game (Frida).
  probe.py snap <name>          save flags (bit array) and ScrWork (int array) to re/snaps/<name>.json
  probe.py diff <a> <b>         print flags and ScrWork entries that differ
  probe.py flag <id> [...]      read flags
  probe.py work <id> [...]      read ScrWork entries
  probe.py watch <secs> <id..>  print ScrWork entries whenever they change ('f<id>' for flags)
Pointers: flags byte array at *(base+0x2303234), ScrWork at *(base+0x2303238)
(from getFlag at rva 0x70890 and the CoZ useOfScrWork signature)."""
import json
import os
import sys
import time

import frida

SNAPS = os.path.join(os.path.dirname(__file__), "..", "..", "re", "snaps")
NFLAG_BYTES = 0x2000
NWORK = 10000

JS = """
const base = Process.getModuleByName('Game.exe').base;
function flags() { return base.add(0x2303234).readPointer(); }
function work() { return base.add(0x2303238).readPointer(); }
rpc.exports = {
  snap(nf, nw) { return [Array.from(new Uint8Array(flags().readByteArray(nf))),
                         Array.from(new Int32Array(work().readByteArray(nw * 4)))]; },
  flag(i) { return (flags().add(i >> 3).readU8() >> (i & 7)) & 1; },
  work(i) { return work().add(i * 4).readS32(); },
};
"""


def attach():
    s = frida.attach("Game.exe")
    sc = s.create_script(JS)
    sc.load()
    return s, sc.exports_sync


def main():
    cmd = sys.argv[1]
    if cmd == "diff":
        a = json.load(open(os.path.join(SNAPS, sys.argv[2] + ".json")))
        b = json.load(open(os.path.join(SNAPS, sys.argv[3] + ".json")))
        for i, (x, y) in enumerate(zip(a["flags"], b["flags"])):
            if x != y:
                for bit in range(8):
                    if (x ^ y) >> bit & 1:
                        print(f"flag {i * 8 + bit}: {(x >> bit) & 1} -> {(y >> bit) & 1}")
        for i, (x, y) in enumerate(zip(a["work"], b["work"])):
            if x != y:
                print(f"work {i}: {x} -> {y}")
        return
    s, ex = attach()
    if cmd == "snap":
        os.makedirs(SNAPS, exist_ok=True)
        f, w = ex.snap(NFLAG_BYTES, NWORK)
        json.dump({"flags": f, "work": w}, open(os.path.join(SNAPS, sys.argv[2] + ".json"), "w"))
        print("saved", sys.argv[2])
    elif cmd == "flag":
        for i in sys.argv[2:]:
            print(i, ex.flag(int(i)))
    elif cmd == "work":
        for i in sys.argv[2:]:
            print(i, ex.work(int(i)))
    elif cmd == "range":  # probe.py range <from> <to>: non-zero ScrWork entries
        lo, hi = int(sys.argv[2]), int(sys.argv[3])
        f, w = ex.snap(1, hi + 1)
        print(" ".join(f"{i}={w[i]}" for i in range(lo, hi + 1) if w[i]))
    elif cmd == "watch":
        end = time.time() + float(sys.argv[2])
        ids = sys.argv[3:]
        last = {}
        while time.time() < end:
            for i in ids:
                v = ex.flag(int(i[1:])) if i.startswith("f") else ex.work(int(i))
                if last.get(i) != v:
                    print(f"{time.time():.2f} {i} = {v}", flush=True)
                    last[i] = v
            time.sleep(0.03)
    s.detach()


if __name__ == "__main__":
    main()
