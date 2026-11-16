# Steins;Gate Access

A screen reader mod for STEINS;GATE (Steam, 2016 release). The game's voices
are Japanese and all of its English text is on screen; this mod reads that
text through your screen reader (NVDA, JAWS, or Windows voices when no screen
reader runs), and describes what the pictures show.

## What it reads

- Every dialogue line, with the speaker's name.
- Audio descriptions of every background, event picture and insert image (507
  pictures), spoken when a picture appears, and characters entering and
  leaving. A character's name is used once the story has named them; before
  that, their appearance is described.
- The date when it changes.
- The movies' burned-in subtitles and credits (prologue).
- Okabe's phone: phone up and down, the menu, Inbox and Outbox, mails (sender,
  subject, text), the address book, calls, the dial pad and every key pressed,
  reply words in mails (not yet tested, see Known limits).
- Text shown inside scenes: the @channel message board, computer screens.
- New TIPS, tutorial boxes, confirmation dialogs and their OK / Cancel choice.
- Menus: title menu and submenus, System menu, Save and Load slots, Config
  (all pages and values), Backlog, Tips list, Clear list, Help.
- Skipping (E, or holding Control) stays quiet; the line where skipping stops
  is read, with the scene.

## Install

With the installer (recommended): download `SteinsGateAccessibilityInstaller.exe`
from the latest release at https://github.com/gomods1/steingate/releases and
run it. It finds the game through Steam (or lets you browse to it), downloads
the latest version of the mod, installs it, and later updates or uninstalls
it. Its window works with a screen reader; `--cli` gives a terminal version.

By hand, from the release zip `SteinsGateAccess-<version>.zip`:

1. Extract the zip into the game folder, next to Game.exe
   (`...\steamapps\common\STEINS;GATE`): it holds `dinput8.dll` and the
   `SteinsGateAccess` folder.

Then, either way, start the game with `SteinsGateAccess\SGALaunch.exe` (Steam
must be running). It replaces the game's own launcher, whose buttons a screen
reader cannot use.

To start from Steam's Play button instead, set the game's launch options
(Properties, General, Launch options) to:

```
"S:\steam\steamapps\common\STEINS;GATE\SteinsGateAccess\SGALaunch.exe" %command%
```

(use your own path). Started any other way, the game runs without the mod.

Why a launcher: the folder name "STEINS;GATE" contains a semicolon, which
stops Windows from loading the mod from the game folder. SGALaunch.exe creates
a folder link without the semicolon (`%LOCALAPPDATA%\SteinsGateAccess\game`)
and starts the game through it. Nothing is injected or patched.

## Keys

Mod keys:

- R: repeat the last dialogue line.
- D: describe what is on screen: the date, the picture, the characters and
  what the phone shows; in a menu, the menu's text.
- Control+Up and Control+Down: move through the current history list.
- Control+Left and Control+Right: switch list: dialogue history, scene
  descriptions, events, details.
- H: the mod menu, with help and settings (Up, Down, Enter, Escape).

Holding Control alone still fast-skips: the mod passes it to the game after a
third of a second.

Game keys: Enter next line or select; arrows move; Backspace back, or hides
the text window in dialogue; E skip mode; F3 or Delete auto mode; F2 or Insert
backlog; F4 tips; F5 load; F6 save; F7 quick load; F8 quick save; F9 config;
F10 title; 1 System menu; Z or C the phone (Phone Trigger); Q and E change
pages in menus; Escape quit.

The title menu has nothing selected until you press an arrow key.

## Settings

Open the mod menu with H. Each setting says its state; Enter or Left and Right
change it, and it is saved at once:

- Say speaker names; remove quote marks.
- Describe backgrounds and pictures; announce characters.
- Read movie subtitles; announce new TIPS.
- New line interrupts speech (off: lines queue, as with every other
  announcement; NVDA still stops speaking when you press a key).

## Known limits

- Important: replying to mails by choosing a highlighted word is the game's
  only way of making story choices, and it decides the ending. The mod reads
  those words, but this was not yet tested in the game, so choosing a reply
  may not work yet.
- The opening and ending songs' lyrics and the short insert movies are not
  yet described.
- Greyed-out (unavailable) System menu items are read like the others.
- The Album, Music and Movie extras (after finishing the game) have no reader
  yet: their screen name is announced.
- English only: the game itself is English.

## Uninstall

Run the installer and choose Uninstall, or by hand: delete `dinput8.dll` and the `SteinsGateAccess` folder from the game folder,
and the folder `%LOCALAPPDATA%\SteinsGateAccess`. Saves are not touched by the
mod.

## Credits and licences

- Speech: Prism (MPL-2.0), licences in `SteinsGateAccess\LICENSES`.
- Character table (`data\charset.utf8`) from Committee of Zero's sc3tools
  (MIT).
- Hooking: MinHook (BSD-2-Clause).
- Byte signatures for this game build come from Committee of Zero's
  STEINS;GATE patch research (LanguageBarrier, sghd-patch); script knowledge
  from their MgsScriptTools and impacto projects.
- Picture descriptions and movie transcripts were written for this mod.
