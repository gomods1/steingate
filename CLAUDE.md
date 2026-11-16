# STEINS;GATE access project

This folder is a mod that makes STEINS;GATE (Steam, 2016 MAGES port) playable by
blind players. It was set up with the game-access-kit plugin and follows the
playbook `rpg.md` in this folder (an RPG playbook; for this visual novel the
dialogue, menu, review-buffer, help, localization and playtest sections apply,
exploration and combat do not). Its memory is `NOTEBOOK.md`, in this folder.

## Player

- Level: modder who codes (assumed from the prepared playbook and Prism library;
  not yet confirmed by the player)
- Language: English. Always reply in this language.
- Screen reader: NVDA (assumed; not yet confirmed)

## Talking to the player

The player may be blind and read everything through a screen reader. Write for that:

- Lists and short paragraphs. Avoid tables and ASCII art in replies.
- Lead with the answer or the result, then the detail.
- A command the player must copy goes alone in a code block.
- Concise and technical: name files and functions.

## How we work

- Explain, then ask, before changing the game folder beyond installing the mod's
  own files (back up first), installing system software, or editing system settings.
- Facts come from sources: never write a game function, address, field or file
  path that was not found in the game files, a dump, CoZ/impacto source, or a log.
  Anything unproven is marked HYPOTHESIS.
- Game updates break mods: the build gate in the mod checks the Game.exe SHA-256.

## Project layout

- `src/` mod source (C++, 32-bit DLL `SteinsGateAccess.dll` + x64 `SpeechHost.exe`).
- `re/` gitignored reference: CoZ LanguageBarrier, sghd-patch, sc3tools, impacto,
  extracted scripts. Never shipped.
- `tools/` extraction and playtest scripts. Not shipped.
- `data/` shipped data: description tables for backgrounds, sprites, CGs, movies.
- Docs: `NOTEBOOK.md`, `ROADMAP.md`, `STATUS.md`, `SCREENS.md`, `PATCH_TARGETS.md`.

## Notebook rules

Read `STATUS.md` and `NOTEBOOK.md` at the start of every task; update both before
ending. Replace stale lines rather than stacking new ones.
