# MOD_TEMPLATE_RPG.md

A playbook for an AI coding agent building a **blind accessibility mod for a PC role-playing game**: party-based or single-hero CRPGs, isometric or third-person, with exploration of a 3D or 2.5D world, branching dialogue, character building, inventory, and combat that is turn-based, real-time-with-pause (RTwP), or both. Any engine: Unity, Unreal, a proprietary engine with a community script extender, or an old native game with no modding support at all. It is distilled from shipped screen reader mods for menu-driven and real-time games, but it deliberately contains **no game-specific facts**. Every class, function, address, scripting call, stat name, screen, key binding and coordinate convention for the target game must be discovered by you, from the game itself. Names in the code samples (`SomeScreen`, `PartyManager`, `GetTitle`) are placeholders, not predictions.

The goal is not "the mod speaks some text". The goal is: **a blind player can create a character, explore the world, talk to everyone, fight every battle, manage the party and inventory, and finish the story to the credits without sighted help.**

An RPG is three games stacked on top of each other, and this template borrows from the two other genre templates accordingly:

| Layer | What the sighted player does | Closest template technique |
|---|---|---|
| **Menus and sheets** (inventory, character sheet, spellbook, journal, shops, level-up, character creation) | Reads dense panels, hovers tooltips, drags items | Screen inventory, focus reading, navigation layer, review buffers (generic template) |
| **Turn-based or paused combat** | Reads the battlefield, hovers enemies for hit chance, places area templates, spends action points | Targeting mode, board review cursor, phantom-event guard, action economy readouts (roguelike template) |
| **Exploration** in a 3D/isometric world | Sees the room, clicks where to walk, notices glinting items, doors, traps and NPCs, follows the quest marker | Spatial foundations, path guidance, scan and entity browser, audio cues (shooter template) |

Dialogue sits across all three and is the heart of most RPGs; it gets its own section (15).

Sections marked **[managed]** apply to games whose code runs on a managed runtime you can decompile (Unity Mono, XNA/MonoGame/.NET, Java). Sections marked **[native]** apply to games compiled to machine code. Sections marked **[extender]** apply to games where a community script extender (a DLL that exposes the engine to Lua, Python or similar) exists and does most of the reverse engineering for you. Unmarked sections apply to everything.

---

## 0. Non-negotiable rules

1. **Understand the game before writing mod code.** Extract everything that can be extracted statically (section 4), then observe the running game (section 5). Never guess a method signature, field name, struct offset, scripting function, stat name or enum value; read it or measure it.
2. **Traverse every screen and every mode of play.** Build a complete inventory (section 6) and drive each entry to "verified". Popups, confirmation dialogs, tutorials, level-up, dice or skill-check overlays, reaction/interrupt prompts, rest and camp screens, death and game-over screens, credits, launchers and error dialogs all count; so do exploration, dialogue, combat, stealth, trading, crafting, world-map travel and cutscenes.
3. **Speak through Prism** (`C:\Users\Nitropc\code\libs\prism`), via one wrapper. No other code calls the speech library (section 8).
4. **Never interrupt speech by default.** Always queue (`interrupt = false`). Combat turns and dialogue lines arrive in bursts, and interrupting cuts off information the player has no other way to get.
5. **Announcements are concise; details go to review buffers** (section 11). Focus on an item says name and the one or two numbers that matter. Rules text, flavor, keyword explanations and comparisons go into a buffer the player reads on demand.
6. **Prefer game state over rendered output.** Read from the game's own objects, stats and view models. UI labels are a fallback; hooked draw calls are a fallback for that; OCR is the last resort.
7. **Review features never change game state.** The battlefield review cursor, the world cursor, the scan, the entity browser and the map review only read; they never move a character, the camera, the selection or the clock. Features that *act* (walk there, face that, cast at that) are separate, explicitly invoked keys, and each announces what it did.
8. **Use the engine's own spatial and rules queries.** Pathfinding, navmesh or tile grid, raycasts and line of sight, hit-chance and damage previews, movement-cost previews, area-of-effect targeting, "can this character use this item/skill now". Call them; never reimplement the game's rules or geometry.
9. **Everything a sighted player learns by looking or hovering must be reachable by keyboard.** Tooltips, portraits and their status icons, health bars over heads, the glint on a lootable item, the red outline of a hostile, the path line and its cost, the colored text in dialogue options, the minimap.
10. **If the keyboard cannot reach it, build the path.** RPGs are overwhelmingly mouse-first (click to move, hover to inspect, drag to equip, right-click for context menus). Where the game has no keyboard navigation, the mod writes the navigation layer (section 10.4). A controller mode, if the game has one, is often the best starting point (tier B).
    - **Navigation follows meaning, not the screen's layout.** Map the keyboard to what makes sense for a screen reader user, never to how the UI looks: sections (Tab), items (arrows), levels (Enter / Backspace), grouped and ordered by what things are and how the game structures them, never by where they are drawn on screen. One key grammar for every screen and mode (section 10.4, "Collecting and ordering items").
11. **Assists are opt-in, configurable and disclosed.** Anything that changes how the game plays (auto-battle, forced turn-based mode, reduced difficulty, auto-loot, auto-walk) is off by default or explicitly chosen by the player, and listed honestly in the README. Information (speech, cues, previews) is not an assist and is on by default. Prefer the game's *own* options (difficulty, auto-pause settings, turn-based toggle) over mod-side changes.
12. **Use the running game as an instrument.** Launch the game, attach debuggers or the extender console, inspect state, take screenshots (sections 1 and 5). A breakpoint or a console query answers in a minute what theorizing cannot in an hour.
13. **Prefer scriptable, text-producing tools.** Frida scripts, headless Ghidra scripts, extender console snippets and Python extractors can be rerun, diffed and committed to `tools/`.
14. **Write down what you learn.** Maintain `CLAUDE.md` (architecture, pitfalls, coordinate conventions, game key bindings), `PATCH_TARGETS.md` (verified hook targets and scripting calls), `SCREENS.md` (screens and play modes), `PLAYTEST.md` (coverage map and session log, section 23), `CUES.md` (audio cues, if any) and, for native games, `STRUCTS.md`.
15. **Keep the plan and the progress on disk.** A future session starts with no memory of this one, and a full RPG takes many sessions.
    - `ROADMAP.md`: the plan. Milestones ordered so each is testable in the running game (for example: M0 foundation and speech, M1 every text the game shows, M2 menus and screens, M3 character creation, M4 dialogue, M5 exploration, M6 combat, M7 inventory/trade/progression, M8 full blindfold playthrough, M9 release), each a checklist of concrete items, ticked `[x]` only when verified in game.
    - `STATUS.md`: read first in every session. "Current state" (what works and was verified in the running game, with date and session number; game build and extender version; where the test saves are; what is half-done), and "Next steps" in order. **Update both sections at the end of every session**, and whenever a milestone item changes state.
    - Never mark something done in either file because it compiled; done means exercised in the game (rule 18).
