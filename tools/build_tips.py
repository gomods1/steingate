"""Build data/tips.tsv (tip id -> category, name, description) from the
decompiled _TIPS script (re/dis/_TIPS.scs table at label 3, re/dis/_TIPS.sct
strings). Record layout, read from the data: 6 little-endian shorts per tip =
category, name, display name, sort key, description string ids, then a flag."""
import re

scs = open("re/dis/_TIPS.scs", encoding="utf-8").read().splitlines()
strings = {}
for line in open("re/dis/_TIPS.sct", encoding="utf-8"):
    m = re.match(r"(\d+):(.*)", line.rstrip("\n"))
    if m:
        strings[int(m.group(1))] = m.group(2)


def clean(s):
    s = re.sub(r"\\c:\d+;", "", s)
    s = re.sub(r"\\[a-z]+;", " ", s)
    s = "".join(chr(ord(c) - 0xFEE0) if 0xFF01 <= ord(c) <= 0xFF5E else (" " if c == "\u3000" else c) for c in s)
    return re.sub(r"\s+", " ", s).strip()


data = bytearray()
in3 = False
for line in scs:
    if re.match(r"^\d+:", line):
        in3 = line.startswith("3:")
        continue
    if in3:
        m = re.match(r"\s+hex\s+(.*)", line)
        if m:
            data += bytes(int(b, 16) for b in m.group(1).split())
        m = re.match(r"\s+dw\s+(\d+)", line)
        if m:
            data += int(m.group(1)).to_bytes(2, "little")
shorts = [int.from_bytes(data[i:i + 2], "little") for i in range(0, len(data) - 1, 2)]
rows = []
i = 0
while i + 5 <= len(shorts):
    count = shorts[i]
    if count == 0xFFFF or count == 0 or count > 8:
        break
    cat, name, disp, key = shorts[i + 1:i + 5]
    descs = shorts[i + 5:i + 5 + count]
    text = " ".join(clean(strings.get(d, "")) for d in descs)
    rows.append((len(rows), clean(strings.get(cat, "")), clean(strings.get(name, "")), text))
    i += 5 + count
with open("data/tips.tsv", "w", encoding="utf-8", newline="\n") as f:
    f.write("# tip id (SetDic operand)\tcategory\tname\tdescription  (from _TIPS.SCX)\n")
    for r in rows:
        f.write("\t".join(str(x) for x in r) + "\n")
print(len(rows), "tips")
for r in rows[:3] + [r for r in rows if "Offline" in r[2]]:
    print(r[0], r[1], "|", r[2], "|", r[3][:60])
