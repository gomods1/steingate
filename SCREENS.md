# Screens and play modes

Status: not started / announced / navigable / verified (verified = exercised
in the running game with the mod's speech checked in spoken.log).

## Screens

- Steam launcher (Launcher.exe, MFC owner-drawn buttons): replaced by
  SGALaunch.exe. verified (game starts with the mod).
- Logos (MAGES., 5pb., Nitroplus, Bink): pictures described. verified.
- Press Enter title: title.bk2 loop, idle plays op.bk2 (described). verified.
- Title menu (TitleMenuMain 10 34 01; cursor RVA 0x1244464): verified.
  - Load submenu (Quick load, Load): verified.
  - Extra submenu (Clear list, Tips; more items after clearing, flag 860):
    verified for the uncleared state; cleared state announced as "item n".
- Load / Save / Quick load (SaveMenuMain): slot number, chapter, title.
  verified. Lock state (Insert, DATALOCK) not read.
- Confirmation dialogs (Load this Data? OK/Cancel): verified.
- Config (OptionMain), 3 pages with values: verified. Default button (Delete)
  works in game; not announced.
- Help (HelpMain), 2 pages as text: verified.
- Clear list (ClistInit): statistics read on open. verified.
- Tips list (EncycMenuMain): rows read; tip text read when shown. navigable
  (scroll offset with many tips not yet tested).
- System menu, key 1 (SystemMenuMain; ScrWork 3338): verified. Unavailable
  items are not marked.
- Backlog, F2 / Insert (MesRevMain): entries with speaker and position.
  verified.
- Achievements, Music, CG library, Movies (after clearing): announced only.
- Exit game dialog (Escape): dialog text and option via notice reader.
  announced (option selection verified on the Load dialog).
- Saving message ("Saving data. Please do not turn off the system."): read.

## Play modes

- Dialogue: every line with speaker; R repeats; Ctrl+arrows history. verified.
- Skip (E) and force skip (Ctrl held): quiet, then last line and scene.
  verified.
- Auto mode (F3): lines read as they come (same path as dialogue). announced.
- Text window hidden (Backspace/Shift): announced. verified (state RVA).
- Scene: backgrounds, CGs, insert images on visibility; characters appear and
  leave; date changes. verified.
- Movies: prologue01/02 subtitles and credits, opening described. verified.
  Endings, op2, insert movies (imv*): not yet transcribed.
- Phone Trigger: phone up/down, menu, mailbox, mail list, mail view, address
  book, contact actions (Call, Outgoing), calling, sending mail, dial pad and
  key presses, blinking notices (Receiving mail, You've got mail). verified.
  Interactive reply words: implemented, not yet reached.
- Tutorial boxes and new TIPS: verified.
