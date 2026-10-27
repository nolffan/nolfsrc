#!/usr/bin/env bash
# The modern build's PS2 levels (MODERN.md, P8): the PlayStation 2 release's
# three "Nine years ago" flashbacks and its "A Man of Influence" third scene,
# with the Baron's training rooms, converted from the PS2 disc.
# Opt-in: `make modern PS2=1` sets NOLFSRC_PS2, and then the PS2 disc image
# must be in rez/isos/ps2/ (either the USA or the European disc: they give
# the same files).
#
#   build/ps2/ps2build.sh strings <out.rc>   the levels' text, for CRes.dll
#                                            (build/all.sh, step 2d)
#   build/ps2/ps2build.sh install <gamedir>  the converted levels, models,
#                                            textures, sounds and images, and
#                                            the attribute edits, into its
#                                            custom/ (build/variant.sh)
#   build/ps2/ps2build.sh clean <gamedir>    take out what an install wrote
#                                            (build/variant.sh, before the
#                                            modern data: an install's
#                                            attribute files are on top of
#                                            those)
#
# Without NOLFSRC_PS2, `strings` and `install` do nothing, so a build without
# it has none of the PS2 content. Everything an
# install writes is listed in custom/PS2-DATA.txt; build/package.sh makes no
# public archive of a game directory that has it, since that data comes from
# the PS2 disc, which the public archive's install scripts cannot read.
set -e
set -o pipefail    # (a failed step before a tee fails the build)
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

disc() {
    local f
    for f in "$REPO"/rez/isos/ps2/*.iso "$REPO"/rez/isos/ps2/*.ISO; do
        [ -f "$f" ] && { echo "$f"; return 0; }
    done
    echo "error: NOLFSRC_PS2 is set, but there is no PS2 disc image (.iso) in rez/isos/ps2/" >&2
    return 1
}

clean() {
    local custom="$1/custom" list
    list="$custom/PS2-DATA.txt"
    [ -f "$list" ] || return 0
    while IFS= read -r f; do
        [ -n "$f" ] && rm -f "$custom/$f"
    done < "$list"
    rm -f "$list"
    echo "   removed the PS2 levels' files from $1/custom/"
}

case "${1:-}" in
strings)
    out="${2:?usage: ps2build.sh strings <out.rc>}"
    rm -f "$out"
    [ -n "${NOLFSRC_PS2:-}" ] || { echo "   (no PS2 levels: make modern PS2=1 adds them)"; exit 0; }
    iso="$(disc)"
    python3 "$HERE/ps2strings.py" rc "$iso" "$HERE/ps2-string-map.txt" "$out"
    echo "   $(basename "$iso") -> $out"
    ;;
install)
    game="${2:?usage: ps2build.sh install <gamedir>}"
    [ -n "${NOLFSRC_PS2:-}" ] || exit 0
    iso="$(disc)"
    python3 "$HERE/ps2install.py" --ps2 "$iso" --game "$game"
    python3 "$HERE/ps2data.py" "$game" | tee /dev/stderr |
        sed -n 's|^ *custom/\([^ ]*\)  <- .*|\1|p' >> "$game/custom/PS2-DATA.txt"
    ;;
clean)
    clean "${2:?usage: ps2build.sh clean <gamedir>}"
    ;;
*)
    sed -n '2,24p' "$0" >&2
    exit 2
    ;;
esac
