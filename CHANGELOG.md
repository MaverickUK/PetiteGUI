# Changelog

All notable changes, in the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. Versions follow [SemVer](https://semver.org/).

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

[0.1.0]: https://github.com/MaverickUK/PetiteGUI/releases/tag/v0.1.0
