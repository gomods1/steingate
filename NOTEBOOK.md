# Notebook: STEINS;GATE

## Game

- Name: STEINS;GATE (2016 Steam release by MAGES., English text, Japanese voices)
- Store and install folder: Steam appid 412830, `S:\steam\steamapps\common\STEINS;GATE`
  (found by find-game.ps1, 2026-10-24)
- Main executable: Game.exe (started by Launcher.exe as `Game.exe mages_sgate`;
  without that argument it shows "Please launch from launcher.")
- Build proven on: Steam buildid 4434295, Game.exe SHA-256 95C5A7BA...6FF7
- 32 or 64 bit: 32-bit (PE Machine 0x14c)
- Engine family: native-custom
- Engine and version: MAGES. engine (SC3 script VM, .mpk archives, Bink 2 movies),
  Game.exe FileVersion 1.0.0.1
- Anti-cheat or DRM: SteamStub (.bind section, .text encrypted on disk). No anti-cheat.

## Stages

- Recon (game-recon): done
- Injection (inject): done (dinput8.dll proxy, started by SGALaunch.exe through a junction)
- Speech (speech): done (SpeechHost.exe x64 + Prism, NVDA backend verified)
- Screen text (screen-text): done for dialogue, menus, backlog, dialogs, phone (see SCREENS.md); reply links pending
- Scanner (scanner): not applicable (no world to walk in); replaced by scene descriptions
- Autowalk (autowalk): not applicable

## Resources

- Committee of Zero (CoZ), https://github.com/CommitteeOfZero: LanguageBarrier
  (runtime injector, struct/hook knowledge), sghd-patch (gamedef.json with byte
  signatures for THIS Steam build; verified unique), sc3tools (MIT; SG charset),
  MgsScriptTools (MIT; SC3 decompiler with steins_gate_hd specs), impacto (ISC;
  engine reimplementation, sgps3 profile, .lay sprite format). Checked 2026-10-24.
- Existing accessibility work: none found (2026-10-24). Searched: GitHub repos
  (steins gate + accessibility/blind/screen reader/nvda/tts), Molitvan list,
  web search, AudioGames forum search. Only a 2016 Steam thread of a blind player
  asking for help (steamcommunity.com/app/412830/discussions/0/224446614467354621).

## Decisions

- 2026-10-24: Verdict feasible (native engine but CoZ signatures cover this build).
- 2026-10-24: Injection by dinput8.dll proxy started through a directory
  junction, not CreateRemoteThread: an injecting launcher was deleted by the
  antivirus right after its first run; a proxy needs no injection.
- 2026-10-24: Speech out of process (SpeechHost.exe x64) because Prism ships
  x64 only and the game is 32-bit.
- 2026-10-24: Skip mode handled by a 150 ms hold before speaking a line; the
  skip state is native, not a script flag (probe: no flag/ScrWork change).
- 2026-10-24: Character names in descriptions are held back until the name has
  appeared as a dialogue speaker (known_characters.txt next to the mod).
- 2026-11-16: Releases from GitHub, gomods1/steingate, as for the Sims mods:
  pushing a tag vX.Y.Z runs .github/workflows/release.yml (downloads Prism
  v0.18.3, build.ps1 via tools/package.py) and attaches
  SteinsGateAccess-<tag>.zip (game-folder layout), the .map and the committed
  installer/SteinsGateAccessibilityInstaller.exe. The installer is game 11 in
  the shared ..\installer project (src/games.rs); it refuses a foreign
  dinput8.dll (no "Steins;Gate Access" text in it) and leaves the
  %LOCALAPPDATA% junction alone on uninstall. Rebuild and recommit it only
  when the installer changes. MinHook is a git submodule.

## Proven facts

- The folder name "STEINS;GATE" contains a semicolon, which splits the DLL
  search path: a dinput8.dll in the game folder is ignored when Game.exe is
  started by its real path (Frida module list), and loads when started through
  the junction %LOCALAPPDATA%\SteinsGateAccess\game (Frida module list, sga.log).
- Game.exe reads the keyboard only through DirectInput8 GetDeviceState (sga.log
  "keyboard read through GetDeviceState").
- SC3 dialogue strings decode correctly with sc3tools' sghd charset (spoken.log
  "???: Hey, what are you mumbling about?").
