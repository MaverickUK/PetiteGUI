# CLAUDE.md: context for working on PetiteGUI

PetiteGUI is a small desktop GUI for CGA and EGA PCs, written in C for 16-bit DOS (any 8088), built with Open Watcom v2 and tried in DOSBox-X. It started from four style studies ([docs/style-studies.html](docs/style-studies.html)); Paper (CGA 640×200, 2 colours) came first, then Ink (EGA 640×200, 16 colours). How it works: [docs/design.md](docs/design.md). Building: [docs/building.md](docs/building.md).

The owner is an experienced software architect (C#/.NET, TypeScript, Python) who is new to DOS and 16-bit C. Explain DOS- and hardware-specific decisions briefly, and favour clear, idiomatic code over clever code.

## Scope

The basics first: desktop, menu bar and menus, windows, icons, alerts, the pointer. No applications yet.

## Rules

- **C89 for `wcc`:** declarations at the top of a block. `int` is 16 bits: use `long` past 32,767, and cast before multiplying sizes (`(long)a * b`).
- **Medium model** (since 0.4.0): code in as many 64 KB segments as it needs, all data in one (DGROUP, 64 KB with the stack). Calls are far, data pointers near. Video memory and DOS blocks are `__far`; never keep a near pointer to them. `swapcore.asm`'s entry points are far (`retf`).
- **Drivers own the hardware.** Only `drv_*.c` touch video memory or video ports; `draw.c` calls only `gfx_*`. A driver function gets coordinates already clipped to the screen, and an EGA one leaves the graphics controller at the BIOS defaults (the mouse driver relies on them).
- **Hide the mouse** (`mouse_hide`/`mouse_show`, which nest) around anything that draws, reads or saves video memory.
- **Window edges on 8-pixel columns** (`SNAP8`), so drawing is whole bytes.
- **Pixels are 2.4:1 tall** in 640×200: square-looking things are wider than tall in pixels.
- **Paper and Ink share everything but the driver and the theme.** A new widget takes its colours from `Theme`, never a literal.
- **The swap core is the first code in memory:** `build/swapcore.obj` stays first in the Makefile's `OBJ`, and the map (`build/PETITE.MAP`) shows `_swap_core` near `0000:0000`. Everything after its `FirstToSave` is swapped out while a game runs.
- **No C far heap** (`_fmalloc`): its freed blocks stay allocated, under a game. Use `_dos_allocmem`/`_dos_freemem`, and free every block before `swap_run`.
- **Nothing of PetiteGUI's may be in use while a game runs:** `run_game` frees the games list and the editor's text, and takes out the `INT 24h` handler first; `swap_run` refuses if any vector still points into PetiteGUI.
- **GameMinder compatibility:** `!GM_INFO.TXT`, `!GM_LOG.TXT` and the launch rules match GameMinder's (`~/Code/GameMinder`, QUICKSTART.md, LAUNCH.PAS, RUNNER.PAS). Change them there first.
- **Pure code** (`rect.c`, `util.c`, `gminfo.c`) has no DOS headers, so `tests/run_tests.sh` builds it on the host. Keep logic that can be pure there.

## Toolchain

- `make` (Open Watcom in `~/opt/openwatcom`, or `WATCOM=`); the Makefile calls the tools by full path because macOS's make 3.81 ignores an exported `PATH`.
- `make test`: host unit tests.
- `make shots`: every scene in both looks through DOSBox-X (`/SHOT=n /DUMP=`), as PNGs in `build/shots/`. After any drawing change, take them and look; copy good ones to `docs/img/` when they show something new.
- `make dostest`: end-to-end tests in DOSBox-X (scanning, swapping, running games, the play log). Run it after any change to games, launching or swapping.
- `tools/run.sh [paper|ink]`: interactive, at about PC/XT speed. Needs NASM too.

## Versioning

SemVer in `src/version.h` and a [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) section in `CHANGELOG.md` for every version. Any change to `src/` or the `Makefile` bumps the version and adds to the changelog in the same commit; other changes go into the unreleased version's section if there is one. `tools/check-changelog.sh origin/main` checks it (CI runs it on every push). A `vX.Y.Z` tag on `main` publishes the release ([docs/building.md](docs/building.md#releasing)).
