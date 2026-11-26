#!/usr/bin/env bash
# Run PetiteGUI in DOSBox-X, to try it by hand.
#
#   tools/run.sh          Paper on an emulated CGA
#   tools/run.sh ink      Ink on an emulated EGA (Options switches between the two)
#
# Cycles are set near an IBM PC/XT's (4.77 MHz 8088), so drawing is as slow as on one;
# CYCLES=max runs it flat out.
set -euo pipefail
cd "$(dirname "$0")/.."

look=${1:-paper}
case $look in
    paper) machine=cga ;;
    ink) machine=ega ;;
    *) echo "usage: tools/run.sh [paper|ink]" >&2; exit 1 ;;
esac
[ -f build/PETITE.EXE ] || make
work=$(mktemp -d)
cp build/PETITE.EXE "$work/"
cat > "$work/run.conf" <<EOF
[dosbox]
machine=$machine
quit warning=false
[cpu]
cputype=8086
core=normal
cycles=${CYCLES:-fixed 300}
[render]
aspect=true
[autoexec]
mount c "$work"
c:
PETITE
exit
EOF
exec "${DOSBOX:-dosbox-x}" -conf "$work/run.conf"