- NVDA backend through Prism in SpeechHost (speechhost.log "backend: NVDA").
- More in PATCH_TARGETS.md.
- 2026-10-26: The first mail with reply words is Kurisu's "Did you write it?"
  (_MAIL.SCX strings 1000 subject, 1001 sender "Assistant", 1002 body with
  [report] [scientist] [understand]; scan: scratch maillinks.py over the 0x09..0x0B
  spans). 115 interactive mail bodies in all. Not reached by 7/29 evening (slot 7).
- 2026-10-26: Layer state blocks: background layer i at ScrWork 4500 + 40 i,
  character layer i at 5100 + 40 i, show flags 2400 + i / 2410 + i (MACROSYS2
  tables 460, 459, 461). File number at +7 (bg) and +9 (chara): matches a probe
  at the board scene (4587 = 477 = the browser picture, flag 2402 set; 5109 =
  65535 = empty). After loading a save the BGload hooks do run (D described
  the picture without the resync fallback), so the fallback in scene.cpp is a
  safety net (HYPOTHESIS that it is ever needed).

- 2026-10-24: An injecting launcher (CreateRemoteThread) was deleted by the
  antivirus after one run; replaced by the junction + proxy route.
- 2026-11-14: The installed SGALaunch.exe was still that old injecting build
  (103,936 bytes, 24 Oct; strings: "Could not load the accessibility mod").
  It started Game.exe from the real STEINS;GATE path, so dinput8.dll never
  loaded and nothing spoke, with no error. Replaced by the 26 Oct junction
  build; build.ps1 -Install now compares hashes of SGALaunch.exe,
  SpeechHost.exe and dinput8.dll and throws on mismatch. How the old file
  came back is unknown (HYPOTHESIS: restored from antivirus quarantine).
  Quick check: Game.exe's command line must start with
  %LOCALAPPDATA%\SteinsGateAccess\game\.
- 2026-10-24: Dialogue/announcements queue (rpg.md rule 4); a setting makes
  new lines interrupt.

## Pitfalls

- ffmpeg cannot decode Bink 2: movie text must be captured in game
  (tools/playtest/capture.py + tools/ocr.ps1, Windows OCR).
- Save/Load cursor: slots 5-8 are the right column; use tools/playtest/slot.py.
- The phone menu remembers its last option; use tools/playtest/phone.py.

- PowerShell Copy-Item with -LiteralPath for the destination fails; use -Destination.
- Log files must be opened shared (_wfsopen _SH_DENYWR) or they cannot be read while the game runs.
- gamectl focus: Windows focus-steal protection; SwitchToThisWindow works.
- Pressing Esc in game opens "Exit game?"; Backspace moves to Cancel.
- Killing skiploop.ps1 while gamectl holds Ctrl leaves Left Ctrl logically
  down (GetAsyncKeyState 0x8000): the game stays in force skip and the mod
  stays quiet. Release it with `python gamectl.py keys ctrl` after any kill.
- An NVDA dialog ("Seleccionar Sintetizador") in the foreground pauses the
  game loop (no perf lines) and cannot be closed by automation (UIAccess):
  the player must close it. A freshly launched game takes the foreground again.
- Never kill a Python process that has Frida attached: the game crashes in
  frida-agent.dll (c0000409, 2026-10-24). Let scripts time out or detach.
  Two Frida scripts with Interceptor hooks on draw functions also left the
  system menu unresponsive to arrow keys.

## Open questions

- Phone Trigger UI state (mail list, mail text, reply links, calls): to reverse engineer.
- Save/Load, Config, Backlog, Tips, Clear list, System menu, system message dialogs: cursor variables to find.
- Supported languages of this build: English text only (HYPOTHESIS; Japanese system strings exist in _STARTUP_WIN.SCX).

## Test log

- 2026-10-24 Frida prototype: dialogue strings decoded at MesSetMesScx. OK.
- 2026-10-24 Mod via SGALaunch: loaded, hooks, NVDA speech, dialogue line, R/D/H/Ctrl+Up keys, Esc masked while mod menu open. OK.
- 2026-10-24 Session 1 playtest, chapter 1 from new game to 7/29 evening (saves
  1-7): dialogue, skip, scene and character descriptions, date, prologue and
  OP movies, title/load/save/config/clear list/tips/help/system menu/backlog,
  dialogs, tutorial boxes, new tips, phone (menu, mail, address book, calls,
  dial pad, sending a blank mail), @channel board. OK. Reply words not reached.
