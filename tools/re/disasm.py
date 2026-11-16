"""Disassemble the memory dump at an RVA.  usage: disasm.py <rva_hex> [count] [base_hex]
Absolute addresses in the output are shown both raw and as RVAs (base from the dump run)."""
import re
import sys

import capstone

img = open(__file__.replace("tools\\re\\disasm.py", "re\\Game_mem.bin").replace("tools/re/disasm.py", "re/Game_mem.bin"), "rb").read()
rva = int(sys.argv[1], 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 40
base = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0xAC0000
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for ins in md.disasm(img[rva:rva + count * 16], base + rva):
    if count == 0:
        break
    count -= 1
    ops = ins.op_str

    def rel(m):
        v = int(m.group(0), 16)
        if base <= v < base + len(img):
            return f"{m.group(0)}<rva {v - base:#x}>"
        return m.group(0)

    ops = re.sub(r"0x[0-9a-f]{6,8}", rel, ops)
    print(f"{ins.address - base:#08x}: {ins.mnemonic:8s} {ops}")
