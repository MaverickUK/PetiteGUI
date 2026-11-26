#!/usr/bin/env bash
# Keeps the version and the changelog honest. CI runs it on every push and pull request.
#
#   tools/check-changelog.sh            the current version is SemVer and CHANGELOG.md has
#                                       a dated, non-empty section and a link for it
#   tools/check-changelog.sh BASE       also: if anything built into PETITE.EXE (src/, the
#                                       Makefile) changed since commit BASE, the version was
#                                       bumped and CHANGELOG.md changed with it
#   tools/check-changelog.sh --tag vX.Y.Z   also: the tag matches the version
#
# Prints the version on success.
set -euo pipefail
cd "$(dirname "$0")/.."

fail() { echo "::error::$*" >&2; echo "check-changelog: $*" >&2; exit 1; }

version_of() { sed -n 's/.*PETITE_VERSION "\(.*\)".*/\1/p'; }
version=$(version_of < src/version.h)
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]] || fail "PETITE_VERSION is not SemVer: '$version'"

# "## [X.Y.Z] - YYYY-MM-DD", followed by some text before the next section
grep -Eq "^## \[$(printf '%s' "$version" | sed 's/[.]/\\./g')\] - [0-9]{4}-[0-9]{2}-[0-9]{2}$" CHANGELOG.md \
    || fail "CHANGELOG.md has no '## [$version] - YYYY-MM-DD' section"
notes=$(awk -v v="$version" 'index($0, "## [" v "]") == 1 { on = 1; next } /^## \[/ || /^\[[^]]+\]: / { on = 0 } on' CHANGELOG.md)
grep -q '[^[:space:]]' <<<"$notes" || fail "CHANGELOG.md's section for $version is empty"
grep -q "^\[$version\]: " CHANGELOG.md || fail "CHANGELOG.md has no link line '[$version]: ...'"

if [ "${1:-}" = "--tag" ]; then
    [ "${2:-}" = "v$version" ] || fail "tag ${2:-} doesn't match PETITE_VERSION $version"
elif [ -n "${1:-}" ] && [ "${1}" != "0000000000000000000000000000000000000000" ]; then
    base=$1
    git cat-file -e "$base^{commit}" 2>/dev/null || fail "unknown base commit $base"
    changed=$(git diff --name-only "$base" HEAD)
    if grep -Eq '^(src/|Makefile$)' <<<"$changed"; then
        old=$(git show "$base:src/version.h" 2>/dev/null | version_of || true)
        [ "$old" != "$version" ] || fail "src/ or the Makefile changed but PETITE_VERSION is still $version: bump it (SemVer)"
        grep -qx 'CHANGELOG.md' <<<"$changed" || fail "the version changed to $version but CHANGELOG.md didn't"
    fi
fi
echo "$version"
