"""Frida prototype: per-frame capture of UI strings drawn by drawSingleTextLine
(RVA 0x45BC0, args from CoZ LanguageBarrier: textureId, startX, startY, a4,
char* sc3string, maxLength, color, glyphSize, opacity) and drawPhoneText
(RVA 0x451E0). Prints the set of strings whenever it changes (a frame ends
when Present-like idle >30 ms is detected by polling).
usage: proto_text.py <seconds>"""
import json
import os
import sys
import time

import frida

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
charset = open(os.path.join(ROOT, "re", "sc3tools", "resources", "sghd", "charset.utf8"), encoding="utf-8").read()

JS = r"""
const charset = CHARSET;
const base = Process.getModuleByName('Game.exe').base;
function skipExpr(p){for(;;){const b=p.readU8();if(b===0)return p.add(1);if((b&0x80)===0){p=p.add(2);continue;}let n=1;if((b&0x60)===0x60)n=5;else if((b&0x40)===0x40)n=3;else if((b&0x20)===0x20)n=2;p=p.add(n+1);}}
function decode(s){let out='';for(let i=0;i<2000;i++){const b=s.readU8();if(b===0xFF)break;
 if(b>=0x80){const g=((b&0x7f)<<8)|s.add(1).readU8();s=s.add(2);out+=g<charset.length?charset[g]:'?';continue;}
 s=s.add(1);switch(b){case 0x00:out+='/';break;case 0x04:case 0x15:s=skipExpr(s);break;case 0x07:case 0x1B:s=s.add(1);break;
 case 0x0C:case 0x11:case 0x12:case 0x13:case 0x14:case 0x16:case 0x19:case 0x1A:s=s.add(2);break;default:break;}}
 return out.replace(/[！-～]/g,c=>String.fromCharCode(c.charCodeAt(0)-0xFEE0)).replace(/　/g,' ');}
let cur = [], lastKey = '';
function add(tag, a, si, ci){ try { cur.push(tag+'('+a[1].toInt32()+','+a[2].toInt32()+') c='+(a[ci].toUInt32()>>>0).toString(16)+' '+decode(a[si])); } catch(e){} }
Interceptor.attach(base.add(0x45BC0), { onEnter(a){ add('line', a, 4, 6); } });
Interceptor.attach(base.add(0x451E0), { onEnter(a){ add('phone', a, 4, 7); } });
setInterval(() => { const key = cur.slice().sort().join('\n'); cur = [];
  if (key !== lastKey) { lastKey = key; send('--- frame\n' + key); } }, 100);
send('hooked');
"""
s = frida.attach("Game.exe")
sc = s.create_script(JS.replace("CHARSET", json.dumps(charset)))
sc.on("message", lambda m, d: print(m.get("payload", m), flush=True))
sc.load()
time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 60)
s.detach()
