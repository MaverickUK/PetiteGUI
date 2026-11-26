# Changelog

All notable changes, in the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. Versions follow [SemVer](https://semver.org/).

## [0.11.0] - 2026-11-26

Smoother, faster drawing: the roadmap's item 14. At PC/XT speed (`tools/bench.sh`, DOSBox-X at 300 cycles), before and after:

| Each | Paper before | after | Ink before | after |
|---|--:|--:|--:|--:|
| The whole screen | 1400 ms | 765 ms (1.8x) | 2140 ms | 1260 ms (1.7x) |
| A folder window | 1125 ms | 602 ms (1.9x) | 1495 ms | 617 ms (2.4x) |
| The File menu, open and shut | 1372 ms | 920 ms (1.5x) | 1687 ms | 837 ms (2.0x) |
| A line of 62 characters | 184 ms | 41 ms (4.5x) | 228 ms | 43 ms (5.3x) |
| A folder scrolled a line | 1620 ms | 215 ms (7.5x) | 2091 ms | 365 ms (5.7x) |
| The next row selected | 850 ms | 215 ms (4.0x) | 993 ms | 242 ms (4.1x) |
| The editor scrolled a line | 5920 ms | 288 ms (20.6x) | 7426 ms | 668 ms (11.1x) |
| A character typed | 384 ms | 155 ms (2.5x) | 439 ms | 155 ms (2.8x) |
| The caret down a line | 896 ms | 279 ms (3.2x) | 988 ms | 265 ms (3.7x) |

### Changed

- **Scrolling moves what's already on the screen**: a list or the editor scrolling by less than a window copies the rows up or down in video memory (on the EGA, all four planes at once through the latches) and draws only the rows that come into view.
- **A window coming to the front** redraws only what covered it and the two title bars and scroll bars that change; opening one draws it once, and closing one draws what it covered, rather than the window in front again.
- **Typing in the editor** redraws from the caret to the end of the line; moving the caret, only the two characters it left and reached.
- **A redraw works out only the parts of a window it touches**: rows outside it aren't formatted, and the title, info line and scroll bar aren't, unless it touches them. Every pixel of a window is filled once, not under the title bar and scroll bar too.
- **The editor walks its view's lines once** to draw them, rather than counting each row's line from the top: scrolling it is about 20 times faster on an XT in Paper, 11 in Ink.
- **Folder windows find their widest name and size once**, when the folder is read, rather than formatting every size at every redraw.
- **Text** goes to the driver a run of characters at a time, the card set up once per run, and its inner loop is in assembly (`src/drawasm.asm`); the EGA driver writes each register with one 16-bit `OUT`.
- **In Paper, the pointer is hidden only over what's being drawn** (INT 33h function 10h), so it doesn't blink while things change elsewhere. In Ink it's still hidden everywhere: the mouse driver would draw it in the middle of the EGA's register settings.

### Added

- `PETITE /BENCH` and `tools/bench.sh`: the drawing timed at PC/XT speed, in both looks.
- `/SHOT=21` and `/SHOT=22` draw windows, a folder list and the editor a piece at a time, and `tools/dostest.sh` checks the screen comes out the same as drawn whole; `/SHOT=23` checks where the pointer is hidden. Host tests for `rect_subtract`.

## [0.10.0] - 2026-11-26

The details: the fourth step of the [roadmap](ROADMAP.md).

### Added

- **Dates:** each file's and folder's date, in a column at the right when the window is wide enough, and **View › By date** (newest first) beside By name and By size.
- **Selecting several files:** Shift-click selects from the last click, Ctrl-click adds or takes away one, Shift with the arrows extends, and Edit › Select all (Ctrl+A) takes everything but `..`. Copy, Move, Delete and dragging act on all of them, asking about any clash before starting and showing one progress box for the lot.
- **File › Info…** for a file or folder: its size, date and time, and attributes, with **read-only** and **hidden** as check boxes (a click, or R and H) that OK sets. System and archive are shown. Hidden files show dimmed in the list.
- **System › Quit** (Ctrl+Q), as well as File › Quit.
- `/SHOT=17` (several selected), `/SHOT=18` (the Info box), `/SHOT=20` (the System menu) and `/SHOT=19` (sorting by date, selecting, copying, Info and deleting, checked by `tools/dostest.sh`); host tests for the date and time formats.

### Changed

- Rename, Info, Open as text and Run… are for one item: with several selected they're greyed out.

## [0.9.0] - 2026-11-26

Editing common files: the third step of the [roadmap](ROADMAP.md).

### Added

- **Selecting text** in the editor: Shift with the arrows, Home, End, Page Up and Page Down, Ctrl+Home and Ctrl+End; dragging with the mouse (past the top or bottom, it scrolls); Shift-click to extend; Edit › Select all (Ctrl+A). Typing, Delete or Backspace replaces what's selected.
- **Cut, copy and paste** (Ctrl+X, Ctrl+C, Ctrl+V), in a new **Edit** menu and on a right-click in the editor. The clipboard is a DOS memory block of its own, kept when the file is closed or another opened (it's let go when a program runs).
- **Find** (Ctrl+F) and **Find next** (F3): ignoring case, from the caret, going round to the top.
- **File › New text file** (Ctrl+N): an Untitled file, whose first save asks where it goes; **File › Save as…** saves anywhere, asking before replacing a file. A bare name goes in the front folder.
- **AUTOEXEC.BAT and CONFIG.SYS** open in the editor on a double-click (Run… still runs AUTOEXEC.BAT).
- `/SHOT=15` (a selection and the Edit menu) and `/SHOT=16` (new file, clipboard, Find and Save as, checked by `tools/dostest.sh`); host tests for the text buffer's ranges and finding.
- The tools (`make`, `make test`, `make shots`, `make dostest`, `tools/run.sh`) work on Windows too, in Git Bash with `mingw32-make`, Open Watcom's `binnt64` and DOSBox-X.

### Changed

- Keys are read through the BIOS (`INT 16h`), not DOS, which would end the program on a Ctrl+C.
- The editor redraws only the lines a selection covered or covers, as well as the caret's.

## [0.8.1] - 2026-11-26

### Changed

- **Less flicker.** Moving through a menu redraws only the two items the highlight leaves and reaches, not the whole menu; selecting a row redraws only the two rows that change, not the window; and any redraw that falls inside one window starts from that window, rather than drawing the desktop and the windows behind it first, only to cover them. All of it is less drawing, so it's quicker too.

### Removed

- **The Trash.** It did nothing (deleting doesn't keep anything to get back); the drive icons now go five to a column.

## [0.8.0] - 2026-11-26

Looking after files: the second step of the [roadmap](ROADMAP.md).

### Added

- **Rename** (F2), **New folder** (F7), **Copy** (F5) and **Move** (F6), in the File menu and the right-click menu: the keys Norton Commander used. Names are checked against DOS's 8.3 rules first.
- **Copy and Move** ask where, suggesting the folder in the window behind: a folder there means into it, a full path or a bare name means that's its new name. Folders copy and move with everything in them.
- **Drag and drop:** drag a file or folder onto another folder window, onto a folder in one (`..` for the one above), or onto a drive's icon. Within a drive it moves; to another drive it copies; Ctrl makes it copy either way.
- **Deleting folders**, after saying how much is in them ("Delete GAMES and its 214 files?").
- A **progress box** for copying, moving and deleting, with how many files of how many; Esc stops it. Before replacing something, it asks.
- A move within a drive is a DOS rename, so it's instant (a folder can only be renamed in place, so moving one elsewhere copies it); across drives it's a copy, then a delete. Copies keep their files' dates and attributes; a copy that fails part way leaves no half file; nothing can be copied or moved into itself.
- If the file open in the editor is renamed or moved, the editor follows it; if it's deleted (with no changes), the editor closes.
- `src/fileops.c`; host tests for the path checks (`path_join`, `path_inside`, `path_valid_name`); `/SHOT=13` runs a sequence of copies, moves, renames and deletes that `tools/dostest.sh` checks on the disk, and `/SHOT=14` asks to delete a folder.

## [0.7.0] - 2026-11-26

Running anything, and every drive. The first step of the [roadmap](ROADMAP.md).

### Added

- **Run any program from a folder window:** double-click an EXE, COM or BAT file (or Enter, or File › Open) and it runs with PetiteGUI swapped out of memory, as games do; a batch file runs through `COMMAND.COM`.
- **File › Run…** (Ctrl+R, and on the right-click menu for programs) asks for a command line, filled in with the selected program, and runs it in the front folder through `COMMAND.COM`, so the PATH and DOS's own commands work. Its output stays on the screen until a key.
- **File › DOS prompt** (Ctrl+D): `COMMAND.COM` with PetiteGUI swapped out, in the front folder; `EXIT` comes back.
- **A text-entry box** (the Run… box first): a caret, Backspace and Delete, the arrows, Home and End, a click to place the caret, Enter for OK and Esc to cancel. Its editing is `src/field.c`, host tested.
- **An icon for every drive DOS has**, found without reading any of them: floppy drives (B: only when there are two), hard disks, CD-ROM drives (from MSCDEX) and network drives, each with an icon of its own, in columns from the right. Up to four folder windows can be open; opening a fifth drive reuses the one furthest back.
- `/DRIVES` lists the drives found; `/SHOT=10` to `12` (a command run, a batch file opened, the Run… box). `tools/dostest.sh` mounts a second drive and a CD-ROM, and checks Run… and a double-clicked batch file.

### Changed

- Nothing runs while the editor has unsaved changes: the editor's text is read back from the disk afterwards, so they'd be lost.
- Coming back from any program, the folder windows are read again (it may have changed them).

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

[0.11.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.11.0
[0.10.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.10.0
[0.9.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.9.0
[0.8.1]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.8.1
[0.8.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.8.0
[0.7.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.7.0
[0.6.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.6.0
[0.5.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.5.0
[0.4.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.4.0
[0.3.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.3.0
[0.2.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.2.0
[0.1.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.1.0
