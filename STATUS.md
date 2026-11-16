# Status

## Current state (2026-10-24, session 1)

Game: STEINS;GATE Steam buildid 4434295 (Game.exe SHA-256 95C5A7BA...6FF7).
Test saves: slot 1 (7/28 lab, before the first Phone Trigger tutorial),
slot 2 (just before the PhoneWave dial pad), 3 (after it), 4 (7/29, call
Daru), 5 (after the blank mail to Moeka), 6 (May Queen cafe), 7 (@channel
board scene).

Verified in the running game (speech from spoken.log, NVDA backend):
- Start: SGALaunch.exe -> junction -> dinput8.dll proxy loads; all signatures
  resolve (with a retry for SteamStub's decrypt race); speech host on NVDA.
- Dialogue: every line spoken with speaker name ("???: Hey, what are you
  mumbling about?"); force skip (Ctrl) and skip mode (E) stay quiet and speak
  the line where skipping stops, plus a scene summary.
- Scene descriptions: 507 backgrounds/CGs/insert images described
  (data/bg.tsv); announced when a picture becomes visible (layer flags 2400+i),
  not when preloaded; character appear/leave on layer flags 2410+i
  (HYPOTHESIS, not yet seen with a sprite on screen after the change).
- Date changes ("Date: Wednesday, July 28.").
- Movies: prologue01/02 timed subtitles and credits (verified by OCR).
- Title menu, Load/Quick load submenu, Extra submenu, Load and Save screens
  (slot number, chapter, title), Config (all three pages with values),
  Clear list statistics, Tips list (rows), Help pages, confirmation dialogs
  (text + OK/Cancel selection), tutorial notice boxes, new TIPS.
- Phone: phone up/down, menu option text, Inbox/Outbox, address book entries
  with position, contact actions (Call), calling, mail view (garbled mails
  summarised), dial pad keys and pressed keys.
- In-scene documents (@channel board, PC screens) read line by line as they
  appear (verified: John Titor thread).
- Backlog entries with speaker and position; System menu items.
- Mod keys: R repeat, D describe (scene + date + phone; in menus the screen
  text), Ctrl+arrows review buffers, H mod menu with help and settings.

2026-11-14: launch with no speech fixed. The installed SGALaunch.exe was an
old build without the junction, so the mod never loaded (see NOTEBOOK.md).
Current launcher installed; game started via the junction, SpeechHost backend
NVDA, "Steins;Gate Access loaded" in spoken.log.

2026-11-16: first public release v0.1.0 on github.com/gomods1/steingate, with
the online installer (SteinsGateAccessibilityInstaller.exe). Release process
in NOTEBOOK.md, Decisions.

## TOP PRIORITY: mail reply words are untested

- STEINS;GATE has no choice menus. Story choices are made on the phone:
  some mails show highlighted words, and choosing one sends a reply.
  Replies, and whether calls are answered, decide which ending is reached
  (game design as described in the session-end note; HYPOTHESIS for this
  project until checked against the scripts or in game).
- The mod already reads the highlighted word and says "This mail has N reply
  words. Up and Down choose one, Enter replies with it." then "Reply word: X,
  n of N" (src/mod/phone.cpp), but play-testing never reached such a mail.
- Everything else on the phone was verified in game: reading mail, the
  address book, calls, the dial pad, sending mail.
- Until this is tested, a blind player may be unable to choose which ending
  they reach. First job next session: continue from save slot 7 with
  tools/playtest/skiploop.ps1 until a mail with reply words appears, then
  verify the hint, the word selection and sending the reply.

Not yet verified / not done:
- Interactive mail replies (link words): code in place (selectedLink from
  sghdDrawInteractiveMail, "This mail has N reply words" hint, "Reply word:
  X, n of N"), not yet reached in game. The first reply-able mail was not
  reached by 7/29 evening. Board posts use the same link markup (poster
  names, >>anchors): only phone-area text (x >= 880) is a reply mail.
- Incoming calls: not yet met (new-mail notices "Receiving mail" and
  "You've got mail" are read).
- System menu: unavailable (greyed) items are not marked.
- Tips list scroll offset with long lists; tip text panel reading.
- Music / CG library / Movies extras (locked until the game is cleared).
- Movie tracks for op2, endings and imv* insert movies (OP and prologue done).
  ffmpeg cannot decode Bink 2: capture in game (capture.py + ocr.ps1).
- Languages other than English (the build appears English-only: HYPOTHESIS).

Known issues seen at the end of session 1 (15:56, a Phone Trigger wait at
7/29 evening, "Phone, phone..."):
- "Text window hidden" was spoken twice in a row.
- The D-key style summary "Date: Thursday, July 29. Nothing is shown on
  screen." was spoken twice without the D key being pressed (probably the
  after-skip scene summary firing twice when skip toggled at a wait point).
  Logs kept in re/last_spoken.log and re/last_sga.log.
- F6 (save) does nothing at that wait point (the game blocks menus there).

## Next steps

1. Load slot 7 (board scene) and continue (skiploop.ps1 now ignores board
   links) to the first mail with reply words; verify the hint, link
   selection speech and sending a reply.
2. Investigate the two known issues above.
3. Incoming calls: verify the phone reader on a ringing call (answer/decline).
4. Greyed System menu items; Tips list scrolling and tip text.
5. Capture and transcribe op2, ending and insert movies when reached.
6. Blindfold pass of chapter 1 using only speech (rpg.md 23.5).
7. Release: zip dist\ (dinput8.dll + SteinsGateAccess\) with README.md.