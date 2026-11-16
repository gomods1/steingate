# Verified hook targets

Build: STEINS;GATE Steam, appid 412830, buildid 4434295, Game.exe SHA-256
95C5A7BAFA0B8ED91F264516E27E636905E46B38AE24BDF2771E3FD69B626FF7, PE timestamp
0x5d639659, 32-bit, SteamStub (.bind) wrapped. Image base seen at 0x00AC0000 in
every launch so far (ASLR on; the mod uses RVAs only).

All RVAs below were checked on this build: signatures with tools/re/sigscan.py
against the decrypted dump (re/Game_mem.bin, from tools/re/dump_module.py),
behaviour with Frida (tools/re/*.py) and the mod log.

## Script VM

- sc3Eval RVA 0x181F0, `int __cdecl(ScriptThread*, int* out)`: evaluates the
  expression at thread->pc and advances pc. Signature from CoZ gamedef.
- getStringFromScript RVA 0x62D40, `const uint8_t* __cdecl(int scriptBuffer, int stringId)`.
- Instruction dispatcher RVA 0x62C30: opcode byte 0 group (0x00 System table
  RVA 0x933610, 0x01 Graph table RVA 0x932FF8, 0x10 User1 table RVA 0x934578),
  byte 1 = index. Handlers `void __cdecl(ScriptThread*)`; blocking handlers
  are re-run every frame without advancing pc.
- ScriptThread: pc at +0x14C, script buffer id at +0x74 (Frida prototype
  tools/re/proto_dialogue.py decoded correct lines with these).
- Flags: bit array behind pointer at RVA 0x2303234 (getFlag RVA 0x70890).
- ScrWork: int array behind pointer at RVA 0x2303238.

## Hooked instructions (opcodes from MgsScriptTools spec-bank, steins_gate_hd)

- 01 0C xx Mes*SetMesScx: sub&3==3 -> 3 exprs, else 1 expr; then int16 LE
  string id in the thread's script buffer. Verified: lines spoken correctly.
- 10 01 BGload(expr mask, expr bg.mpk file id). Re-executed per frame while
  loading. -16777216 (0xFF000000) etc. = colour fill.
- 10 05 00 CHAloadAtlas(expr mask, expr chara.mpk file id).
- 10 10 BGrelease(expr mask), 10 11 CHArelease(expr mask).

## Native UI state

- Title menu cursor: int at RVA 0x1244464 (0 Start, 1 Load, 2 Extra, 3 Config,
  4 Help). Found with tools/re/memdiff.py; read by title draw code ~RVA 0x41230.
- Title Load submenu cursor RVA 0x124446C; Extra submenu cursor RVA 0x1244470.
- Title submenu open: int at RVA 0x12615E4 != 0 (draw code calls 0x40650 for
  Load, 0x41030 for Extra). Extra layout changes when flag 860 is set (0x40C30).

- Load submenu labels: 0 Quick load, 1 Load (screenshot).
- Save/Load screen: cursor in page RVA 0x1261734, page RVA 0x126173C
  (8 slots per page, 6 pages; Right on the right column turns the page).
  F5 opens Load, F6 opens Save. Save mode from the SaveMenuInit byte (10 23 00 xx).
- Confirmation dialogs ("Load this Data?" OK/Cancel): selected option RVA
  0x1242DC8 (0 first, 1 second). Their text comes through drawSingleTextLine.
- Config: page RVA 0x1243878 (0..2), cursor of page p RVA 0x1243880 + 4p.
  Fields (defaults routine RVA 0x708C0; mapped with tools/re/configmap.py):
  phone-trigger skip release 0x2303294, tips info 0x230328C, auto quick save
  0x230326C, message speed 0x230325C (256..4096), auto speed 0x2303260
  (256..2048), skip mode 0x2303268 (0 read only, 1 all), voice 0x2303248,
  music 0x230324C, SE 0x2303250 (+0x2303254), movie 0x2303258 (0..128), sync
  voice 0x2303264, skip voice 0x2303288; per-character voice volume
  0x23033B8 + 4i, on/off 0x2303330 + 4i, rows -> i = 0,1,4,3,5,2,6..11.
- Help page RVA 0x1243A00. Tips list row cursor RVA 0x12E66C.
- System menu (key 1): cursor ScrWork 3338 (SW_SYSMENUCNO). Items: Backlog,
  Quick save, Quick load, Save, Load, Tips list, Config, Full screen, Help,
  Title menu, Close menu, Exit game.
- Backlog (draw code RVA 0x46C90): count 0x1343D04, entry->line 0x14FB470,
  line->first glyph 0x13D64D0, glyph count 0x1407210, glyph ids (u16) at
  0x1343D08 (0x8000|1 name start, |2 name end, |3 end), cursor 0x14FB450.
- Skip mode flag RVA 0x23034D0. Text window alpha RVA 0x2304E0C (256 shown,
  0 hidden; Backspace toggles in dialogue).

## Layers and date

- Background layer i shown while flag 2400+i; character layer i while flag
  2410+i (verified: flag 2411 set when Itaru's sprite appeared on screen).
- Date shown top left: ScrWork 1430 = month*100+day (year 2010).

## Phone (ScrWork)

- 6801 slide (0 hidden .. 20 up), 6802 phone screen: 0 home, 20 menu, 1
  mailbox (Inbox/Outbox), 3 mail list, 5 mail view, 6 address book, 7/39
  contact actions, 9 sending, 10 calling, 11 in call, 23 dial pad.
- 6812 menu option (0 mail, 1 address book), 6806 mail list row (6807 mail
  id), 6821 address book row, 6831 dial pad key (1-9, *, 0, #), 6832 keys
  entered.
- Phone text: drawPhoneText (white headers, black entries, red selection);
  interactive mail: sghdDrawInteractiveMail with selectedLink; links are
  ruby-style spans (0x09 .. 0x0B) in _MAIL strings.

## Text draw functions (MinHook)

- drawSingleTextLine RVA 0x45BC0, drawPhoneText RVA 0x451E0,
  sghdDrawInteractiveMail RVA 0x45640 (CoZ signatures and argument lists).

## Input

- Game reads the keyboard only through DirectInput8 (GetDeviceState, 256
  bytes). No user32 key APIs imported or called (tools/re/probe_input.py).
- The mod patches IDirectInputDevice8 vtable slots 9/10 (shared per flavour).

## Bink

- IAT slots: _BinkOpen@8 RVA 0xB7210, _BinkClose@4 RVA 0xB721C (pefile).
- BINK struct head: Width, Height, Frames, FrameNum, LastFrameNum, FrameRate,
  FrameRateDiv (Bink SDK layout; HYPOTHESIS until the movie track timing is
  checked in game).
