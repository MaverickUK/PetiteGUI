# Changelog

All notable changes, in the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. Versions follow [SemVer](https://semver.org/).

## [0.3.0] - 2026-11-26

Real folders, a text editor and right-click menus.

### Added

- **A text editor:** TXT, INI and CFG files open in it on a double-click (or Enter, or File › Open), and any file with **Open as text** (File menu, right-click, Ctrl+E). Arrows, Home/End, Page Up/Down, Ctrl+Home/End, Backspace, Delete, Enter, Tab (spaces to the next column of 4), a click to place the caret; **Save** (Ctrl+S) keeps the file's CR LF line ends, writing to `PETITE.$$$` first so a full disk can't lose the file. Closing or quitting with changes asks to save. Files up to 60,000 bytes, held as a gap buffer (`src/textbuf.c`, host tested) in a DOS memory block that's handed back while a game runs and read back after.
- **Right-click menus:** on a file (Open, Open as text, Info, Delete) and on a game (Run, Run setup, Game info).
- `/SHOT` scenes 5 (the editor), 6 (the right-click menu) and 7 (an edit saved, checked by `tools/dostest.sh`).

### Changed

- **The A: and C: windows show real directories**, not sample files. Double-click a folder (or Enter, or File › Open) to go into it, and `..` (or Backspace) to go back up; the window's title is the path. A: starts closed, so a floppy drive isn't read until its window opens; an empty drive says it isn't ready rather than DOS asking "Abort, Retry, Fail?" over the desktop.
- **Delete really deletes the file**, after asking (folders can't be deleted yet).
- `tools/run.sh` sets up a drive with the sample games, at a 286's speed for Ink and an XT's for Paper, with EMS, and leaves DOSBox-X at the DOS prompt after Ctrl+Q.

## [0.2.0] - 2026-11-26

GameMinder's games, run with PetiteGUI swapped out of memory.

### Added

- **A Games window** (a joystick icon on the desktop, a Games menu, Ctrl+G) listing the games under `ScanPath` that have GameMinder's `!GM_INFO.TXT`, by name, with their years. The first time it opens it scans and saves `PETITE.IDX` beside `PETITE.EXE`; **Games › Rescan** scans again. The scan shows its progress.
- **Running games** with a double-click, Enter or Games › Run, and their setup programs with Games › Run setup, by GameMinder's rules: `exec:` with its arguments, batch files through `COMMAND.COM`, `before:` and `after:` in one batch file with the game, and a warning when `memory:` won't fit.
- **Memory swapping** while a game runs: everything but a ~1.8 KB stub goes to EMS, or to `SWAP.$$$` without EMS (`src/swapcore.asm`, TurboPower's ExecWithSwap by way of GameMinder's NASM port). The games list's memory is handed back to DOS first, and read back from the index afterwards.
- **Play stats** in each game's `!GM_LOG.TXT`, in GameMinder's format.
- `PETITE.CFG` (`scanpath`, `useems`, `playstats`), and `/SCAN`, `/LIST`, `/RUN=name` and `/MEM` on the command line.
- `tools/dostest.sh`: end-to-end tests in DOSBox-X, with fixture games and a stand-in game that records the memory it was given. Host tests for the metadata and launch rules (`src/gminfo.c`).

### Changed

- Menus and alerts save the pixels under them in DOS memory blocks rather than Open Watcom's far heap, which keeps freed blocks: they'd have stayed allocated under a game.
- Long names in lists are cut short before the right-hand column.

## [0.1.0] - 2026-11-26

The basics, in two looks.

### Added

- **Paper:** CGA mode 06h, 640×200 in black and white, with dithered greys and striped title bars (`drv_cga.c`).
- **Ink:** EGA mode 0Eh, 640×200 in 16 colours, through the EGA's set/reset and bit mask (`drv_ega.c`). The default on an EGA or VGA; **Options** switches between the two.
- A menu bar with drop-down menus (click, or press and drag), check marks, disabled items and keyboard shortcuts.
- Two windows that move, come to the front, close and scroll, each with a list of sample files to select from.
- Desktop icons (A:, C:, Trash) that open their windows on a double-click.
- Alerts with a default button (About, Info, Delete).
- The mouse through INT 33h, with a squat arrow for 200-line modes.
- Menus and alerts save the pixels under them and put them back; everything else redraws only what changed.
- `/SHOT` and `/DUMP` for screenshots, `tools/shot.sh` and `tools/dump2png.py` to take them through DOSBox-X, `tools/run.sh` to try it.
- Host unit tests, and CI that builds with Open Watcom v2.
- Releases: a `vX.Y.Z` tag that matches `src/version.h` publishes a GitHub release with a zip, its checksum and this changelog's section as the notes.
- `tools/check-changelog.sh`, run by CI on every push and pull request: the version is SemVer, this changelog has a dated section for it, and a change to what's built bumps the version and adds to the changelog.
- Badges in the README: build, release, latest version, SemVer, changelog, licence.

[0.3.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.3.0
[0.2.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.2.0
[0.1.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.1.0
