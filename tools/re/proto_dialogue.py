"""Frida prototype: log every dialogue string, BGload and CHAload the script VM runs.
Validates ScriptThreadState offsets (pc 0x14C, script buffer id 0x74),
getStringFromScript, the instruction tables and the SC3 string decoding.
usage: proto_dialogue.py <seconds>"""
import json
import os
import sys
import time

import frida

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
charset = open(os.path.join(ROOT, "re", "sc3tools", "resources", "sghd", "charset.utf8"),
               encoding="utf-8").read()

JS = r"""
const charset = CHARSET;
const base = Process.getModuleByName('Game.exe').base;
const RVA = { tblUser1: 0x934578, tblGraph: 0x932ff8, tblSystem: 0x933610, getStr: 0x62d40, sc3Eval: 0x181f0 };
const getStr = new NativeFunction(base.add(RVA.getStr), 'pointer', ['int', 'int'], 'mscdecl');
function skipExpr(p) {
  for (;;) {
    const b = p.readU8();
    if (b === 0) return p.add(1);
    if ((b & 0x80) === 0) { p = p.add(2); continue; }
    let n = 1;
    if ((b & 0x60) === 0x60) n = 5; else if ((b & 0x40) === 0x40) n = 3; else if ((b & 0x20) === 0x20) n = 2;
    p = p.add(n + 1);
  }
}
function exprConst(p) { // value of a single-literal expression, else null
  const b = p.readU8();
  if ((b & 0x80) === 0 || b === 0) return null;
  let v, n;
  if ((b & 0x60) === 0x60) { v = p.add(1).readS32(); n = 5; }
  else if ((b & 0x40) === 0x40) { v = ((b & 0x1f) << 16) | p.add(1).readU16(); v = (v << 11) >> 11; n = 3; }
  else if ((b & 0x20) === 0x20) { v = ((b & 0x1f) << 8) | p.add(1).readU8(); v = (v << 19) >> 19; n = 2; }
  else { v = (b & 0x1f); v = (v << 27) >> 27; n = 1; }
  if (p.add(n + 1).readU8() !== 0) return null;
  return v;
}
function decode(s) {
  let out = '', name = null, inName = false, nameBuf = '';
  for (let i = 0; i < 4000; i++) {
    const b = s.readU8();
    if (b === 0xFF) break;
    if (b >= 0x80) { const g = ((b & 0x7f) << 8) | s.add(1).readU8(); s = s.add(2);
      const ch = g < charset.length ? charset[g] : '?'; if (inName) nameBuf += ch; else out += ch; continue; }
    s = s.add(1);
    switch (b) {
      case 0x00: out += ' '; break;
      case 0x01: inName = true; break;
      case 0x02: inName = false; name = nameBuf; break;
      case 0x04: case 0x15: s = skipExpr(s); break;
      case 0x07: case 0x1B: s = s.add(1); break;
      case 0x0C: case 0x11: case 0x12: case 0x13: case 0x14: case 0x16: case 0x19: case 0x1A: s = s.add(2); break;
      default: break;
    }
  }
  return { name, text: out };
}
const tables = { user1: base.add(RVA.tblUser1), graph: base.add(RVA.tblGraph), system: base.add(RVA.tblSystem) };
function hookOp(tbl, op, label, fn) {
  const target = tables[tbl].add(op * 4).readPointer();
  Interceptor.attach(target, { onEnter(args) { try { fn(args[0], label); } catch (e) { send('ERR ' + label + ' ' + e); } } });
}
hookOp('graph', 0x0C, 'MesSet', (th) => {
  const pc = th.add(0x14C).readPointer();
  const sub = pc.add(2).readU8();
  let p = pc.add(3);
  const nexpr = sub === 3 ? 3 : (sub === 0 ? 1 : -1);
  if (nexpr < 0) { send('MesSet sub ' + sub); return; }
  for (let i = 0; i < nexpr; i++) p = skipExpr(p);
  const id = p.readS16();
  const buf = th.add(0x74).readS32();
  const d = decode(getStr(buf, id));
  send(`MES buf=${buf} id=${id} sub=${sub} ${d.name ? '[' + d.name + '] ' : ''}${d.text}`);
});
hookOp('user1', 0x01, 'BGload', (th) => {
  const pc = th.add(0x14C).readPointer(); const a = pc.add(2);
  send(`BGload ${exprConst(a)} ${exprConst(skipExpr(a))}`);
});
hookOp('user1', 0x05, 'CHAload', (th) => {
  const pc = th.add(0x14C).readPointer(); const sub = pc.add(2).readU8(); const a = pc.add(3);
  send(`CHA sub=${sub} ${exprConst(a)} ${exprConst(skipExpr(a))}`);
});
send('hooked');
"""

s = frida.attach("Game.exe")
sc = s.create_script(JS.replace("CHARSET", json.dumps(charset)))
sc.on("message", lambda m, d: print(m.get("payload", m), flush=True))
sc.load()
time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 30)
s.detach()
