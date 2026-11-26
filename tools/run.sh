#!/usr/bin/env bash
# Run PetiteGUI in DOSBox-X, to try it by hand.
#
#   tools/run.sh          Paper on an emulated CGA
#   tools/run.sh ink      Ink on an emulated EGA (Options switches between the two)
#
# C: holds PETITE.EXE, a PETITE.CFG pointing at C:\GAMES, and the sample games from
# tests/dos/GAMES ("Memory Test" really runs: tests/dos/freemem.c, which writes how much memory
# it was given to C:\OUT\RUN.TXT). The screensaver starts after a minute. Speed is near an IBM PC/XT's for Paper (4.77 MHz 8088)
# and a 286's for Ink; CYCLES=max runs it flat out.
set -euo pipefail
cd "$(dirname "$0")/.."

look=${1:-paper}
case $look in
    paper) machine=cga; cycles="fixed 300"; cpu=8086 ;;
    ink) machine=ega; cycles="fixed 3000"; cpu=286 ;;
    *) echo "usage: tools/run.sh [paper|ink]" >&2; exit 1 ;;
esac
[ -f build/PETITE.EXE ] || make
WATCOM=${WATCOM:-$HOME/opt/openwatcom}
case "$(uname -s)/$(uname -m)" in
    Darwin/arm64) OWBIN=$WATCOM/armo64 ;;
    Darwin/*) OWBIN=$WATCOM/bino64 ;;
    *) OWBIN=$WATCOM/binl64 ;;
esac
work=$(mktemp -d)
cp build/PETITE.EXE "$work/"
cp -R tests/dos/GAMES "$work/GAMES"
mkdir -p "$work/OUT"
(cd "$work" && WATCOM=$WATCOM INCLUDE=$WATCOM/h PATH=$OWBIN:$PATH \
    "$OWBIN/wcl" -q -bt=dos -ms -0 -fe=GAMES/MEMTEST/FREEMEM.EXE "$OLDPWD/tests/dos/freemem.c" >/dev/null \
    && rm -f ./*.o ./*.obj)
printf 'scanpath:C:\\GAMES\nsaverdelay:1\n' > "$work/PETITE.CFG"
echo "C: is $work"
cat > "$work/run.conf" <<EOF
[dosbox]
machine=$machine
quit warning=false
[cpu]
cputype=${CPUTYPE:-$cpu}
core=normal
cycles=${CYCLES:-$cycles}
[render]
aspect=true
[dos]
ems=true
[autoexec]
mount c "$work"
c:
PETITE
EOF
exec "${DOSBOX:-dosbox-x}" -conf "$work/run.conf"
