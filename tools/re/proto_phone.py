"""Frida prototype: log phone text draws (drawPhoneText RVA 0x451E0,
sghdDrawInteractiveMail RVA 0x45640, sghdDrawLinkHighlight RVA 0x44E00;
signatures and argument lists from CoZ LanguageBarrier GameText.cpp) and the
phone ScrWork variables 6800-6830 whenever they change.
usage: proto_phone.py <seconds>"""
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
function decode(s){let out='';for(let i=0;i<4000;i++){const b=s.readU8();if(b===0xFF)break;
 if(b>=0x80){const g=((b&0x7f)<<8)|s.add(1).readU8();s=s.add(2);out+=g<charset.length?charset[g]:'?';continue;}
 s=s.add(1);switch(b){case 0x00:out+='/';break;case 0x01:out+='[N:';break;case 0x02:out+=']';break;
 case 0x04:case 0x15:s=skipExpr(s);out+='{c}';break;case 0x07:case 0x1B:s=s.add(1);break;
 case 0x19:out+='{a'+s.readU16()+'}';s=s.add(2);break;case 0x1A:out+='{anc'+s.readU16()+'}';s=s.add(2);break;
 case 0x0C:case 0x11:case 0x12:case 0x13:case 0x14:case 0x16:s=s.add(2);break;case 0x18:out+='{pnc}';break;case 0x1E:out+='{rc}';break;
 default:out+='{'+b.toString(16)+'}';}}
 return out.replace(/[！-～]/g,c=>String.fromCharCode(c.charCodeAt(0)-0xFEE0)).replace(/　/g,' ');}
const seen = {};
function logOnce(tag, text){ if (seen[tag] === text) return; seen[tag] = text; send(tag + ': ' + text); }
Interceptor.attach(base.add(0x451E0), { onEnter(a){ try{ logOnce('phoneText@'+a[1].toInt32()+','+a[2].toInt32(), decode(a[4])); }catch(e){} } });
Interceptor.attach(base.add(0x45640), { onEnter(a){ try{ logOnce('mail', decode(a[4]) + ' selectedLink=' + a[12].toInt32()); }catch(e){} } });
Interceptor.attach(base.add(0x44E00), { onEnter(a){ try{ logOnce('linkHi', 'selectedLink=' + a[9].toInt32()); }catch(e){} } });
const work = () => base.add(0x2303238).readPointer();
let last = [];
setInterval(() => { const w = work(); const v = []; for (let i = 6800; i < 6830; i++) v.push(w.add(i*4).readS32());
  const ch = []; v.forEach((x,i)=>{ if (last[i] !== x) ch.push((6800+i)+'='+x); }); if (ch.length && last.length) send('work ' + ch.join(' ')); last = v; }, 50);
send('hooked');
"""
s = frida.attach("Game.exe")
sc = s.create_script(JS.replace("CHARSET", json.dumps(charset)))
sc.on("message", lambda m, d: print(m.get("payload", m), flush=True))
sc.load()
time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 60)
s.detach()
