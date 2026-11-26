#!/usr/bin/env bash
# End-to-end tests in DOSBox-X (no window), for what the host tests can't reach: scanning for
# GameMinder's !GM_INFO.TXT, the index, running games swapped out (to EMS, then to a file),
# batch files with before:/after:, and GameMinder's play log.
#
# A stand-in game (tests/dos/freemem.c) records how much memory it was given. Run directly
# from DOS first, then from PetiteGUI: with PetiteGUI swapped out it should get nearly as much.
set -euo pipefail
cd "$(dirname "$0")/.."
make -s build/PETITE.EXE

WATCOM=${WATCOM:-$HOME/opt/openwatcom}
case "$(uname -s)/$(uname -m)" in
    Darwin/arm64) OWBIN=$WATCOM/armo64 ;;
    Darwin/*) OWBIN=$WATCOM/bino64 ;;
    *) OWBIN=$WATCOM/binl64 ;;
esac

work=$(mktemp -d)
[ -n "${KEEP:-}" ] && echo "work: $work" || trap 'rm -rf "$work"' EXIT
mkdir -p "$work/PETITE" "$work/GUI" "$work/OUT" "$work/DDRIVE" "$work/CD"
echo "a CD" > "$work/CD/README.TXT"
cp build/PETITE.EXE "$work/PETITE/"
cp build/PETITE.EXE "$work/GUI/"
printf 'scanpath:C:\\GAMES\\MEMTEST\n' > "$work/GUI/PETITE.CFG"
printf 'one\r\ntwo\r\n' > "$work/OUT/EDIT.TXT"
mkdir -p "$work/SET"
cp build/PETITE.EXE "$work/SET/"
printf '; my settings\nscanpath:C:\\GAMES\nSaver: clock\n; the end\n' > "$work/SET/PETITE.CFG"
cp -R tests/dos/GAMES "$work/GAMES"
(cd "$work" && WATCOM=$WATCOM INCLUDE=$WATCOM/h PATH=$OWBIN:$PATH \
    "$OWBIN/wcl" -q -bt=dos -ms -0 -fe=GAMES/MEMTEST/FREEMEM.EXE "$OLDPWD/tests/dos/freemem.c" >/dev/null)
rm -f "$work"/*.o "$work"/*.obj
printf 'scanpath:C:\\GAMES\n' > "$work/PETITE/PETITE.CFG"

cat > "$work/test.conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=svga_s3
quit warning=false
[cpu]
cycles=max
[dos]
ems=true
[autoexec]
mount c "$work"
mount d "$work/DDRIVE"
mount e "$work/CD" -t cdrom
c:
cd \\PETITE
C:\\GAMES\\MEMTEST\\FREEMEM.EXE C:\\OUT\\BASE.TXT direct
PETITE /MEM > C:\\OUT\\MEM.TXT
PETITE /LIST > C:\\OUT\\LIST.TXT
PETITE /RUN=memory > C:\\OUT\\RUN1.TXT
PETITE /RUN=batch > C:\\OUT\\RUN2.TXT
echo scanpath:C:\\GAMES> PETITE.CFG
echo useems:false>> PETITE.CFG
PETITE /RUN=memory > C:\\OUT\\RUN3.TXT
PETITE /RUN=noname > C:\\OUT\\RUN4.TXT
cd \\GUI
PETITE /CGA /SHOT=4 /DUMP=C:\\OUT\\BACK.BIN > C:\\OUT\\GUI.TXT
PETITE /EGA /SHOT=10 /DUMP=C:\\OUT\\RUNCMD.BIN > C:\\OUT\\RUNCMDO.TXT
PETITE /EGA /SHOT=11 /DUMP=C:\\OUT\\OPENBAT.BIN > C:\\OUT\\OPENBATO.TXT
PETITE /DRIVES > C:\\OUT\\DRIVES.TXT
PETITE /EGA /SHOT=7 /DUMP=C:\\OUT\\EDIT.BIN > C:\\OUT\\EDITRUN.TXT
PETITE /CGA /SAVER=cycle /STEPS=60 /DUMP=C:\\OUT\\CCYCLE.BIN
PETITE /EGA /SAVER=cycle /STEPS=60 /DUMP=C:\\OUT\\ECYCLE.BIN
PETITE /CGA /SAVER=blank /STEPS=60 /DUMP=C:\\OUT\\CBLANK.BIN
PETITE /EGA /SAVER=blank /STEPS=60 /DUMP=C:\\OUT\\EBLANK.BIN
PETITE /CGA /SAVER=stars /STEPS=60 /DUMP=C:\\OUT\\CSTARS.BIN
PETITE /EGA /SAVER=stars /STEPS=60 /DUMP=C:\\OUT\\ESTARS.BIN
PETITE /CGA /SAVER=lines /STEPS=60 /DUMP=C:\\OUT\\CLINES.BIN
PETITE /EGA /SAVER=lines /STEPS=60 /DUMP=C:\\OUT\\ELINES.BIN
PETITE /CGA /SAVER=maze /STEPS=60 /DUMP=C:\\OUT\\CMAZE.BIN
PETITE /EGA /SAVER=maze /STEPS=60 /DUMP=C:\\OUT\\EMAZE.BIN
PETITE /CGA /SAVER=melt /STEPS=60 /DUMP=C:\\OUT\\CMELT.BIN
PETITE /EGA /SAVER=melt /STEPS=60 /DUMP=C:\\OUT\\EMELT.BIN
PETITE /CGA /SAVER=life /STEPS=60 /DUMP=C:\\OUT\\CLIFE.BIN
PETITE /EGA /SAVER=life /STEPS=60 /DUMP=C:\\OUT\\ELIFE.BIN
PETITE /CGA /SAVER=games /STEPS=60 /DUMP=C:\\OUT\\CGAMES.BIN
PETITE /EGA /SAVER=games /STEPS=60 /DUMP=C:\\OUT\\EGAMES.BIN
PETITE /CGA /SAVER=clock /STEPS=60 /DUMP=C:\\OUT\\CCLOCK.BIN
PETITE /EGA /SAVER=clock /STEPS=60 /DUMP=C:\\OUT\\ECLOCK.BIN
cd \\SET
PETITE /SET=saver:maze
PETITE /SET=saverdelay:15
dir /a C:\\PETITE > C:\\OUT\\DIR.TXT
exit
EOF
dosbox-x -silent -nogui -nomenu -conf "$work/test.conf" > "$work/log.txt" 2>&1 &
pid=$!
for _ in $(seq 1 120); do
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.5
done
kill "$pid" 2>/dev/null || true

python3 - "$work" <<'EOF'
import os, re, sys
w = sys.argv[1]
fails = []

def read(*p):
    path = os.path.join(w, *p)
    return open(path, encoding="latin-1").read() if os.path.exists(path) else ""

def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails.append(what)

base = re.findall(r"free=(\d+)", read("OUT", "BASE.TXT"))
run = re.findall(r"free=(\d+)(.*)", read("OUT", "RUN.TXT"))
batch = read("OUT", "BATCH.TXT")
listing = read("OUT", "LIST.TXT")
mem = read("OUT", "MEM.TXT")
log = read("GAMES", "MEMTEST", "!GM_LOG.TXT")
index = read("PETITE", "PETITE.IDX")
print(mem.strip())
print(listing.strip())

check(len(base) == 1, "the stand-in game ran directly: %s KB free" % (base[0] if base else "?"))
check("4 games in C:\\GAMES" in listing, "the scan found all 4 games, nested and nameless ones too")
check("NONAME" in listing and "Deep Down" in listing, "a game with no name: is listed by its directory")
check(index.count("\npath:") == 4 and "scanpath:C:\\GAMES" in index, "PETITE.IDX holds the 4 games")
check("plays:3" in index and "minutes:175" in index and "last:2026/11/24" in index,
      "PETITE.IDX has the play stats from a !GM_LOG.TXT")
check("developer:Nobody Soft" in index and "type:Shareware" in index and "video:EGA" in index and "sound:AdLib" in index,
      "PETITE.IDX has GameMinder's other details")
check(len(run) == 3, "the game ran three times from PetiteGUI (EMS, the swap file, the Games window)")
if base and len(run) >= 2:
    b = int(base[0])
    for (free, args), how in zip(run, ("EMS", "swap file", "EMS, from the Games window")):
        lost = b - int(free)
        check(lost <= 4, "swapped to %s: the game got %s KB, %d KB less than run directly" % (how, free, lost))
        check(args.strip() == "hello world", "its arguments arrived: %r" % args.strip())
check("before" in read("OUT", "BEFORE.TXT") and "after" in read("OUT", "AFTER.TXT"),
      "before: and after: ran around the batch game")
check("batch" in batch, "the batch game ran, with its argument")
check(log.count(">") == 3 and log.count("<") == 3, "!GM_LOG.TXT has three plays in GameMinder's format")
check(bool(re.match(r">\d{4}/\d\d/\d\d \d\d:\d\d\n<\d+\n", log)), "the log lines look like GameMinder's")
check("wasn't found" in read("OUT", "RUN4.TXT") or "No game" in read("OUT", "RUN4.TXT"),
      "a game whose program is missing is reported, not run")
check("SWAP" not in read("OUT", "DIR.TXT") and "PETITE.BAT" not in read("OUT", "DIR.TXT").upper(),
      "no swap file or launch batch file left behind")
check(os.path.getsize(os.path.join(w, "OUT", "BACK.BIN")) == 16384 if os.path.exists(os.path.join(w, "OUT", "BACK.BIN")) else False,
      "back on the desktop afterwards (the screen was dumped)")
if os.path.exists(os.path.join(w, "OUT", "BACK.BIN")):
    os.makedirs("build/shots", exist_ok=True)
    os.system("python3 tools/dump2png.py '%s' build/shots/back.png" % os.path.join(w, "OUT", "BACK.BIN"))
    print("     the desktop after the game: build/shots/back.png")
edited = open(os.path.join(w, "OUT", "EDIT.TXT"), "rb").read()
check(edited == b"Hi\r\none\r\ntwo\r\n!", "the editor saved a typed line and kept CR LF: %r" % edited)
check(not os.path.exists(os.path.join(w, "OUT", "PETITE.$$$")), "no temporary file left from saving")
for k in ["cycle", "blank", "stars", "lines", "maze", "melt", "life", "games", "clock"]:
    for look, size in (("C", 16384), ("E", 64000)):
        p = os.path.join(w, "OUT", look + k.upper()[:7] + ".BIN")
        data = open(p, "rb").read() if os.path.exists(p) else b""
        drawn = len(set(data)) > 1
        ok = len(data) == size and (not drawn if k == "blank" else drawn)
        check(ok, "the %s saver ran in %s and %s" % (k, "Paper" if look == "C" else "Ink",
                                                    "left the screen black" if k == "blank" else "drew"))
cfg = read("SET", "PETITE.CFG").replace("\r", "")
check(cfg == "; my settings\nscanpath:C:\\GAMES\nsaver:maze\n; the end\nsaverdelay:15\n",
      "/SET changed one setting and added another, keeping the rest: %r" % cfg)
runcmd = re.findall(r"free=(\d+) fromrun", read("OUT", "RUNCMD.TXT"))
check(len(runcmd) == 1, "File > Run... ran a command through COMMAND.COM, with its arguments")
if base and runcmd:
    lost = int(base[0]) - int(runcmd[0])
    check(lost <= 8, "and it got %s KB, %d KB less than run directly (COMMAND.COM stays under it)" % (runcmd[0], lost))
check(len(re.findall(r"free=", read("OUT", "BATCH.TXT"))) == 2, "a batch file double-clicked in a folder ran")
drives = read("OUT", "DRIVES.TXT")
check("C: hard disk" in drives and "D: hard disk" in drives and "E: CD-ROM" in drives and "B:" not in drives,
      "the drives found: %s" % drives.strip().replace("\r", "").replace("\n", ", "))
print("%d failed" % len(fails))
sys.exit(1 if fails else 0)
EOF
