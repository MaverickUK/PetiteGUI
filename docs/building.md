# Building and testing

Back to the [README](../README.md).

## Tools

- **Open Watcom v2**, the C compiler and linker for 16-bit DOS. Download `ow-snapshot.tar.xz` from a [dated release](https://github.com/open-watcom/open-watcom-v2/releases) and unpack it to `~/opt/openwatcom` (or set `WATCOM`). It carries native binaries for Apple Silicon (`armo64`), Intel Macs (`bino64`) and Linux (`binl64`); the Makefile picks one.
- **DOSBox-X** (`brew install dosbox-x`) for running it, and for the screenshots.
- **A host C compiler** for the unit tests.

## Commands

```sh
make                # build/PETITE.EXE
make test           # host unit tests (rectangles, number formatting), with ASan/UBSan
make shots          # build/shots/{paper,ink}{0,1,2}.png, through DOSBox-X with no window
tools/run.sh        # try it: Paper on an emulated CGA at about PC/XT speed
tools/run.sh ink    # Ink on an emulated EGA
CYCLES=max tools/run.sh   # flat out
```

## Screenshots

`PETITE /SHOT=n /DUMP=FILE.BIN` draws a scene (0 the desktop, 1 the File menu open, 2 the Delete alert), writes video memory exactly as it's laid out, and quits. `tools/shot.sh` runs that in DOSBox-X for both looks, on an emulated CGA for Paper and EGA for Ink, and `tools/dump2png.py` turns each dump into a PNG at 4:3. The pictures in [img/](img) come from it: after a change to the drawing, run `make shots` and look.

## CI

`.github/workflows/build.yml` downloads Open Watcom on Ubuntu, builds `PETITE.EXE`, runs the host tests and keeps the executable as an artifact.

## Versioning

SemVer in `src/version.h` (`PETITE_VERSION`). Bump it with every change and add a `CHANGELOG.md` entry.