16. **Do not invent game facts in documentation or code comments.** If you did not read it in extracted code or data, or observe it in a log, console or debugger, it is a hypothesis; label it as one. Community wikis, modding docs and extender API docs are leads, not facts, until checked against this build.
17. **Saves on the dev machine are disposable; players' saves are not.** Create, overwrite, edit and delete test saves freely (section 23 depends on it). But the *shipped* mod must not add anything to a save that makes it unloadable without the mod (new stats, items, scripted state), because players' saves hold dozens of hours.
18. **Play the game yourself, all of it.** You are the first tester. Drive the running game with screenshots and input (section 23), reach every screen, every play mode and every edge case, and finish the story. Then do it again **blindfolded**: only mod keys in, only the spoken-line log out. A feature nobody exercised in the running game is not done.
19. **No obscure keys.** Any information that belongs to something the player can focus (a party member and that member's stats, equipment, skills and approval; an item and its properties; an ability; a quest and its details; a class on the creation screen; a setting and what it does) goes into the review buffers, readable with **Ctrl+Up** as soon as the thing is focused (section 11). Never give it a dedicated hotkey ("read strength", "read armor class", "read item value"): the player cannot discover or remember a key per fact. Dedicated keys are reserved for **HUD information**, the standing state a sighted player reads off the screen during play without focusing anything (the party's HP and resources in combat, whose turn it is, gold, area and time of day, the active objective), and for actions.
20. **Mod settings live in the game's settings UI.** Never make players change settings with hotkeys or by editing files. If the mod can add a tab of its own to the game's options menu, it does; otherwise it adds a "<Mod> settings" entry to a menu the player already reaches (options, pause, main menu) that opens a settings screen built with the mod's normal navigation (section 22). A file on disk may *store* the settings; players never need to open it.
21. **Ship in every language the game supports.** Every string the mod speaks or shows comes from one localization table and follows the game's current language. English-only is acceptable during development; **a release never ships without the mod's strings in every language the game supports** (section 20).
22. **Read only what is relevant on screen.** When a screen opens, say its name, any message it is showing (a notification, a dialog's text, a tooltip, a tutorial hint, an error) and the focused item; never read every tab, option or control on it (section 10.4). The player discovers those by moving focus, as a sighted player discovers them by looking. Notifications, tooltips and help messages that appear on screen (quest updated, item received, a tutorial popup, a "not enough gold" message) are relevant and are read when they appear.

---

## 1. What you may do on this machine

The user owns this machine and a legitimate copy of the game, and the mod is single-player accessibility work. Within that:

**Allowed without asking each time**

- Launch, drive and close the game. Take screenshots. Read its logs, config and save files (copies).
- Install analysis tools in user scope (`scoop`, `winget`, `pip`, `dotnet tool`). Say what you installed and why; record it in `CLAUDE.md`.
- Attach debuggers, read and write the game's process memory, inject DLLs and scripts (Frida, your own, the community extender).
- Patch **copies** of game binaries and data files for experiments.

**Ask first**

- Anything that needs administrator rights, installs a driver (virtual gamepad drivers included), changes system-wide settings, or disables a security feature.
- Installers that bundle third-party offers (the Cheat Engine installer at `S:\games\software\CheatEngine751P.exe` is one).
- Modifying the game's install in place. **Back up first, always**, to a dated folder outside the install.
- Save files: unless the user says otherwise, saves on the dev machine are **disposable test material** (create, overwrite, delete freely). Keep a curated `saves/` library of milestone saves (section 23.3) outside the game's folder so they survive cleanups and game reinstalls.
- Installing community frameworks (script extenders, mod managers, loaders) into the user's game: say what each one changes (achievements, save compatibility, update breakage).

**Not allowed**

- Touching anti-cheat or online/multiplayer sessions. Many RPGs have co-op: find out how the game separates it, and make the mod either inert or explicitly host-only there (decide with the user).
- Cracking or redistributing copy protection.
- Shipping game code, decompiled source, extracted assets or patched binaries. The release contains only the mod's own files.

**Hygiene**: never leave the game, a debugger, or an injected helper running at the end of a task. Record PIDs and clean up by PID.

---

## 2. Phase 1: Reconnaissance

Record every answer in `CLAUDE.md`. All of it can be done before starting the game.

| Question | How to find out |
|---|---|
| Engine / runtime | Install folder, exe imports, strings. `Assembly-CSharp.dll` = Unity Mono; `GameAssembly.dll` = IL2CPP; `.pak` + `Engine\` = Unreal; `.pck` = Godot; a proprietary archive format plus a big native exe = custom engine (common for CRPG studios). |
| Community script extender? | The single most important question for a modern CRPG. Long-lived RPGs often have a community DLL that exposes the engine's entity system, rules data, scripting VM and UI to Lua or another language, with published API docs and IDE type stubs. If it exists, it replaces most of sections 4.3 and 5 for gameplay features. Record its version, its loader, its license, how it is installed, what it disables (achievements, some online features), and how quickly it follows game patches. Its API docs are leads (rule 16): verify each call you use in the console. |
| Official modding support | An official editor/toolkit, an in-game mod manager, a documented mod folder and load-order file. These define how the mod is installed and what it may contain. |
| Bitness, packing, exact build | PE header, Detect It Easy, SHA-256 and version of every exe and DLL you hook. Record now; the mod checks at startup. RPGs patch often after launch: note the patch cadence. |
| Multiple executables | Separate exes per graphics API (e.g. DirectX and Vulkan builds), a launcher, a crash reporter. The mod must work with whichever the player launches. |
| Launcher | Does the game start through a publisher or studio launcher (often a web view, often inaccessible)? Is there a documented or discovered switch to skip it? |
| UI framework | uGUI/TextMeshPro, UI Toolkit, UMG, Scaleform/Flash, NoesisGUI (XAML/WPF-like, MVVM with view models), Coherent/Gameface (HTML), ImGui, or custom. MVVM frameworks are a gift: the **view model** is structured game state that the UI binds to, ideal for reading. |
| Separate controller UI? | Many console-ported RPGs ship **two UIs**: a mouse UI and a controller UI with radial menus and focus navigation. The controller UI is often the more accessible starting point (tier B). Find out how the game decides which one to show and whether it can be forced. |
| Navigation tier | **Tier A**: keyboard focus in the UI framework. **Tier B**: a controller navigation system the keyboard does not drive, or only partly. **Tier C**: mouse-only. Decide per screen. |
| Movement model | Click-to-move with pathfinding, direct movement (WASD/stick), or both. Party formation and follow behavior. Whether turn-based mode can be toggled outside combat. |
| Combat model | Turn-based (initiative, action points or action/bonus/movement economy), RTwP (auto-pause settings, queued orders), or real-time. Where the rules live: data tables, script, or code. |
| Rules data | Stats, spells/skills, items, statuses/conditions, classes, feats/perks, loot tables. Usually in data files (text tables, XML, JSON, binary). The mod needs their **display names and descriptions**, not their internals. |
| Dialogue system | Dialogue trees in data files, a scripting VM, conditions and checks on options, skill-check or dice UIs, companion reactions/approval. |
| Localization | String tables per language, handles/IDs, the single "get translated string" entry point, and **the full list of languages the game supports**: the mod must ship in all of them (rule 21). |
| Input paths | Window messages, raw input, SDL, DirectInput, XInput, an engine input layer. Usually a UI path and a gameplay-action path. Where default key bindings are stored (a config file in the user profile is common). |
| Audio | Middleware (Wwise, FMOD) or custom mixer; whether the game ducks audio in dialogue; whether the extender exposes it. |
| Saves and profile folders | Paths, format, whether mods are recorded in saves, cloud sync. |
| Existing accessibility | The game's own options (subtitles, text size, dialogue history, auto-pause, colorblind modes, narrator). Prior community accessibility attempts. |

---

## 3. Toolbox

Check what is installed first (`Get-Command`, `scoop list`, `winget list`); verify package IDs at install time.

| Need | Tool | Notes |
|---|---|---|
| Identify packer/compiler/bitness | Detect It Easy (`diec`) | |
| Native disassembly/decompilation | Ghidra (headless), rizin | Export functions, strings, xrefs, RTTI class names to a gitignored `re/` folder. |
| .NET decompilation | `ilspycmd` (installed) | `game/` folder, gitignored. |
| Unity assets | AssetRipper at `S:\games\software\AssetRipper_win_x64\` | |
| Godot | GDRE Tools at `S:\games\software\` | |
| Proprietary archives | Community extractors (often a CLI converter per engine), else a Python extractor in `tools/` | Convert binary resources to text (XML/JSON) so they can be grepped. |
| Debuggers | x64dbg, `cdb`/WinDbg | Command files in `tools/`. |
| Dynamic instrumentation | Frida (`pip install frida-tools`) | Prototype every native hook here first. |
| Memory scanning | `pymem` scripts; Cheat Engine only with the user (ask first) | |
| File/API tracing | Frida file-API hooks; ProcMon (admin, ask) | |
| Extender console | The extender's own console / REPL | The fastest instrument you will have: query entities, call scripting functions, subscribe to events live. Save useful snippets to `tools/console/`. |
| Audio | miniaudio (vendored); Audacity or numpy for offline checks | Only if the mod plays its own cues (section 19). |
| Build | Toolchain matching the game's bitness | Record in `CLAUDE.md`. |

---

## 4. Phase 2: Static extraction

Everything extracted goes into gitignored folders or outside the repo. It is copyrighted reference material, never shipped.

### 4.1 Code

- [managed] `ilspycmd -p --nested-directories -r "<Managed>" -o game "<Managed>\Assembly-CSharp.dll"`; IL2CPP: dumper signatures plus Ghidra.
- [native] Imports, strings, RTTI class list and vtables, library identification, `re/names.csv`, calling conventions (generic template 4.3).
- [extender] The extender's **source code**, if public, is the best symbol file you will get for the engine: component and struct layouts, function signatures, byte patterns for engine functions, enum names. Read it (respect its license: reading and referencing is fine; copying code into your mod depends on the license). Generate its IDE type stubs if it offers them and grep them.

### 4.2 Data

Extract and convert to text:

- **String tables** for every shipped language.
- **Rules data**: stats, spells/abilities, statuses/conditions, items, classes, races/origins, feats/perks, difficulty settings.
- **Dialogue trees**: the structure of a conversation, its conditions, checks and outcomes. Useful to understand what the dialogue UI shows (check type, difficulty, requirement tags), not to spoil the story in speech.
- **UI layout files** (XAML, prefabs, SWF, HTML): a free screen inventory, and the names of bound view-model properties.
- **Input configuration**: the default key bindings. You need them before choosing a single mod key.
- **Icons and inline tags**: status icons, damage type icons, inline glyph tags in tooltips. The mod turns them into words.

### 4.3 Build a map of the game

Locate and document in `CLAUDE.md`:

- The **main loop / per-frame hook** (or the extender's tick event), and on which thread or context game state may be read.
- **Client/server split**: many modern RPGs run a local server even in single player (game logic, scripting) and a client (UI, camera, input, rendering). Know which side owns each piece of state and how to pass data between them.
- The **screen / UI manager**: how screens open, close and stack; which one has input; where modals come from.
- The **entity model**: characters, items, containers, doors, traps, interactables; how to enumerate them; how to get display name, position, faction/attitude, health, level, statuses.
- The **party model**: party members, the currently controlled character, selection groups, followers, summons.
- The **rules surface**: stats and derived values, resources (hit points, action points, spell slots, cooldowns, charges), statuses, the "can use" query for abilities and items.
- The **event surface**: combat start/end, turn start/end, round change, attack/damage/heal/miss, status applied/removed, death/downed/revived, item gained/lost, gold changed, level up, quest update, dialogue started/ended, skill check rolled, companion reaction.
- **Previews and simulations**: hit chance, damage and path-cost previews; any preview that runs real rules code. Find the preview flag now; it is the source of phantom announcements later.
- The **tooltip system**: how tooltips are assembled. Usually the best source of descriptions.
- The **notification / message system**: toasts, banners, "quest updated" and "item received" popups, tutorial hints and error lines that appear on their own. These are what rule 22 says to read as they appear.
- The **options screen**: how its tabs and controls are built and how values are saved, so the mod can add a settings tab of its own (section 22).
- The **current language**: where the setting lives, which languages the game supports, and how the game reacts when it changes (section 20).
- The **command flow**: the function(s) that take (character, action, target) and make things happen: move to position, attack, cast/use ability at target or position, use/pick up/open, talk to, end turn. That is what the mod calls.
- The **camera** and world-to-screen projection; whether interaction depends on what is on screen.

---

## 5. Phase 3: Dynamic analysis

- **Get the game windowed** before debugging (breakpoint + exclusive fullscreen = frozen desktop).
- **Launch and track** by PID with a timeout; skip the launcher if possible.
- **Screenshots are your eyes.** Capture the window and read the image to learn what a sighted player sees on each screen.
- **Drive input** with `SendInput` scancodes, or better, through the extender or game functions.
- **Saves library**: build (or ask the user to help build) saves that land on hard-to-reach states: character creation, level-up pending, a trader, a camp, a multi-enemy fight, a fight with area hazards, a dialogue with skill checks, a locked chest, a trapped corridor, a late-game area, a party wipe.
- **Make something happen, catch the code that did it, read backwards** (generic template 5.2): hook text APIs with backtraces, watch values with write breakpoints, log every event the extender exposes while you play a fight or a conversation.
- **Extender event logging**: subscribe to every relevant event and log name plus arguments for a whole combat, a whole dialogue, a trade. Those logs become the event surface in `PATCH_TARGETS.md`.

---

## 6. Phase 4: Screen and play-mode inventory (mandatory)

Create `SCREENS.md`. Enumerate screens from **code or UI layout files**, **data**, and **runtime logging with screenshots**; each source misses things.

| Screen | Class / file | Entry signal (verified) | Focus model | Info a sighted player sees | Hover-only / spatial-only info | Status |
|---|---|---|---|---|---|---|

Status: **not started / announced / navigable / verified** (verified = tested with a screen reader, including edge cases: empty lists, locked items, disabled buttons, not enough gold, failed checks, interrupted actions).

RPG prompts (questions, not the inventory): launcher, intro videos, main menu, load/save with slot details, difficulty and custom rules, **character creation** (origin/race/class/subclass/background/abilities/skills/spells/appearance/voice/name, every step), tutorial prompts, the exploration HUD (portraits, hotbar, minimap, action/resource bars, turn order, combat log), context and radial menus, **inventory** (per character, shared, containers, loot windows, split stack, weight), **equipment** and comparison tooltips, **character sheet** (every tab), **spellbook/abilities** (prepare/memorize), **level up** (every choice), **dialogue** (lines, options, check tags, dice or check overlay, companion reactions), **trading/barter**, crafting/alchemy/enchanting, **journal** (quests, notes, codex/glossary, tutorials), **map** (local, world, fast travel points, markers, custom markers), rest/camp, party management (swap, recruit, dismiss), **combat** (turn order, targeting, AoE placement, movement preview, reactions/interrupt prompts, death saving, downed allies), stealth and detection, lockpicking and disarming, reading books and letters, cutscenes, death/game over, credits, options (every tab, including key rebinding and the game's own accessibility options), confirmations, errors, co-op lobby (announce as inert/host-only).

**Play modes** are rows in a second table: exploration (one character, whole party), sneaking, turn-based exploration (if the game allows it), dialogue, combat (own turn, enemy turn, reaction prompt, targeting, AoE placement, movement), trading, camp, cutscene, world-map travel, loading. Each: what a sighted player perceives, which mod keys and cues exist, help context, status.

**The mod is not done while any row is below "verified".**

---

## 7. Phase 5: Mod architecture

```
<Game>Accessibility/
├── Core/              # Speech wrapper, input, config, keywords, buffers, focus helpers, speech scheduler
├── Game/              # The ONLY layer that knows engine names/addresses/extender calls: typed accessors
├── State/             # StateCache, play-mode tracker, party model, event surface, preview guard
├── Rules/             # Stat/ability/status/item readers and description builders (FocusReadout)
├── Screens/           # Per-screen handlers + generic fallback (10.4), Readers/ per UI domain
├── Dialogue/          # Dialogue reader, option describer, check/dice handler, history
├── Exploration/       # Spatial helpers, scan, entity browser, world cursor, path guidance, sensing
├── Combat/            # Turn tracker, battlefield review, targeting, AoE placement, movement planner, reactions
├── Inventory/         # Inventory/equipment/trade/container lists, pick-up-and-place
├── Audio/             # Optional cue engine (section 19)
├── Hooks/             # One file per hooked target or subscribed event group
├── Help/              # Context help and screen state tracker
├── Utilities/         # Text cleanup, localization, logging
└── tools/             # RE and console scripts. Not shipped.
```

Startup order: logging, build check, co-op/online check, config, speech, keyword glossary, buffers, hooks and subscriptions, handlers, help contexts.

**Split-runtime mods** [extender]: when the gameplay-facing part must be written in the extender's scripting language (which usually cannot load native libraries or play audio), the mod has two halves:

- a **script half** (Lua or similar) that reads game state, subscribes to events, handles keys and issues commands;
- a **native bridge** (a small DLL loaded by a community native loader or a proxy DLL) that owns Prism and the audio engine.

Between them, a **one-way message channel** (script → native) with a tiny line protocol (`say <interrupt> <channel> <ttl> <text>`, `cue <name> <x> <y> <z>`, `listener <x> <y> <z> <yaw>`, `stop`). Find the lowest-latency channel the sandbox allows (a file write the native side intercepts or watches, a log/print sink the native side hooks, a shared memory region if the extender offers one) and **measure** its latency and failure modes before building on it. Keep the channel behind one interface on both sides so it can be replaced when the extender changes.

---

## 8. Speech: Prism

Prism abstracts screen readers (NVDA, JAWS, …) and TTS engines behind one C API. Location:

```
C:\Users\Nitropc\code\libs\prism\
├── prism-windows-x64\include\prism.h                ← the API; ground truth
├── prism-windows-x64\dynamic\release\bin\prism.dll  ← ship with the mod
└── prism-sdk-v0.18.2\doc\index.html                 ← manual
```

Take header and DLL from the same package; `tolk.dll` is not needed; binaries are **x64 and arm64 only** (a 32-bit game needs an out-of-process x64 `SpeechHost.exe` over a named pipe, as in the generic template 8.3). Facts: cdecl; null-terminated UTF-8; C `bool` is 1 byte (`UnmanagedType.I1` in P/Invoke); `prism_init(NULL)` is valid; `prism_registry_create_best` returns an **already initialized** backend; never free library-owned strings; free the backend then `prism_shutdown`; **a backend is not thread-safe**; prefer `prism_backend_output` when `PRISM_BACKEND_SUPPORTS_OUTPUT` (bit 5) is set, else `speak` (bit 2) + `braille` (bit 4); `0` is `PRISM_OK`, `prism_error_string` for logs.

`ScreenReaderOutput` wrapper (the only code that touches Prism):

- Load `prism.dll` by full path; init; `create_best`; log the backend; announce "`<Mod>` loaded".
- `Speak(text, interrupt = false)`: cleanup pipeline (section 20), output, on error log and (rate-limited) recreate the backend so a screen reader started after the game is picked up.
- One thread only; enqueue from elsewhere.
- Log every spoken line verbatim behind a debug switch.
- `Say` for focus echoes and on-demand reads; `SayEvent`/`LogEvent` for unsolicited announcements (recorded in the Events buffer and an event log file next to the mod).
- **Speech scheduler** in front of the wrapper for real-time channels (exploration guidance, combat narration): channel key (newer replaces unsent older), priority, time-to-live, duration estimate from text length and a speech-rate factor. Menus bypass it.

---

## 9. Hooking pattern

- Every hook or event handler is **fault-isolated** (try/catch, `pcall`, SEH). An error escaping a handler can break the game function or the extender event.
- Every hook installation or subscription **logs success or failure**. Grep the log after every launch.
- Record each verified target in `PATCH_TARGETS.md`: name/signature or RVA+bytes, what it is used for, the build and extender version it was verified on.
- Handlers do the minimum (capture, enqueue); text building and speech run in the per-frame update.
- Know whether a hook fires on the **request** or on the **resolution** (an attack declared vs. the damage dealt; a spell cast started vs. its effects applied).
- [managed] Harmony manual patching by name, cached reflection (generic template 9.1). [native] Build gate, `module + RVA` with byte signatures, correct calling conventions (generic template 9.2). [extender] Subscribe by event name; check the event exists at startup and log if it does not; wrap every callback.

---

## 10. Reading focus and text

### 10.1 Focus polling

Poll the UI framework's focused element (tier A) or the controller navigation system's current item (tier B) every frame. Also poll game-side selections that live outside the UI focus: the controlled character, the selected hotbar slot, the hovered/picked entity, the current target.

### 10.2 The extraction chain

`GetTextForElement(element)` tries domain readers, most specific first: special cases; rich game objects (items, abilities, characters, statuses); dialogue options; settings controls with value and state; list entries (journal, codex, map markers); character-creation choices with locked/requirement state; tooltip-only elements; generic label with context; cleaned internal name. One reader per domain.

**MVVM UIs**: read the element's **data context / view model**, not its visual children. The view model holds the item handle, the ability ID, the "is enabled" flag and the tooltip data; the visual tree holds only what is currently drawn.

### 10.3 Traps

The focused element is a child and the meaning is on an ancestor; displayed text differs from stored text (placeholders, format strings, one frame stale); hidden elements keep their text; state is part of the text (equipped, selected, locked and why, not enough gold, unusable and why, "3 of 7"); icon-only buttons need a label map; draw-call hooks give strings without structure.

### 10.4 Navigation layer (tier B and C)

The full pattern is in the generic template 10.4; the rules that matter most in RPGs:

- **Navigation follows meaning, not the screen's layout** (generic template 10.4, "Collecting and ordering items"): sections on Tab / Shift+Tab, items on the arrows, levels on Enter / Backspace, grouped and ordered by what things are and how the game structures them, never by where they are drawn; one key grammar for every screen, menu and mode, the mod's own panels included. Only a spatial tool (a cursor on a grid or a map, aiming, placing) uses the arrows for space.
- **Reuse the game's controller UI when it exists** (tier B): its radial menus, focus navigation and list views are already built for a device without a pointer. Drive its current item, force controller mode if the game only navigates there, and watch its active layer for popups.
- **Per-screen handlers with a generic fallback**; the router picks the handler from game state every frame. A screen with no handler degrades to "clumsy", never "dead keyboard".
- **Screen-open announcement**: the screen's name, any message it is showing (dialog text, a notification, a tutorial hint), then the focused item; **never the list of its tabs, options or controls** (rule 22). The character sheet opens as "Character sheet, Shadowheart, Abilities tab. Strength, 13", not as every stat on every tab; the options screen opens on its first tab and focused control only.
- **Focus is hover is the armed target.** Mouse-driven RPGs act on what is hovered. After every focus move, push the game's hover/picking state onto the focused item; unhover the old first so a refused hover fails closed; re-sync when describing.
- **Activation ladder**: game handler → simulated pointer sequence → press/release path → synthetic OS click at a projected position (last resort).
- **Ask before pressing**: call the game's validation ("can equip", "can afford", "can cast now") and speak the refusal with the reason.
- **Drag and drop becomes pick up and place** (inventory, equipment slots, hotbar, party order): Enter picks up, arrows browse destinations with a target-aware description including "not a valid slot", Enter places, Escape cancels.
- **Context menus and radial menus** become vertical lists.
- **Virtual rows** for content with no focusable item (character sheet numbers, combat log, book pages).
- **Never answer a key with silence**; **controls announce role, value and how to change it**; **rewrite mouse wording** in tutorials into the mod's keys, per language.

---

## 11. Concise focus and review buffers

### 11.1 FocusReadout

```csharp
public class FocusReadout
{
    public string Summary;                            // spoken on focus
    public List<string> Details = new List<string>(); // one buffer item each
    public string FullText;                           // everything in one string, for non-focus callers (help, logs)
}
```

RPG summaries, as a sighted player takes them in at a glance:

| Object | Summary | Details (buffer) |
|---|---|---|
| Item | name, key number (damage, armor value, charges), equipped/usable state, value if trading | type, rarity, requirements and whether met, every property and effect, comparison with the equipped item ("+2 armor, loses fire resistance"), weight, value, flavor last |
| Ability/spell | name, cost (action type, resource, slot level, cooldown), usable now or why not | range, area, damage/effect with dice and current bonuses, save/attack type, duration, concentration or similar, each keyword explained, upgrade/upcast effects |
| Character (ally) | name, HP current/max, key conditions | class/level, every stat with its modifier, resources, all statuses with durations, equipment, skills and proficiencies, approval/relationship as the game shows it |
| Character (other) | name, attitude (hostile/neutral/ally), HP as the game shows it (exact or descriptive), distance/direction in exploration | level/difficulty if shown, visible statuses, resistances if known to the player, what the game's own examine/inspect panel says |
| Status/condition | name, remaining duration | full description, source |
| Dialogue option | option text, check tag and difficulty if shown | who is speaking to whom, requirement explanation |
| Quest (journal) | name, state (active/completed/failed), tracked or not | current objective, every journal entry in order, location and rewards if the game shows them |

Read *current* values from runtime instances, not definitions. **Keyword explanations live only in buffers**, never in live announcements (`includeKeywords` parameter).

`Details` is the **only** home for the rest (rule 19). Every stat, property, description, requirement, relationship state and quest entry a focusable thing has is a line in its details buffer, reached with Ctrl+Up straight after focusing it. A party member focused in the party list, on a portrait or on a character-select screen says name, HP and key conditions; Ctrl+Up reads class and level, the next Ctrl+Up the next stat, then equipment, skills and approval. Never add a hotkey that reads one of those facts.

### 11.2 AnnouncementBuffer and BufferManager

Named lists with a cursor (index 0 newest/top, cursor starts at -1, no wrap, `Add` shifts the cursor, optional refresher, reset the cursor only when content changed). Ctrl+Up/Down within a buffer, Ctrl+Left/Right switch buffers skipping empty ones. **When focus moves to something with details, its buffer becomes the current buffer with the cursor at -1**, so the first Ctrl+Up after focusing an item, ability, party member or quest always reads that thing's first detail line without the player switching buffers first.

RPG buffer set:

- Focus-fed: **UI**, **Item**, **Ability**, **Character**, **Status**.
- Event-fed: **Events** (everything unsolicited), **Combat log** (every roll, hit, miss, damage, save; the game's own combat log if it has one, read from its data), **Dialogue** (the conversation so far, speaker-tagged), **Story** (journal updates, books, letters, cutscene subtitles).
- Refresher-driven: **Party** (each member's HP, resources, conditions), **Turn order**, **Nearby** (entities around the controlled character), **Quest** (active quest and current objective).

Clear focus-fed buffers on screen change; keep Dialogue until the conversation ends (then archive it into Story).

---

## 12. Input

### 12.1 Keys

Route by mode, in strict priority: help browser, mod lists (browser, glossary), targeting/placement mode, battlefield/world review cursor, Ctrl combos, plain hotkeys.

**Extract the game's default bindings before choosing any key** (section 4.2). RPGs bind a lot: hotbar numbers, party selection, UI panels on letters, camera on WASD/arrows, pause on Space, quick save/load on F-keys. Sharing a key with an action that is inert in the current mode is acceptable; never share with a disruptive one (end turn, attack, quick load). Do not take screen reader keys. A single held **mod layer key** (plus letters) often avoids most conflicts.

Baseline mod keys (all rebindable in the mod's settings, section 22):

| Key | Action |
|---|---|
| help | Context help as a browsable list (do not assume F1 is free: many RPGs bind F-keys to party members) |
| re-read | Re-read the focused item's summary |
| read screen | Structured summary of the current screen, on demand only |
| status keys (HUD) | Controlled character (HP, resources, conditions); whole party; gold; time of day / day count |
| where am I | Area name, sub-area, facing, nearby notable things |
| objective | Active quest objective with distance and direction if the game has a marker |
| scan / browser | Scan the surroundings (19.x cues if present); open the entity browser (16.3) |
| combat keys | Turn order; whose turn and resources left; battlefield review; target cycle; end-turn confirmation |
| review buffers | Ctrl+arrows; Ctrl+Up after focusing anything reads its details (11.2) |

**Keep the key set small (rule 19).** A new hotkey is justified only for HUD information or an action. If what you want to put on a key is a fact about something the player can focus (a party member's strength, an item's value, a spell's range, a companion's approval), it is a details line in that thing's buffer instead. Verbosity, direction style, cue volumes and every other preference are settings (section 22), never keys.

### 12.2 Suppression

When the mod claims a key, the game must not also act on it. Find every input path; suppress per key, only while a claim is active; key off physical state for the press that opens a mode; latch `ClosedThisFrame`; never leak suppression onto held movement or camera keys.

### 12.3 "Is this screen really in front?"

Use the game's own notion of which UI owns input. Not a denylist of overlays, not "top of the stack".

---

## 13. Gameplay foundations

### 13.1 State and play modes

- **StateCache**: party, controlled character, managers, entity pools; invalidate on load, level transition, character switch. `IsInGameplay` and `IsInCombat` flags driven by verified start/stop signals, and confirm the stop signal fires.
- **Play-mode tracker** derived from state every frame: exploration, sneaking, dialogue, combat (own turn / other turn / reaction prompt / targeting / placement), trading, looting, camp, cutscene, world map, loading, paused. Announce changes the player did not directly cause ("combat started, you act third", "detected", "dialogue").
- **Event announcements** through `LogEvent`: combat start/end, turn start, attacks and results, damage and healing, statuses, downed/death/revive, item and gold changes, XP and level-up available, quest updates, companion reactions, area entered, discoveries (hidden object found, trap detected, lock picked).
- **Phantom events**: previews (hit chance, damage, path cost, AoE), dialogue-tree evaluation, cutscene setup and script-spawned entities must never be announced. Centralize in one `PreviewGuard`.
- **Floods**: an enemy round can be dozens of events. Coalesce per actor ("Goblin archer: hits Shadowheart for 6, misses you"), keep raw lines in the Combat log buffer, and offer an end-of-round summary.
- **Priorities**: urgent (a party member downed, a reaction prompt, a trap triggered, detection), normal, ambient (barks, flavor). Verbosity drops ambient first.
- **Frame budget**: entity scans, raycasts and path queries spread across frames and cached; mod cost well under a millisecond per frame.
- **Internal names mislead**: factions, attitudes, team IDs, "player"/"hero" enums. Verify by observation.

### 13.2 Spatial foundations

Needed for exploration and combat alike. Establish in the running game and record in `CLAUDE.md`: up axis, units, handedness, heading convention (where 0 points, direction of increase), how the camera yaw relates to screen "up".

- **Reference frame**: in an isometric or free-camera RPG, "left" should mean **left on screen / relative to the camera** by default, because that is how the game and sighted companions describe things; offer compass directions (relative to the game's map north) as a setting (section 22), and include the compass facing in "where am I". In a third-person RPG with a character-facing camera, use camera forward. Pick once per mode, document it, use it in speech and cues alike.
- **Direction language**: clock positions or eight directions; distances in the game's own unit if it shows one (meters, feet, tiles, squares); height when it matters ("on a ledge 4 meters above").
- **World queries** wrapped once in `Exploration/Spatial`: pathfinding (path, length, reachable), tile/navmesh info (walkable, surface/hazard, height), raycast and line of sight, entity enumeration by radius and category, area/region name at a position, map markers.
- **Helpers**: `Describe(point)` → "12 meters, 2 o'clock, 3 meters below"; `PathDistance` vs straight distance ("8 meters away, 20 to walk"); `HasLineOfSight`.

---

## 14. Characters, party and progression

- **Party overview key** (HUD): each member in party order: name, HP, key resources, conditions, "downed", "controlled".
- **Party member details**: everything else about a member (class, level, every stat, equipment, skills, spells known, approval) is details lines in the Character buffer, read with Ctrl+Up after focusing the member in the party list, on a portrait or on the character sheet (rule 19). No per-stat or per-member-sheet hotkeys.
- **Switching character**: announce who is controlled now, their HP and their position relative to the previous character.
- **Group/formation**: whether members follow, are separated ("Lae'zel is 30 meters behind, unlinked"), who is in stealth.
- **Character sheet**: every tab as virtual rows with the full stat breakdown the game offers on hover (base, modifiers, sources).
- **Level-up**: every choice is a list with full descriptions in the buffer, prerequisites stated, and "recommended" if the game marks it. Summarize the pending choices before the first step and all choices before confirming.
- **Character creation**: functional choices (origin, class, abilities, skills, spells, background) get full descriptions and consequences. **Appearance** is dozens of sliders and swatches with no gameplay effect: offer the game's presets and randomize, read each option's name if it has one, and keep per-slider access for players who want it. Never block progress on a visual-only step.
- **Point-buy and allocation screens**: speak remaining points on every change, the effect of the value ("16, plus 3 modifier"), and why a button is disabled.
- **Rest and resources**: what a rest restores, what it costs (supplies, time), and what is currently depleted.
- **Companions and reputation**: reactions and approval changes are events (normal priority); the current relationship state, as the game shows it, is a details line of each companion (Ctrl+Up on the companion), not a key of its own.

---

## 15. Dialogue and narrative

Dialogue is where most of an RPG's content lives. It must be flawless.

- **Lines**: speaker name, then line text, spoken as each line appears; the whole conversation accumulates in the Dialogue buffer. Voice acting is audible, but narration, unvoiced lines and narrator descriptions of actions are not: speak them all (setting: speak voiced lines too, default on, so that the speaker is always named).
- **Options**: announce "N options" when the choice appears, then read the focused option. Each option: its number if the game supports number keys, the text, and every **tag** a sighted player sees: skill or ability check with its type and difficulty, class/race/background-exclusive markers, "leave", "attack" (a hostile ending), "[continue]", greyed-out/unavailable options with the reason, previously chosen options if the game marks them.
- **Checks and dice**: if the game shows a roll (dice animation, target number, modifiers, advantage/disadvantage, bonuses like inspiration or a spell), announce the setup ("Persuasion check, difficulty 15, your bonus +5, advantage"), make every pre-roll choice keyboard-reachable (add bonus, use a reroll resource), speak the rolled value, total and outcome, and keep it all in the Combat log/Dialogue buffer.
- **Reactions**: "Karlach approves" style popups are events; the conversation continues.
- **Timed choices**: say how long remains, or better, find the game's own option to disable the timer.
- **Cutscenes**: announce start, skip key, speak subtitles, announce when control returns.
- **Books, letters, notes, inscriptions**: read into the Story buffer, paginated by sentence.
- **Journal**: quests as a list (active, completed, failed), each with its entries in order; codex/glossary entries as a list; the tutorial log.
- **Barks** (ambient NPC chatter): ambient priority, off at low verbosity.

---

## 16. Exploration

A sighted player sees the room at a glance: exits, NPCs, glinting loot, suspicious floor tiles, a lever on the wall, the quest marker at the edge of the screen. Replace that glance.

### 16.1 Moving

- **Click-to-move** games: the mod issues "move to" through the **game's own move command** for the controlled character (the party follows as it normally would). The destination comes from the entity browser, the world cursor, a map marker or the guidance target. Announce departure, arrival, and interruption ("path blocked", "stopped: detected").
- **Direct movement** (WASD/stick): support it, and add the shooter-style guidance (16.4) and sensing (16.5).
- **Movement ladder**: the game's player move command → the scripting layer's move-to-position → cursor warp plus a click on the projected point, verified by reading what the game's picking/hover system says is under the cursor before clicking.
- **Unreachable**: if the game's pathfinder says no path, say so and why if known (height, closed door, gap that needs a jump).
- **Traversal**: jumps, climbs, ladders, doors, gaps. If the game shows a special cursor or prompt for these, turn it into a spoken option.

### 16.2 World cursor

A virtual, read-only cursor on the ground (tiles, grid cells or fixed steps), moved by arrows with a large-step modifier, starting on the controlled character. It announces what is there: surface and hazards, walkability, height change, objects and characters, and path distance from the character. Keys: jump back to the character, "go here" (explicit action), "interact with what is here", "look": full description.

### 16.3 Scan, entity browser and "where am I"

- **Entity browser** (a list): categories **people** (hostile, neutral, companions, traders, quest givers), **containers and loot**, **doors and exits** (including area transitions and their destinations), **interactables** (levers, books, shrines, crafting stations), **hazards and traps** (only those the party has detected), **map markers and quest targets**. Sorted by path distance. Each entry: "name, state, distance, direction" ("chest, locked, 6 meters, 10 o'clock"). Enter: walk there and interact, or open an action list (talk, attack, pickpocket, lockpick, examine, pick up, throw) built from the game's own "available interactions" query, including unavailable ones with the reason.
- **Honor the fog of war and perception rules**: list only what a sighted player could currently see or has discovered. A hidden trap or secret door is found by the game's perception checks, and then announced as a discovery. Revealing hidden things is an assist (rule 11), not information.
- **The game's own "highlight interactables" key** (many RPGs have one) is the reference for what counts as visible and interactable. Find its query and use it.
- **Scan key**: a spoken summary ("3 hostiles, 2 containers, 1 exit north") and, if cues are enabled, a positioned sound per entity in distance order, pitch encoding height (19).
- **Where am I**: area and sub-area name, indoors/outdoors, facing, standing on (surface/hazard), nearest exit, nearest point of interest, party members nearby.

### 16.4 Guidance

- **Guidance target**, one at a time: the tracked quest's marker, a custom map marker, the last browser choice, the nearest of a category. A key speaks it with path distance and direction; another cycles the source.
- **Follow the path**, not the straight line: request the game's path to the target and guide along it (turn instructions: "doorway ahead", "stairs down", "turn left in 5"); recompute when the player strays; announce arrival and what is there.
- **Walk there** (explicit action, and in click-to-move games the default): issue the game's move command to the next reachable point on the path, repeated until arrival, so the game's own party movement, stealth and trap-avoidance rules apply.
- **Area transitions**: announce the destination before taking them ("to the Underdark, confirm?") if the game asks for confirmation; after loading, announce the new area.

### 16.5 Sensing and safety

- **Hazards on the path**: before moving, check the game's own path preview for surfaces, traps, enemies' sight, opportunity attacks or falls, and warn ("path crosses fire").
- **Detection**: enemies' awareness and vision (sneaking), "you are being watched", "detected" as urgent events.
- **Stuck detection**: a move command that made no progress for a second → "blocked", plus the nearest alternative route.
- **Falls and drops**: warn when a jump or move target would cause fall damage if the game computes it.

### 16.6 Map and fast travel

- The **map screen** gets a marker browser (quests, discovered locations, fast travel points, traders, custom markers) and a review cursor that speaks area names and nearby markers.
- **Fast travel**: a list of discovered points, with the current one marked; confirm announces the destination.
- **Custom markers**: set one on the world cursor or a browser entry, with a name.

---

## 17. Combat

### 17.1 Turn-based combat

- **Combat start**: "combat, 5 enemies, you act second", plus turn order into the Turn order buffer.
- **Turn start**: whose turn; for the player's characters: HP, resources (actions, bonus actions, movement, action points, spell slots), conditions, nearest threats ("2 enemies in melee range"), and anything urgent (concentration at risk, standing on a hazard, low HP).
- **Enemy turns**: coalesced narration per actor (moves, attacks, results), urgent events first; the full rolls in the Combat log.
- **Battlefield review cursor** (roguelike board review, on the combat map): read-only, jumps between combatants in turn order or by proximity, announcing name, HP, conditions, distance and direction, elevation relative to the controlled character, line of sight, cover or high ground, what it is threatening. Also tile-level stepping for surfaces and hazards.
- **Targeting mode**: after choosing an ability, arrows or number keys cycle **valid targets** (the game's own validity check), each announced with the game's own preview: hit chance, expected damage or effect, save type and DC, advantage/disadvantage and why (flanking, high ground, darkness), whether allies are in the area, range. Enter confirms, Escape cancels. The previews are phantom-event territory (13.1).
- **Area placement**: for area abilities, a placement cursor on the ground (world cursor rules) announcing who would be affected ("hits 3 enemies and Gale"), whether the center is in range and in line of sight; cycle to "centered on target X" shortcuts; Enter casts.
- **Movement in combat**: a movement cursor announcing path cost vs remaining movement, whether the destination is reachable this turn, and what the path triggers (opportunity attacks, hazards, falls, entering enemy sight); a "move to target's melee range" shortcut; jump/dash/disengage options if the game has them.
- **Reactions and interrupts**: when the game pauses to ask ("use Shield?", "opportunity attack?"), speak it with urgent priority and make every option reachable; if the game offers "always/never ask" settings, point to them.
- **End turn**: confirm when resources remain unused ("end turn with 1 action and 6 meters unused?").
- **Death saves, downed allies, revivals**: urgent events, and the party overview marks downed members.
- **Combat end**: result, loot available, XP, level-up available.

### 17.2 Real-time-with-pause combat

- Encourage the game's **auto-pause** options (on combat start, enemy spotted, character downed, spell cast, end of round); make them reachable and recommend them in help. Announce every auto-pause and its reason.
- While paused, the player gives orders per character using the targeting mode above; the mod reads the **queued orders** per character.
- A **round/tick summary** after each unpause segment: who did what, damage, conditions.
- Offer turn-based mode if the game has one (rule 11: the game's own option).

### 17.3 Combat assists

Optional, disclosed, decided with the user: auto-end-turn, the game's own difficulty and "story mode", AI control of companions if the game offers it, "auto-target nearest valid". Many blind players want the tactics kept and the information added.

---

## 18. Inventory, equipment, trade and loot

- **Grids become lists**, grouped the way the game groups (tabs, categories, containers), sortable by the game's own sort options; each item's summary as in 11.1; "N of M" position.
- **Item facts are details, not keys** (rule 19): properties, requirements, comparison with the equipped item, weight, value and flavor are lines in the Item buffer, read with Ctrl+Up after focusing the item. No "read value", "read weight" or "compare" hotkeys. Total carried weight and gold are HUD and may keep keys.
- **Equipment**: the paper doll becomes a slot list ("main hand: longsword, 1d8+3"); equipping announces the change and what it replaced; comparison in the buffer.
- **Pick up and place** for moving items between characters, containers and slots; the game's own "send to", "equip", "use", "drop", "split" actions via the item's context list.
- **Weight and encumbrance** after every change if it crosses a threshold.
- **Looting**: "loot all" and per-item, with value and weight; announce empty containers.
- **Trading**: two lists (theirs, yours), price and whether affordable, running balance of the offer, the trader's gold, and the game's attitude/price modifiers.
- **Hotbar**: a list per row; assign by pick-up-and-place; each slot says ability or item, cost and usable state.
- **Consumables and scrolls**: usable state and why not.
- **Crafting**: recipes as a list, ingredients owned/missing, result description.

---

## 19. Audio cues (optional but recommended for exploration)

Speech is enough for menus, dialogue and turn-based combat. Exploration in 3D benefits from sound: where the exit is, which way the path turns, where the enemies are standing.

- **Channel rule**: if it is about *where*, or changes faster than once a second, it is a cue; if it has words, it is speech.
- **One mapping for the whole mod**: pan = horizontal direction in the reference frame; pitch = height relative to the listener; repetition rate = closeness; timbre = category; muffled = behind. Document in `CUES.md`.
- **Cue types**: one-shot (discovery, hit, turn start, item picked up), beacon (guidance path point, scanned target), scan sweep (a burst of positioned one-shots after the scan key), detent (camera crossing the path direction).
- **Engine choices**: the mod's own stream (miniaudio in the native half; independent of game ducking, needs positions sent from the game side) or the game's own audio middleware if the extender exposes positional playback (free spatialization matching the game's, but ducked and paused with the game's mix). Measure both; decide with the user.
- **Glossary** list with previews; per-family toggles and a preset in the mod's settings (section 22); an instant-mute key.
- In turn-based combat, cues are optional flavor; speech carries the information.

---

## 20. Text pipeline, keywords, localization

All speech passes through one cleanup function: decode once at the boundary; turn inline icon/glyph tags into words (damage types, dice, status icons, controller button glyphs into the player's actual keys); strip markup; resolve leaked localization handles and unfilled placeholders (never speak `{0}` or a raw handle); rewrite mouse wording (10.4); collapse whitespace.

**Dice notation**: speak "1d8 plus 3" (a setting offers "1 d 8"), ranges "4 to 11", modifiers with sign.

**Localization**: find the game's single "translated string" call and wrap it in `TryLocalize(handleOrId)`. Prefer asking the game for the final display string of an object (item, ability, status, character name) over assembling it yourself. The mod's *own* strings (announcements, role words such as "slider" and "checkbox", direction words, dice wording, help text, hints, settings labels, the glossary, rewritten mouse wording) are centralized in one table from day one, keyed, with English as the fallback, and never concatenated from fragments in code (word order differs between languages; use format strings with named placeholders).

- **Every language the game supports, not one** (rule 21). List the game's languages in `CLAUDE.md` during recon (from its language setting and its string tables). Developing in English only is fine; **releasing is not**: a release ships the mod's table translated into every one of those languages.
- **Follow the game's language setting**, read at startup and again whenever it changes; the mod's settings may offer an override, never a requirement.
- Test every language at least once in the running game before release: strings that overflow, encodings that break, plural forms, and rewrite patterns (10.4) that only match English.
- A missing key in a language falls back to English **and logs a warning**; the release check fails if any language has missing keys.

**Keyword glossary**: build it from the game's own descriptions of conditions, damage types, mechanics and rules terms at runtime. Hardcode only what the game never explains.

---

## 21. Help system

`IHelpContext` per screen and play mode; highest-priority active context wins (reaction prompt > targeting > combat > dialogue > exploration). The help key opens a browsable list. Help text lists the game's keys for the context as well as the mod's, and names the active cues. Each screen's open announcement ends with a one-line hint, suppressible by verbosity.

---

## 22. Configuration

Settings: verbosity, every hotkey, braille, per-category announcement toggles (barks, companion reactions, dice details, enemy turn detail), direction style (clock/compass/screen-relative), units, dice wording, speech-rate factor, cue families and volumes, language override, and every assist (off unless chosen). **Players change them inside the game, never with hotkeys and never by editing a file** (rule 20).

1. **A tab in the game's own settings UI** whenever the mod can create one: find how the options screen builds its tabs and controls (section 4.3), and add a "<Mod>" tab made of the game's own slider, toggle and dropdown controls, so it looks, sounds and navigates like the rest of the options and works for sighted helpers too. [extender] Some extenders or community mod-config frameworks already provide an in-game settings panel; use it when it is keyboard-reachable and the mod can read it.
2. Only if the options screen cannot host a tab: a **"<Mod> settings" entry** in a menu the player already reaches (the options menu, the pause menu, the main menu) that opens a mod screen built on the navigation layer (10.4), with the same key grammar and control announcements as every other screen.
3. Every setting announces its label, role, current value and how to change it (10.4), its help line goes in its details buffer, and changes apply immediately (no reload, no restart unless the game itself requires it, and then say so).
4. Settings labels and option names are mod strings: localized like everything else (section 20).
5. Persist to the loader's or extender's per-mod settings, or a file next to the native half. That file is storage: tolerate bad values with a logged warning and a default, but never document it as the way to configure the mod. The README describes where the settings tab is.

---

## 23. Playtesting: drive the game yourself

A blind player will meet every screen, every odd corner of every map and every rare combat situation in the game. The only way to find the dead ends first is to **play the game yourself, completely**, twice: once *sighted* (screenshots) to map it, once *blindfolded* (mod output only) to prove it. No part of the mod is done because it compiled or because a unit test passed.

### 23.1 The harness: eyes, hands, ears, memory

Build this in the first week and keep it in `tools/playtest/`. Every piece is a script, so a session can be rerun.

| Need | Implementation |
|---|---|
| **Eyes** | Window capture of the game only (PowerShell `CopyFromScreen` on the window rect, Python `mss`, or `PrintWindow`; some GPU-rendered windows need desktop duplication instead of `PrintWindow`, so test both). Save as `shots/<session>/<nnnn>_<label>.png`, downscaled if large, and read them. A computer-use tool that screenshots and clicks is also fine, when available. |
| **Hands** | `SendInput` with **scancodes** (games using raw input, SDL or DirectInput ignore posted messages and virtual keys), with realistic hold times (30–80 ms) and gaps; mouse moves in absolute window coordinates for the sighted pass. Better still, the extender's own key-injection and console commands, which do not need window focus. Bring the window to the foreground before synthetic OS input and verify it is foreground. |
| **Ears** | The mod's spoken-line log and event log, tailed per step. Every step of a script records "keys sent → lines spoken → screenshot". |
| **State probe** | Console or bridge queries answering the ground truth: current screen/UI state, play mode, controlled character, position, HP, combat state, open dialogue node, selected target. Assertions compare mod speech against this, not against memory. |
| **Memory** | `PLAYTEST.md`: a running log of sessions (date, build, save used, route, findings with screenshot references) and the **coverage map** (23.2). |
| **Reset** | Load a named save, or a console command sequence, to return to a known state in seconds. Kill-by-PID and relaunch when the game hangs; detect hangs (no new frame / no log activity for N seconds) automatically. |

Rules for the harness: never run blind-input scripts while the game window is not verified to be in front; always stop the script when an unexpected screen appears (compare a screenshot or UI state against the expected one) instead of sending more keys into the unknown; log every key sent.

### 23.2 Coverage map

`SCREENS.md` lists *what exists*; the coverage map in `PLAYTEST.md` tracks *what you have exercised*, by axis:

- **Screens and modals**: every row of `SCREENS.md`, opened, navigated end to end, every control operated at least once, closed by every way it can close (confirm, cancel, Escape, the game closing it itself).
- **Play modes**: every row of the play-mode table, entered and left in every way the game allows (combat started by the player, by enemies, by dialogue; ended by victory, flight, wipe, surrender).
- **Map spots**: per area, a list of **navigation spots**: every exit and transition, every door, ladder, jump point and climb, every trader, quest giver and companion, every waypoint/fast travel point, every trap and hazard zone, every dead end, narrow passage, multi-level structure, elevator, water and unreachable-looking ledge. Each spot: reached with mod guidance only? announced correctly on arrival? interaction worked?
- **Content**: every quest, every companion's recruitment and key scenes, every trader, every class/subclass at least through level-up screens, every kind of check (skill, save, attack, dialogue dice).
- **Edge cases** (23.4).

A row is covered when there is a screenshot, a speech log excerpt and a verdict. Uncovered rows are the next session's route.

### 23.3 Save library

Saves are disposable, so use them aggressively:

- Quick-save before every risky step; reload on anything unexpected rather than improvising.
- Keep a curated **milestone library** (`saves/NN_description`, copied out of the game's save folder with a text file noting build, party, location and what it is for): fresh character creation start, end of tutorial, each act or chapter start, before each boss, each trader, each companion recruitment, a pending level-up for every class, a fight with hazards, a fight with reactions, a stealth section, a party wipe about to happen, the final area, the credits trigger.
- Use the extender console or debug helpers (behind a debug flag) to manufacture rare states quickly: set HP to 1, apply a status, spawn an enemy group, give an item, set a quest flag, teleport. Note which states were manufactured: a manufactured state can hide a bug in how the game normally reaches it, so each major one also gets reached naturally at least once.

### 23.4 Edge-case matrix

Test each item on every screen or mode where it applies. These are where blind players get stuck.

**Menus and UI**

- Empty lists (empty inventory, no quests, no traders' goods, no spells prepared), one-item lists, very long lists (hundreds of items: scrolling, performance, position announcements).
- Disabled and locked controls; greyed-out options with and without a reason; unaffordable items; requirement not met.
- Modals on top of modals (confirmation over trade over inventory); modals that appear **during** combat or dialogue; the game closing a screen by itself (combat starting while in inventory, a cutscene triggering).
- Screens opened by the mouse wording in tutorials; tooltips that only exist on hover; context menus with nested levels.
- Text fields (character name, save name, custom marker): typing, editing, mod hotkeys inactive while typing.
- Resolution/UI-scale changes, alt-tab and focus loss, controller plugged in mid-session (the game switching UI mode under the mod).
- Language switched to every language the game ships (at least a smoke pass): no raw handles, no English-only rewrites breaking, the mod's own strings present and following the switch.

**Exploration**

- Paths that cross hazards, go through closed or locked doors, need a jump or a climb, lead up or down levels; destinations that are unreachable; targets on another floor directly above or below.
- Party members separated, stuck, left behind across a transition; switching control between them mid-route.
- Walking into a trap, getting detected while sneaking, an ambush starting combat mid-move.
- Area transitions, fast travel, loading screens, returning to an area; dying from a fall.
- Dark areas, invisible or hidden entities (the mod must not reveal what the party cannot perceive), interactables that appear only after a perception check.
- Crowded scenes (camp, city streets): entity browser sorting, performance, bark floods.

**Dialogue**

- Every option type: check tags, class/race/background-exclusive, unavailable, leave, hostile endings; options that start combat; dialogues started by an NPC walking up; dialogues with several speakers; companion interjections; dialogue during combat.
- Dice checks: success, failure, critical success and failure, advantage/disadvantage, bonuses and rerolls, a check with no roll shown (passive).
- Very long lines; unvoiced narrator lines; lines that auto-advance.

**Combat**

- Starting combat every way; surprise; joining an ongoing fight; fleeing.
- Every action type: melee, ranged, spell with single target, multiple targets, area placement, self, ally, ground position, summon, item use, throw, shove/jump/dash equivalents, end turn with resources left.
- Invalid targets (out of range, no line of sight, wrong type), partially obstructed area placement, friendly fire.
- Reactions and interrupts, for the player and against the player; concentration broken; turn skipped; incapacitated characters; summons and temporary party members.
- Downed allies, death saves, revive, a party wipe and the game-over flow.
- Enemies with long turns (many actions), many enemies (turn-order length), hidden enemies, enemies on other levels, enemies that flee or become allies.
- Combat ending while a mod mode (targeting, review cursor) is open.

**Inventory, trade, progression**

- Full inventory, overweight, stack splitting, moving between characters and containers, equipping two-handed items over shields, items the character cannot equip.
- Trade with not enough gold, selling quest items, buyback.
- Level-up for every class with every kind of choice; respec if the game has it.

**Robustness**

- Save and load in every mode (during dialogue, mid-combat, in a trade), autosave triggers, loading a save made without the mod.
- Screen reader not running, started after the game, restarted mid-session; braille on.
- The game patch or extender version changing (build gate announces it).

### 23.5 Sighted pass, then blindfold pass

**Sighted pass (mapping)**: play with screenshots, using mouse and keyboard as a sighted player would, to learn each screen and area and fill `SCREENS.md` and the navigation-spot lists. Log the mod's speech alongside; every mismatch between screenshot and speech is a finding.

**Blindfold pass (proof)**: from a milestone save, play **using only the mod's keys and the game's keyboard bindings, and deciding only from the spoken-line log** (do not look at screenshots to decide; capture them only for the report). If you cannot tell from the speech what to do next, that is a bug, even if a screenshot would have made it obvious. Record every point where you needed information the speech did not give, every silent key, every time Enter acted on something unannounced. After the session, compare with screenshots to explain each finding.

Order: sighted pass of an area → fixes → blindfold pass of the same area → fixes → next area. Keep going until a **blindfold run covers the whole story, from the main menu through character creation to the credits.** Chunk it by milestone saves; each chunk's run log goes into `PLAYTEST.md`.

### 23.6 Scenario checks and automation

- **Scenario checks** run from a debug key or console: "enemy placed at the character's left → described as left / 9 o'clock", "chest behind a wall → path distance longer than straight", "area placed on X → affected list matches the game's own highlight", "attack preview hovered → no damage event logged", "move command to an unreachable point → 'no path' spoken". Rerun all after every change to spatial, targeting or event code.
- **Replayable routes**: once a route through an area works blindfolded, save it as a script (keys and expected speech/state per step) and rerun it after changes as a regression test. Expect flakiness from animation timing: wait on state, not on fixed sleeps.
- **Crawl scripts** for menus: from each screen's handler, visit every item, activate every safe control (skip destructive ones: delete save, quit, sell), and log label, role and state; diff against the previous run.

### 23.7 Debug loop

1. Build and deploy; launch; check the logs for hook/subscription failures first.
2. Unreadable element: dump its path, type, ancestors and view model/fields; go back to the extracted code or data.
3. When behavior contradicts your model, measure (console query, logging hook, breakpoint).
4. **Development helpers** behind a debug flag, never in the release.
5. You cannot hear the output: logs are your ears. Ask the user (or a blind tester) to confirm what was actually spoken when timing, ordering, voice-over overlap or screen reader behavior matters, and to do their own monitor-off sessions at each milestone.
6. After a crash, decide whether the mod caused it (repro without the mod) before continuing; record in `CLAUDE.md`.
7. Clean up: the game, debuggers and scripts are stopped at the end of every session.

---

## 24. Pitfalls checklist

- Hook target, event name or scripting call guessed instead of read.
- Preview/simulation (hit chance, damage, path cost, AoE, dialogue condition evaluation) firing announcements.
- Game changes selection or controlled character without a key press; poll state.
- MVVM: reading the visual tree instead of the view model; view model reused by a pooled list item and read for the wrong entry.
- Focus moved but the game's hover/picking did not: Enter acts on something the player never heard.
- Announcing things the party cannot see (hidden traps, invisible enemies, undiscovered doors): an unintended assist and a spoiler.
- Screen-relative directions in one feature and compass in another; heading convention assumed instead of verified.
- Enemy turns narrated line by line with no coalescing; the player's own turn buried under the tail of the enemy's.
- Dialogue options read without their check tags, requirement markers or unavailable reasons.
- Dice roll overlays with a mouse-only "click to roll".
- Reaction prompts that time out or pause the game waiting on an input the keyboard cannot produce.
- Character creation blocked on a visual-only step.
- Drag-only inventory operations with no keyboard path; hotbar assignable only by drag.
- Mod keys colliding with party selection, hotbar numbers, quick load, end turn.
- Mod data written into saves; saves broken when the mod is removed.
- Co-op sessions not detected; the mod acting on another player's characters.
- Extender or game patch changes an API: the mod fails silently instead of logging "not found".
- Script half ↔ native half channel stalls or reorders messages under load; speech lagging seconds behind the game.
- `interrupt = true` anywhere; keyword explanations in live announcements; stale buffer content after a screen change.
- A hotkey per fact ("read strength", "read item value", "read approval") for something the player can focus, instead of a details line under Ctrl+Up.
- Ctrl+Up after focusing a party member or item reads the previous buffer, because the focused thing's buffer was not made current.
- Settings changed by hotkeys or by asking the player to edit a config file, when the game's options screen could host a mod tab.
- A screen's opening announcement reading every tab and option on it (the whole character sheet, every options tab).
- A notification, tooltip or tutorial hint that appears on screen ("quest updated", "inventory full") and is never read.
- Mod strings only in English at release; strings concatenated from fragments; a rewrite pattern that only matches English.
- Prism called from two threads; `bool` marshalled as 4 bytes; wrong-bitness `prism.dll`.

---

## 25. Definition of done

- [ ] Every row in `SCREENS.md` (screens and play modes) is **verified**, and every row of the coverage map and edge-case matrix in `PLAYTEST.md` has a screenshot, a speech excerpt and a verdict.
- [ ] A **blindfold run** (mod keys and speech log only) has covered the whole story from the main menu to the credits, in milestone chunks, with every finding fixed or recorded as a known issue.
- [ ] The story is playable from character creation to the credits with the monitor off, including every combat type, dialogue with checks, trading, level-ups, companions, camp/rest and fast travel; party wipes and failed checks covered.
- [ ] The player can always answer, without sight: where am I, what is around me, how do I get to my objective, who is in this fight and where, whose turn is it and what can I do, what will this action do before I do it, what just happened, what does this item or ability do, what is my party's state.
- [ ] Every hover-only, color-only, icon-only and position-only piece of information has a keyboard path or an announcement.
- [ ] What Enter acts on is always the item or target last announced; invalid targets and unavailable actions are labelled with the game's reason.
- [ ] No mouse needed anywhere: click-to-move, drag and drop, context menus, radial menus, dice rolls and reaction prompts all have keyboard paths.
- [ ] No phantom announcements from previews; no announcements of things the party cannot perceive.
- [ ] No double actions; no silent keys; generic fallback on every screen without a handler.
- [ ] Buffers fill and clear correctly; help is accurate for every screen and mode.
- [ ] No obscure keys: every fact about a focusable thing (party member, item, ability, quest) is reached with Ctrl+Up after focusing it; dedicated keys exist only for HUD information and actions.
- [ ] Every mod setting is changed in the game: a tab in the game's options when possible, otherwise a mod settings screen reachable from the game's menus. No setting needs a hotkey or a file edit.
- [ ] The mod's strings exist in **every language the game supports**, with no missing keys, and each language was checked in the running game.
- [ ] Screens open with their name, any message they show and the focused item only; notifications, tooltips and hints are read when they appear.
- [ ] Works with NVDA and with no screen reader (TTS fallback); recovers when the screen reader starts later.
- [ ] Logs free of hook failures and recurring errors; unsupported game or extender versions are detected and announced.
- [ ] Removing the mod leaves every save loadable.
- [ ] `CLAUDE.md`, `PATCH_TARGETS.md`, `SCREENS.md`, `CUES.md` (if cues), `STRUCTS.md` (native) and a player-facing README (supported versions, prerequisites such as an extender or loader, install steps, full key list, where the in-game settings are, assists disclosure) are current.
- [ ] Release bundle contains only the mod's own files plus `prism.dll` with Prism's `LICENSES` and `NOTICE`; frameworks the player installs themselves are linked, not redistributed unless their licenses allow it.
