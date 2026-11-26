# Changelog

All notable changes, in the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. Versions follow [SemVer](https://semver.org/).

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

[0.2.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.2.0
[0.1.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.1.0
