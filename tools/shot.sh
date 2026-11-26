#!/usr/bin/env bash
# Screenshots of every scene in both looks, through DOSBox-X with no window:
# Paper on an emulated CGA, Ink on an emulated EGA. Writes build/shots/{paper,ink}{0,1,2,3,5,6,8,12,14}.png.
# Scene 3 opens the games list, scanning the fixture games in tests/dos/GAMES.
#
#   tools/shot.sh            all eighteen
#   tools/shot.sh paper 1    one
#   tools/shot.sh savers     each screensaver in both looks, a few seconds in:
#                            build/shots/saver-NAME-{paper,ink}.png (Colour cycle's palette turns
#                            don't show: a dump is video memory, not the colours on the screen)
set -euo pipefail
cd "$(dirname "$0")/.."

DOSBOX=${DOSBOX:-dosbox-x}
OUT=build/shots
mkdir -p "$OUT"
[ -f build/PETITE.EXE ] || { echo "Build first: make" >&2; exit 1; }

shoot() { # look scene [saver steps]
    local look=$1 scene=$2 machine flag name work
    case $look in
        paper) machine=cga; flag=/CGA ;;
        ink) machine=ega; flag=/EGA ;;
        *) echo "unknown look $look" >&2; exit 1 ;;
    esac
    name=$(echo "$look$scene" | tr '[:lower:]' '[:upper:]' | cut -c1-8)
    local args="/SHOT=$scene" out="$OUT/$look$scene.png"
    if [ -n "${3:-}" ]; then
        args="/SAVER=$3 /STEPS=$4"
        out="$OUT/saver-$3-$look.png"
        name=$(echo "S$3" | tr '[:lower:]' '[:upper:]' | cut -c1-8)
    fi
    work=$(mktemp -d)
    cp build/PETITE.EXE "$work/"
    cp -R tests/dos/GAMES "$work/GAMES"
    mkdir -p "$work/D" "$work/CD"
    echo "a CD" > "$work/CD/README.TXT"
    printf 'scanpath:C:\\GAMES\ngamecolumns:year,genre,developer,plays,last,time\n' > "$work/PETITE.CFG"
    cat > "$work/shot.conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=$machine
quit warning=false
[cpu]
cycles=max
[autoexec]
mount c "$work"
mount d "$work/D"
mount e "$work/CD" -t cdrom
c:
PETITE /LIST > NUL
PETITE $flag $args /DUMP=$name.BIN
exit
EOF
    "$DOSBOX" -silent -nogui -nomenu -conf "$work/shot.conf" > "$work/log.txt" 2>&1 &
    local pid=$! i
    for i in $(seq 1 60); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.5
    done
    kill "$pid" 2>/dev/null || true
    if [ ! -f "$work/$name.BIN" ]; then
        echo "no dump for $look $scene; DOSBox-X log in $work/log.txt" >&2
        exit 1
    fi
    python3 tools/dump2png.py "$work/$name.BIN" "$out"
    rm -rf "$work"
    echo "$out"
}

if [ "${1:-}" = savers ]; then
    for look in paper ink; do
        for spec in cycle:5 stars:150 lines:400 maze:320 melt:160 life:80 games:3 clock:3; do
            shoot $look - "${spec%%:*}" "${spec##*:}"
        done
    done
elif [ $# -eq 2 ]; then
    shoot "$1" "$2"
else
    for look in paper ink; do
        for scene in 0 1 2 3 5 6 8 12 14; do
            shoot $look $scene
        done
    done
fi
