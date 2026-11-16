"""Native-state finder: snapshot Game.exe's .data section and search for int32
(or byte) cells following an expected value sequence across snapshots.
  memdiff.py snap <name>                     save .data to re/snaps/<name>.bin
  memdiff.py find <v1> <name1> <v2> <name2> ...  cells equal to v_i in snapshot i
  memdiff.py findb ...                        same for single bytes
  memdiff.py read <rva> [count]               live int32 values at rva
.data: rva 0x12a000, size 0x274a19c (PE section table)."""
import os
import struct
import sys

import frida

SNAPS = os.path.join(os.path.dirname(__file__), "..", "..", "re", "snaps")
DATA_RVA, DATA_SIZE = 0x12A000, 0x274A19C

JS = """
const base = Process.getModuleByName('Game.exe').base;
rpc.exports = {
  read(off, len) { return Array.from(new Uint8Array(base.add(off).readByteArray(len))); },
};
"""


def attach():
    s = frida.attach("Game.exe")
    sc = s.create_script(JS)
    sc.load()
    return s, sc.exports_sync


def main():
    cmd = sys.argv[1]
    if cmd == "snap":
        s, ex = attach()
        os.makedirs(SNAPS, exist_ok=True)
        out = bytearray()
        step = 0x400000
        for off in range(0, DATA_SIZE, step):
            out += bytes(ex.read(DATA_RVA + off, min(step, DATA_SIZE - off)))
        open(os.path.join(SNAPS, sys.argv[2] + ".bin"), "wb").write(out)
        print("saved", sys.argv[2], len(out))
        s.detach()
    elif cmd in ("find", "findb"):
        args = sys.argv[2:]
        pairs = [(int(args[i]), args[i + 1]) for i in range(0, len(args), 2)]
        snaps = [open(os.path.join(SNAPS, n + ".bin"), "rb").read() for _, n in pairs]
        hits = []
        if cmd == "find":
            import array
            arrs = []
            for sn in snaps:
                a = array.array("i")
                a.frombytes(sn[: len(sn) // 4 * 4])
                arrs.append(a)
            first_v = pairs[0][0]
            a0 = arrs[0]
            for i in range(len(a0)):
                if a0[i] == first_v and all(arrs[k][i] == pairs[k][0] for k in range(1, len(arrs))):
                    hits.append(DATA_RVA + i * 4)
        else:
            for i in range(len(snaps[0])):
                if all(snaps[k][i] == (pairs[k][0] & 0xFF) for k in range(len(snaps))):
                    hits.append(DATA_RVA + i)
        print(len(hits), "hits")
        for h in hits[:60]:
            print(hex(h))
    elif cmd == "toggle":
        # cells equal in snapshots A and C but different in B: memdiff.py toggle A B C
        import array
        arrs = []
        for n in sys.argv[2:5]:
            a = array.array("i")
            sn = open(os.path.join(SNAPS, n + ".bin"), "rb").read()
            a.frombytes(sn[: len(sn) // 4 * 4])
            arrs.append(a)
        a, b, c = arrs
        hits = [(DATA_RVA + i * 4, a[i], b[i]) for i in range(len(a)) if a[i] == c[i] and a[i] != b[i]]
        print(len(hits), "hits")
        for h in hits[:80]:
            print(hex(h[0]), h[1], "->", h[2])
    elif cmd == "read":
        s, ex = attach()
        rva = int(sys.argv[2], 16)
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        b = bytes(ex.read(rva, n * 4))
        print([struct.unpack_from("<i", b, i * 4)[0] for i in range(n)])
        s.detach()


if __name__ == "__main__":
    main()
