"""Dump the decrypted Game.exe image from the running process (SteamStub keeps
.text encrypted on disk). Writes re/Game_mem.bin (image laid out at RVAs) and
prints the load base.  usage: dump_module.py [outfile]"""
import os
import sys

import frida

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), "..", "..", "re", "Game_mem.bin")

session = frida.attach("Game.exe")
script = session.create_script("""
rpc.exports = {
  info() { const m = Process.getModuleByName('Game.exe'); return [m.base.toString(), m.size]; },
  read(off, len) { const m = Process.getModuleByName('Game.exe');
    const r = []; let p = 0;
    // read page by page so an unreadable page becomes zeros
    while (p < len) { const n = Math.min(0x1000, len - p);
      try { r.push(new Uint8Array(m.base.add(off + p).readByteArray(n))); }
      catch (e) { r.push(new Uint8Array(n)); }
      p += n; }
    const all = new Uint8Array(len); let o = 0; for (const c of r) { all.set(c, o); o += c.length; }
    return Array.from(all); }
};
""")
script.load()
base, size = script.exports_sync.info()
print("base", base, "size", hex(size))
with open(out, "wb") as f:
    chunk = 0x100000
    for off in range(0, size, chunk):
        n = min(chunk, size - off)
        f.write(bytes(script.exports_sync.read(off, n)))
print("wrote", out)
session.detach()
