#!/usr/bin/env bash
# Time the drawing (PETITE /BENCH) in DOSBox-X with no window, at about an IBM PC/XT's speed
# (a 4.77 MHz 8088: cycles fixed 300) for both looks, and print what each thing took.
#
#   tools/bench.sh                 Paper on a CGA and Ink on an EGA, both at XT speed
#   CYCLES=3000 tools/bench.sh     a 286's speed instead
#
# The times are DOSBox's idea of an XT, so compare them with each other (before and after a
# change), not with a stopwatch on real hardware.
set -euo pipefail
cd "$(dirname "$0")/.."
# DOSBox-X and Python on Windows want Windows paths; elsewhere a path is a path.
hostpath() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }

DOSBOX=${DOSBOX:-dosbox-x}
[ -f build/PETITE.EXE ] || { echo "Build first: make" >&2; exit 1; }

bench() { # look
    local look=$1 machine flag work pid i k
    case $look in
        paper) machine=cga; flag=/CGA ;;
        ink) machine=ega; flag=/EGA ;;
    esac
    work=$(mktemp -d)
    cp build/PETITE.EXE "$work/"
    cp -R tests/dos/GAMES "$work/GAMES"
    mkdir -p "$work/MANY"
    for i in $(seq 100 199); do echo "$i" > "$work/MANY/FILE$i.TXT"; done
    for i in $(seq 1 300); do
        printf 'Line %3d: the quick brown fox jumps over the lazy dog, again and again.\r\n' "$i"
    done > "$work/BENCH.TXT"
    printf 'scanpath:C:\\GAMES\n' > "$work/PETITE.CFG"
    cat > "$work/bench.conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=$machine
quit warning=false
[cpu]
cputype=8086
core=normal
cycles=fixed ${CYCLES:-300}
[autoexec]
mount c "$(hostpath "$work")"
c:
PETITE $flag /BENCH /DUMP=BENCH.OUT
exit
EOF
    "$DOSBOX" -silent -nogui -nomenu -conf "$(hostpath "$work/bench.conf")" > "$work/log.txt" 2>&1 &
    pid=$!
    for k in $(seq 1 2400); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.5
    done
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    if [ -f "$work/BENCH.OUT" ]; then
        tr -d '\r' < "$work/BENCH.OUT"
    else
        echo "no results for $look; DOSBox-X log in $work/log.txt" >&2
        return 1
    fi
    rm -rf "$work"
}

bench paper
echo
bench ink
