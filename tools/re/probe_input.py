"""Find how the running game reads the keyboard: count calls to the usual
user32 key APIs and to IDirectInputDevice8 GetDeviceState/GetDeviceData
(found by vtable of any device the game polls).  usage: probe_input.py [seconds]"""
import sys
import time

import frida

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 4
s = frida.attach("Game.exe")
sc = s.create_script(r"""
const counts = {};
function hook(mod, name) {
  const p = Process.getModuleByName(mod).findExportByName(name);
  if (!p) return;
  Interceptor.attach(p, { onEnter() { counts[name] = (counts[name] || 0) + 1; } });
}
for (const n of ['GetKeyboardState','GetAsyncKeyState','GetKeyState','GetRawInputData','GetRawInputBuffer'])
  hook('user32.dll', n);
const di = Process.findModuleByName('dinput8.dll');
send({dinput8: di ? di.path : null, mods: Process.enumerateModules().map(m => m.name)});
rpc.exports = { counts() { return counts; } };
""")
sc.on("message", lambda m, d: print(m.get("payload", m)))
sc.load()
time.sleep(secs)
print(sc.exports_sync.counts())
s.detach()
