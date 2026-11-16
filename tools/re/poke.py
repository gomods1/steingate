"""Write an int32 into Game.exe memory at an RVA (dev helper).
usage: poke.py <rva_hex> <value>"""
import sys

import frida

rva, val = int(sys.argv[1], 16), int(sys.argv[2])
s = frida.attach("Game.exe")
sc = s.create_script(f"""const b = Process.getModuleByName('Game.exe').base;
b.add({rva}).writeS32({val}); send(b.add({rva}).readS32());""")
sc.on("message", lambda m, d: print(m.get("payload", m)))
sc.load()
s.detach()
