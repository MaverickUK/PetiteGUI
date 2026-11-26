#!/usr/bin/env bash
# Screenshots of every scene in both looks, through DOSBox-X with no window:
# Paper on an emulated CGA, Ink on an emulated EGA. Writes build/shots/{paper,ink}{0,1,2,3,5,6}.png.
# Scene 3 opens the games list, scanning the fixture games in tests/dos/GAMES.
#
#   tools/shot.sh            all twelve
#   tools/shot.sh paper 1    one
set -euo pipefail
cd "$(dirname "$0")/.."

DOSBOX=${DOSBOX:-dosbox-x}
OUT=build/shots
mkdir -p "$OUT"
[ -f build/PETITE.EXE ] || { echo "Build first: make" >&2; exit 1; }

shoot() { # look scene
    local look=$1 scene=$2 machine flag name work
    case $look in
        paper) machine=cga; flag=/CGA ;;
        ink) machine=ega; flag=/EGA ;;
        *) echo "unknown look $look" >&2; exit 1 ;;
    esac
    name=$(echo "$look$scene" | tr '[:lower:]' '[:upper:]')
    work=$(mktemp -d)
    cp build/PETITE.EXE "$work/"
    cp -R tests/dos/GAMES "$work/GAMES"
    printf 'scanpath:C:\\GAMES\n' > "$work/PETITE.CFG"
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
c:
PETITE $flag /SHOT=$scene /DUMP=$name.BIN
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
    python3 tools/dump2png.py "$work/$name.BIN" "$OUT/$look$scene.png"
    rm -rf "$work"
    echo "$OUT/$look$scene.png"
}

if [ $# -eq 2 ]; then
    shoot "$1" "$2"
else
    for look in paper ink; do
        for scene in 0 1 2 3 5 6; do
            shoot $look $scene
        done
    done
fi
