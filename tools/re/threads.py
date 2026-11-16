"""List the game's threads (module of current pc) and its dinput8 modules.
usage: threads.py <pid>"""
import sys

import frida

s = frida.attach(int(sys.argv[1]))
sc = s.create_script("""
const mods = Process.enumerateModules().filter(x => /dinput8/i.test(x.name)).map(x => [x.name, x.base.toString(), x.size, x.path]);
const th = Process.enumerateThreads().map(t => { const mm = Process.findModuleByAddress(t.context.pc); return (mm ? mm.name : '?') + ' ' + t.state; });
send({mods, th});
""")
sc.on("message", lambda m, d: print(m.get("payload", m)))
sc.load()
s.detach()
