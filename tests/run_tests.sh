#!/usr/bin/env bash
# Host unit tests: the code with no DOS in it, built with the host's C compiler, with
# AddressSanitizer and UndefinedBehaviorSanitizer where the compiler has them.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host
CC=${CC:-cc}
FLAGS="-std=c99 -Wall -Wextra -Werror -g"
if $CC -fsanitize=address,undefined -x c -o /dev/null - <<<'int main(void){return 0;}' 2>/dev/null; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer"
fi
# shellcheck disable=SC2086
$CC $FLAGS -o build/host/test_core tests/test_core.c src/rect.c src/util.c src/gminfo.c src/textbuf.c src/saverlib.c -lm
build/host/test_core
