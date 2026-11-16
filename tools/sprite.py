"""Assemble MAGES .lay character sprites (Steam S;G: little-endian, pixel
texture coords, 30x30 tiles; format from impacto character2d.cpp).
usage: sprite.py <chara_dir> <out_dir> [name_glob]
Writes <name>_f<face>.png for every face state (body + face + first mouth frame)."""
import fnmatch
import os
import struct
import sys

from PIL import Image

T = 30


def load_lay(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<i", d, 0)[0]
    states = {}
    for i in range(n):
        sid, start, count = struct.unpack_from("<Iii", d, 8 + 12 * i)
        tiles = []
        for j in range(count):
            sx, sy, tx, ty = struct.unpack_from("<4f", d, 12 * n + 8 + (start + j) * 16)
            tiles.append((sx, sy, tx, ty))
        states[sid] = tiles
    return states


def render(atlas, states, ids):
    allt = [t for i in ids for t in states.get(i, [])]
    minx = min(t[0] for t in allt); miny = min(t[1] for t in allt)
    maxx = max(t[0] for t in allt) + T; maxy = max(t[1] for t in allt) + T
    out = Image.new("RGBA", (int(maxx - minx), int(maxy - miny)), (0, 0, 0, 0))
    for i in ids:
        for sx, sy, tx, ty in states.get(i, []):
            tile = atlas.crop((int(tx), int(ty), int(tx) + T, int(ty) + T))
            out.alpha_composite(tile, (int(sx - minx), int(sy - miny)))
    return out


def main():
    src, dst = sys.argv[1], sys.argv[2]
    pat = sys.argv[3] if len(sys.argv) > 3 else "*"
    os.makedirs(dst, exist_ok=True)
    for f in sorted(os.listdir(src)):
        if not f.lower().endswith(".png") or not fnmatch.fnmatch(f.upper(), pat.upper() + ".PNG"):
            continue
        name = f[:-4]
        lay = os.path.join(src, name + "_.lay")
        if not os.path.exists(lay):
            continue
        atlas = Image.open(os.path.join(src, f)).convert("RGBA")
        states = load_lay(lay)
        faces = sorted(s & 0xFFFF for s in states if s >> 28 == 2)
        for face in faces or [0]:
            ids = [1, 0x20000000 | face, 0x40000000 | (face << 8)]
            img = render(atlas, states, ids)
            bg = Image.new("RGB", img.size, (120, 120, 130))
            bg.paste(img, (0, 0), img)
            bg.save(os.path.join(dst, f"{name}_f{face}.png"))
    print("done")


if __name__ == "__main__":
    main()
