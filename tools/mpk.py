"""MAGES MPK v2 archive lister/extractor.

Format (from CoZ sghd-patch mpkpack.py): 'MPK\\0', u16 minor, u16 major, u64 count,
entries at 0x40, 0x100 bytes each: u32 compression, u32 id, u64 offset,
u64 compressed size, u64 size, name (0xE0 bytes, NUL terminated).

usage: mpk.py list <archive>
       mpk.py extract <archive> <outdir> [glob]
"""
import fnmatch
import os
import struct
import sys
import zlib


def read_toc(f):
    magic = f.read(4)
    if magic != b"MPK\0":
        raise SystemExit("not an MPK archive")
    minor, major = struct.unpack("<HH", f.read(4))
    (count,) = struct.unpack("<Q", f.read(8))
    entries = []
    for i in range(count):
        f.seek(0x40 + i * 0x100)
        raw = f.read(0x100)
        comp, fid, off, csize, size = struct.unpack_from("<IIQQQ", raw, 0)
        name = raw[0x20:].split(b"\0", 1)[0].decode("ascii", "replace")
        entries.append((fid, name, off, csize, size, comp))
    return (major, minor), entries


def main():
    cmd, arc = sys.argv[1], sys.argv[2]
    with open(arc, "rb") as f:
        ver, entries = read_toc(f)
        if cmd == "list":
            print(f"# version {ver}, {len(entries)} entries")
            for fid, name, off, csize, size, comp in entries:
                print(f"{fid}\t{name}\t{size}\t{comp}")
            return
        outdir = sys.argv[3]
        pat = sys.argv[4] if len(sys.argv) > 4 else "*"
        os.makedirs(outdir, exist_ok=True)
        for fid, name, off, csize, size, comp in entries:
            if not fnmatch.fnmatch(name.lower(), pat.lower()):
                continue
            f.seek(off)
            data = f.read(csize)
            if comp:
                data = zlib.decompress(data)
            with open(os.path.join(outdir, name), "wb") as o:
                o.write(data)
        print("done")


if __name__ == "__main__":
    main()
