"""Merge image descriptions (re/desc/out_*.tsv: NAME<TAB>text) into data/bg.tsv."""
import glob
import statistics

desc = {}
for f in sorted(glob.glob("re/desc/out_*.tsv")):
    for line in open(f, encoding="utf-8-sig"):
        line = line.rstrip("\r\n")
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) != 2:
            print("BAD", f, line[:80])
            continue
        desc[parts[0].strip()] = parts[1].strip()
rows = [l.rstrip("\n").split("\t") for l in open("data/bg.tsv", encoding="utf-8") if not l.startswith("#")]
missing = [r[1] for r in rows if r[1] not in desc]
print("described", len(desc), "missing", missing)
with open("data/bg.tsv", "w", encoding="utf-8", newline="\n") as f:
    f.write("# bg.mpk file id\tfile\taudio description (written from the images, 2026-10-24)\n")
    for r in rows:
        f.write(f"{r[0]}\t{r[1]}\t{desc.get(r[1], '')}\n")
words = [len(v.split()) for v in desc.values()]
print("avg words", round(statistics.mean(words), 1), "max", max(words))
