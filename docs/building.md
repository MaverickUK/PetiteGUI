# Building and testing

Back to the [README](../README.md).

## Tools

- **Open Watcom v2**, the C compiler and linker for 16-bit DOS. Download `ow-snapshot.tar.xz` from a [dated release](https://github.com/open-watcom/open-watcom-v2/releases) and unpack it to `~/opt/openwatcom` (or set `WATCOM`). It carries native binaries for Apple Silicon (`armo64`), Intel Macs (`bino64`) and Linux (`binl64`); the Makefile picks one.
- **DOSBox-X** (`brew install dosbox-x`) for running it, and for the screenshots.
- **NASM** (`brew install nasm`) for the swap core.
- **A host C compiler** for the unit tests.

**On Windows**, in Git Bash: unpack the same snapshot to `~/opt/openwatcom` (the Makefile uses its `binnt64`), and use `mingw32-make` for `make` (from [WinLibs](https://winlibs.com), which brings `gcc` and NASM too). The scripts give DOSBox-X and Python Windows paths, and use `python` where `python3` is only the Store's shortcut. If DOSBox-X isn't on the `PATH` as an `.exe` (Scoop's shim isn't one), name it: `DOSBOX=~/scoop/apps/dosbox-x/current/dosbox-x.exe mingw32-make shots`; `tools/dostest.sh` calls `make` itself, so give it `MAKE=mingw32-make`.

## Commands

```sh
make                # build/PETITE.EXE
make test           # host unit tests (rectangles, number formatting), with ASan/UBSan
make shots          # build/shots/{paper,ink}{0,1,2,3,5,6,8,12,14,15,17,18,20}.png, through DOSBox-X with no window
tools/shot.sh savers  # build/shots/saver-NAME-{paper,ink}.png
make dostest        # end-to-end tests in DOSBox-X: scanning, swapping, running games
tools/run.sh        # try it: Paper on an emulated CGA at about PC/XT speed
tools/run.sh ink    # Ink on an emulated EGA
CYCLES=max tools/run.sh   # flat out
tools/bench.sh      # time the drawing at PC/XT speed, both looks (PETITE /BENCH)
```

## End-to-end tests

`tools/dostest.sh` (`make dostest`) runs PetiteGUI in DOSBox-X with no window, on the fixture games in `tests/dos/GAMES` and a stand-in game (`tests/dos/freemem.c`) that records how much memory DOS gave it and its arguments. It checks:
- the scan finds every game, including nested and nameless ones, and writes `PETITE.IDX`;
- a game run from PetiteGUI, swapped to EMS and then to a file, gets within 4 KB of the memory it gets run straight from DOS, with its arguments;
- a batch game runs with its `before:` and `after:`;
- `!GM_LOG.TXT` gets GameMinder's lines;
- a missing program is reported, and no swap or batch file is left behind;
- after a game run from the Games window, the desktop is back;
- the editor saves a typed line into a CR LF file, keeping CR LF, and leaves no temporary file;
- a new file, typed in, copied and pasted, found in, cut from and pasted to, is saved as `NEW.TXT` with CR LF; `CONFIG.SYS` opens in the editor from a folder;
- scenes 21 and 22 (windows opened, raised and closed, a folder list and the editor scrolled, selected and typed in, all drawn a piece at a time) leave the screen exactly as drawing it whole does, in both looks; scene 23 shows the pointer left alone by drawing elsewhere in Paper (INT 33h function 10h), and hidden by any drawing in Ink;
- a folder sorted by date comes newest first; a Shift-click and Ctrl-clicks select three items, which copy to D: together; Info, with R and Enter put in the keyboard buffer, makes a file read-only, and a confirmed delete still removes it; Select all and Delete empty D:;
- every screensaver runs 60 steps in Paper and in Ink and returns, having drawn (Blank: having left the screen black);
- `/SET` changes one setting and adds another, keeping the rest of `PETITE.CFG`;
- the index holds the play stats from a `!GM_LOG.TXT` and GameMinder's other details;
- File › Run…'s path runs a command through `COMMAND.COM` with its arguments, and within 8 KB of the memory it gets run directly; a batch file double-clicked in a folder runs;
- the drives are found: C:, a second drive mounted as D: and a CD-ROM as E: (and DOSBox's own Z:), with no B:;
- a sequence of file operations: a folder copied to D: and moved back (whole, its dates kept, gone from D:), a file copied, moved into a new folder, the folder renamed and the file renamed, a folder refused a copy into itself, an overwrite refused, a tree copied then deleted.

`KEEP=1 tools/dostest.sh` keeps the DOS drive it made, to look at.

## Screenshots

`PETITE /SHOT=n /DUMP=FILE.BIN` draws a scene (0 the desktop, 1 the File menu open, 2 the Delete alert, 3 the games list, scanning the fixture games, 5 the editor, 6 the right-click menu, 8 a window resized and another zoomed, 9 the About box, 12 the Run… box, 14 deleting a folder, 15 a selection in the editor with the Edit menu open, 17 several files selected, with their dates, 18 the Info box, 20 the System menu; 4, 7, 10, 11, 13, 16, 19, 21, 22 and 23 are for `tools/dostest.sh`), writes video memory exactly as it's laid out, and quits. `tools/shot.sh` runs that in DOSBox-X for both looks, on an emulated CGA for Paper and EGA for Ink, and `tools/dump2png.py` turns each dump into a PNG at 4:3. The pictures in [img/](img) come from it: after a change to the drawing, run `make shots` and look.

## CI

`.github/workflows/build.yml` runs on every push to `main` and every pull request: it checks the version and changelog (below), downloads Open Watcom on Ubuntu, runs the host tests, builds `PETITE.EXE` and keeps it as an artifact. A second job runs the end-to-end tests in DOSBox-X.

## Versions and the changelog

The version is [SemVer](https://semver.org), in `src/version.h` (`PETITE_VERSION`), and every version has a section in [CHANGELOG.md](../CHANGELOG.md) ([Keep a Changelog](https://keepachangelog.com/en/1.1.0/)): `## [X.Y.Z] - YYYY-MM-DD`, what changed under Added, Changed, Fixed or Removed, and a link line at the bottom.

- **Any change to what's built** (`src/`, the `Makefile`) bumps the version and adds to the changelog, in the same push. Patch for fixes, minor for new features, major for breaking changes (while it's 0.x, minor for anything notable).
- **Other changes** (docs, tools, CI) go into the section of the version not yet released, if there is one.

`tools/check-changelog.sh` enforces this, and CI runs it against the commit before each push (or a pull request's base): it fails when `src/` changed without a new version, or the version changed without the changelog. Run it before pushing:

```sh
tools/check-changelog.sh origin/main
```

## Releasing

When a version is ready, on `main`:

```sh
git tag v0.1.0
git push origin v0.1.0
```

`.github/workflows/release.yml` builds and tests the tag, checks it matches `PETITE_VERSION` and is on `main`, then publishes a GitHub release: `petitegui-X.Y.Z.zip` (`PETITE.EXE`, `LICENSE.TXT`, `CHANGES.TXT`), `SHA256SUMS`, and the version's changelog section as the notes. A version with a hyphen (`0.2.0-beta.1`) is marked as a pre-release.
