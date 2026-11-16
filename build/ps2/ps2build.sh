#!/usr/bin/env bash
# The modern build's PS2 levels (MODERN.md, P8): the PlayStation 2 release's
# three "Nine years ago" flashbacks and its "A Man of Influence" third scene,
# with the Baron's training rooms, converted from the PS2 disc.
# Opt-in: `make modern PS2=1` sets NOLFSRC_PS2, and then the PS2 disc image
# must be in ext/rez/isos/ps2/ (either the USA or the European disc: they
# give the same files).
#
#   build/ps2/ps2build.sh strings <out.rc>   the levels' text, for CRes.dll
#                                            (build/all.sh, step 2d)
#   build/ps2/ps2build.sh install <gamedir> <dir>
#                                            the converted levels, models,
#                                            textures, sounds and images, and
#                                            the attribute edits, into <dir>,
#                                            which build/all.sh packs into
#                                            the game directory's
#                                            nolfsrcps2.rez (step 4c)
#   build/ps2/ps2build.sh oldcustom <gamedir>
#                                            take out what an install of a
#                                            build before nolfsrcps2.rez wrote
#                                            into custom/ (custom/PS2-DATA.txt
#                                            lists it; build/all.sh, step 4)
#
# Without NOLFSRC_PS2, `strings` and `install` do nothing, so a build without
# it has none of the PS2 content. build/package.sh makes no public archive of
# a game directory with nolfsrcps2.rez, since that data comes from the PS2
# disc, which the public archive's install scripts cannot read.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

disc() {
    local f
    for f in "$REPO"/ext/rez/isos/ps2/*.iso "$REPO"/ext/rez/isos/ps2/*.ISO; do
        [ -f "$f" ] && { echo "$f"; return 0; }
    done
    echo "error: NOLFSRC_PS2 is set, but there is no PS2 disc image (.iso) in ext/rez/isos/ps2/" >&2
    return 1
}

# Only the files custom/PS2-DATA.txt lists, and then the folders they leave
# empty; nothing else in custom/ (a player's files, the launcher's
# Custom\*.rez) and none of our archives. A modern game directory only.
oldcustom() {
    local custom="$1/custom" list f d
    list="$custom/PS2-DATA.txt"
    [ -f "$list" ] || return 0
    grep -q '	NOLFLauncher\.exe$' "$1/MANIFEST.txt" 2>/dev/null ||
        { echo "error: $1 is not a modern game directory; its custom/ is left alone" >&2; return 1; }
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        rm -f "$custom/$f"
        d="$(dirname "$custom/$f")"
        while [ "$d" != "$custom" ] && rmdir "$d" 2>/dev/null; do d="$(dirname "$d")"; done
    done < "$list"
    rm -f "$list"
    echo "   removed the PS2 levels' files of an earlier build from $1/custom/"
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
    game="${2:?usage: ps2build.sh install <gamedir> <dir>}"
    dir="${3:?usage: ps2build.sh install <gamedir> <dir>}"
    [ -n "${NOLFSRC_PS2:-}" ] || exit 0
    iso="$(disc)"
    python3 "$HERE/ps2install.py" --ps2 "$iso" --game "$game" --out "$dir"
    python3 "$HERE/ps2data.py" "$game" "$dir"
    ;;
oldcustom)
    oldcustom "${2:?usage: ps2build.sh oldcustom <gamedir>}"
    ;;
*)
    sed -n '2,26p' "$0" >&2
    exit 2
    ;;
esac
