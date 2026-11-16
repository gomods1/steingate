"""Find code that references an absolute data address (rva), in the dump.
usage: xref.py <rva_hex> [base_hex]  -> prints instruction addresses (rva) and disassembly."""
import struct
import sys

import capstone

img = open(__file__.replace("tools\\re\\xref.py", "re\\Game_mem.bin"), "rb").read()
rva = int(sys.argv[1], 16)
base = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0xAC0000
needle = struct.pack("<I", base + rva)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
TEXT_END = 0x1000 + 0xB52EA
pos = 0x1000
while True:
    i = img.find(needle, pos, TEXT_END)
    if i < 0:
        break
    pos = i + 1
    # try to decode an instruction that contains the operand: start up to 8 bytes before
    for back in range(1, 9):
        start = i - back
        ins = next(md.disasm(img[start:start + 16], base + start), None)
        if ins and start + ins.size > i + 3 and start <= i:
            print(f"{start:#08x}: {ins.mnemonic} {ins.op_str}")
            break
