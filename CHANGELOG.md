# Changelog

All notable changes, in the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. Versions follow [SemVer](https://semver.org/).

## [0.6.0] - 2026-11-26

Resizing windows.

### Added

- **A size box** in each window's bottom-right corner, under its scroll bar: drag it and an outline follows, and the window takes that size when the button is let go. The top-left corner stays put; windows can't be made smaller than 176 x 72 or run off the screen, and their width is a whole number of bytes. Lists stay scrolled within range, the editor shows more or fewer lines and columns, and the Games window fits more or fewer columns.
- **Zoom:** a double-click on a title bar makes the window fill the screen, and another puts it back as it was.
- `/SHOT=8` (a window resized and another zoomed); a host test for the resize limits (`rect_resize`).

### Changed

- System › About shows where PetiteGUI lives: github.com/MaverickUK/PetiteGUI (`/SHOT=9`).

## [0.5.0] - 2026-11-26

Columns in the Games window.

### Added

- **Choose the Games window's columns** in the Games menu: Year, Genre, Developer, Type, Video, Sound, Memory needed, Times played, Last played and Time played, besides the name. They're kept in `PETITE.CFG` (`gamecolumns:year,genre,plays`); Year alone to start with. The window widens to fit them, up to the screen; any that still don't fit beside a 12-character name are left off from the right.
- **Headings**, and a click on one sorts by it: names, text and years A to Z (blanks last), times played, time played, memory and the last played most first. The column it's sorted by is highlighted, and the selected game stays selected.
- **Play stats in the index:** scanning reads each game's `!GM_LOG.TXT` too, and after a game runs from PetiteGUI its count, last date and time are read again, so the columns are up to date.

### Changed

- The games index holds every GameMinder detail now (index version 2: an older `PETITE.IDX` is rebuilt when the Games window next opens). In memory, the games' text is one pool of strings the records point into, so 500 games still fit.
- The Game showcase saver reads it all from the index, opening no files.

## [0.4.0] - 2026-11-26

Screensavers.

### Added

- **Nine screensavers**, in a new **Saver** menu with how long to wait (1, 5 or 15 minutes; 5 by default) and **Start now**. A key, a click or the slightest mouse movement wakes them, and that input doesn't reach the desktop.
  - **Colour cycle:** a zone plate drawn once, then animated only by turning the palette (Ink's 15 colours, or Paper's ink through all 16), so it costs nothing per frame.
  - **Blank:** the picture switched off at the video card.
  - **Starfield**, **Lines** (two shapes on Lissajous curves, XORed, with trails), **Maze** (built, then solved, then a new one).
  - **Melt** (the desktop slides away a byte-wide column at a time) and **Life** (Conway's, seeded from what's on the desktop).
  - **Game showcase:** a game from your GameMinder collection every seven seconds, with what it is and how much you've played it.
  - **Clock:** seven-segment digits and the date, moved each minute.
  - **Random** picks one (not Blank) each time; **Off** turns them off.
- `saver` and `saverdelay` in `PETITE.CFG`, written back when they're chosen in the menu, keeping the file's other lines; `/SET=key:value` does the same from the command line.
- `/SAVER=name /STEPS=n` runs a saver over the desktop and dumps the screen; `tools/shot.sh savers` takes them all; `tools/dostest.sh` runs each in both looks, and checks `/SET`.
- Host tests for the savers' arithmetic (`src/saverlib.c`): the sine table, Life, the maze (every cell reached, one way between any two, solved), random numbers.

### Changed

- **The medium memory model:** code may now use more than one 64 KB segment (it had reached 62 KB in one). The swap core's entry points are far calls; it's still the first code in memory, and a swap still leaves about 1.8 KB.
- Video drivers gained the savers' operations: writing a row of pixels, moving byte columns down, XOR spans, reading a pixel, blanking and the palette.

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

[0.6.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.6.0
[0.5.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.5.0
[0.4.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.4.0
[0.3.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.3.0
[0.2.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.2.0
[0.1.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.1.0
