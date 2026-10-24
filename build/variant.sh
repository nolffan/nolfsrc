#!/usr/bin/env bash
# The game built from the same source with project defines that the release
# build never has -- never for measuring: progress is only ever scored on the
# release build (`make`, `make progress`).
#
#   build/variant.sh debug         make debug          -> out/nolf-legacy-debug/
#   build/variant.sh modern        make modern         -> out/nolf-modern-release/
#   build/variant.sh modern-debug  make modern-debug   -> out/nolf-modern-debug/
#
# (on the host the make targets run this in the build container; inside
# `make shell` run it directly). Play the result exactly like out/nolf-legacy-release/:
# out/<name>/linux-run.sh (the launcher), or build/playtest.sh out/<name> ...
#
#   debug    NOLFSRC_DEBUG and symbols, for finding out why something goes
#            wrong. Compile + /Z7 /Oy- /D NOLFSRC_DEBUG (debug information,
#            frame pointers kept so a backtrace can walk the stack; every
#            other flag, the optimisation included, is the release one);
#            resource scripts + /d NOLFSRC_DEBUG;
#            link + /DEBUG /OPT:REF /INCREMENTAL:NO (a .pdb per binary, the
#            same unreferenced-code removal as release). The .pdb and .map of
#            every binary go beside it. src/nolfsrc/nolfsrc_debug.h's
#            NOLFSRC_LOG lines and lithtech.exe's crash backtraces
#            (nolfsrc_crash.h) go to nolfsrc.log in the game's directory.
#   modern   NOLFSRC_MODERN: the retail bugs fixed (RETAIL_BUGS.md) and new
#            features (MODERN.md). Compile + /D NOLFSRC_MODERN, resource
#            scripts + /d NOLFSRC_MODERN, nothing else; then the game-data
#            edits in src/modern/data/ are applied to the data from the
#            discs (tools/moddata.py), into custom/. And open-source
#            middleware instead of RAD's (NOLF_MIDDLEWARE=open, build/ext.sh):
#            OpenMiles for Miles, our FFmpeg-based player for Bink, built
#            from the ext/ submodules into out/ext/; the movies play.
#   modern-debug   both.
#
# The flags reach every compile and link through build/env.sh's
# NOLFSRC_CFLAGS / NOLFSRC_LDFLAGS, which are empty for every release
# command -- and which build/env.sh refuses outside the trees this script
# makes, so they cannot reach the release build by accident.
#
# One gap in the debug information: Intel C/C++ 4.0 emits none for a file
# compiled with /Qip (the d3d.ren files whose names end in qip/qipni/...),
# so frames in those functions have no name; the crash log gives their
# address as the .map file beside d3d.ren lists it (tools/mapaddr.py).
#
# It never touches the release build. The source is copied (without any
# build output) into out/build-<variant>/ and built there by the same
# build/all.sh, so src/*/Release -- what `make progress` scores -- and
# out/nolf-legacy-release/ are left exactly as they were. The copy keeps its
# own build outputs from one run to the next, and only changed sources are
# rewritten into it, so a rebuild redoes only what changed. The game data
# are extracted into out/<name>/ on its first run (about 1.1 GB, a few
# minutes) and kept after that. Last, the game directory is packaged as
# out/<name>-private.tar.gz and -public.tar.gz, like the release build's
# (build/package.sh).
set -e
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

VARIANT="${1:-}"
case "$VARIANT" in
    debug)        CFLAGS="/Z7 /Oy- /D NOLFSRC_DEBUG"
                  LDFLAGS="/DEBUG /OPT:REF /INCREMENTAL:NO"
                  RCFLAGS="/d NOLFSRC_DEBUG" ;;
    modern)       CFLAGS="/D NOLFSRC_MODERN"
                  LDFLAGS=""
                  RCFLAGS="/d NOLFSRC_MODERN" ;;
    modern-debug) CFLAGS="/Z7 /Oy- /D NOLFSRC_DEBUG /D NOLFSRC_MODERN"
                  LDFLAGS="/DEBUG /OPT:REF /INCREMENTAL:NO"
                  RCFLAGS="/d NOLFSRC_DEBUG /d NOLFSRC_MODERN" ;;
    *) echo "usage: build/variant.sh debug|modern|modern-debug" >&2; exit 2 ;;
esac
# the game directory it builds: out/nolf-<legacy|modern>-<release|debug>/
case "$VARIANT" in
    debug)        NAME=nolf-legacy-debug ;;
    modern)       NAME=nolf-modern-release ;;
    modern-debug) NAME=nolf-modern-debug ;;
esac

if ! command -v wine >/dev/null 2>&1; then
    echo "error: no wine here -- run \`make $VARIANT\` from the repository root, or" >&2
    echo "       run this inside \`make shell\`." >&2
    exit 1
fi

TREE="$REPO/out/build-$VARIANT"
echo "== $VARIANT tree: src/ build/ tools/ -> out/build-$VARIANT/ (sources only; its outputs kept)"
mkdir -p "$TREE"
source "$REPO/build/project.sh"
python3 tools/releasesrc.py mirror "$REPO/src" "$TREE/src"
rm -rf "$TREE/build" "$TREE/tools"
tar -C "$REPO" -cf - --exclude='__pycache__' build tools | tar -C "$TREE" -xf -
# the compilers, and the retail game where build/retail.sh looks for it
[ -L "$TREE/toolchains" ] || ln -s ../../toolchains "$TREE/toolchains"
[ -L "$TREE/rez" ] || ln -s ../../rez "$TREE/rez"
# the modern build's open-source middleware (build/ext.sh), built once into
# out/ext/ for every variant
[ -L "$TREE/ext" ] || ln -s ../../ext "$TREE/ext"

export NOLFSRC_CFLAGS="$CFLAGS"
export NOLFSRC_LDFLAGS="$LDFLAGS"
export NOLFSRC_RCFLAGS="$RCFLAGS"
export NOLF_GAME_OUT="$REPO/out/$NAME"
export NOLF_PACKAGE=0
case "$VARIANT" in modern*) export NOLF_MIDDLEWARE=open NOLF_EXT_OUT="$REPO/out/ext" ;; esac
bash "$TREE/build/all.sh"

case "$VARIANT" in modern*)
    echo
    echo "== modern data: src/modern/data/ recipes -> out/$NAME/custom/"
    bash "$TREE/build/ps2/ps2build.sh" clean "$NOLF_GAME_OUT"
    python3 "$TREE/tools/moddata.py" apply "$NOLF_GAME_OUT" "$TREE/src/modern/data"
    if [ -n "${NOLFSRC_PS2:-}" ]; then
        echo
        echo "== the PS2 levels (MODERN.md, P8): the PS2 disc in rez/isos/ps2/ -> out/$NAME/custom/"
        bash "$TREE/build/ps2/ps2build.sh" install "$NOLF_GAME_OUT"
    fi ;;
esac

case "$VARIANT" in *debug)
    echo
    echo "== symbols: each built binary's .pdb (and .map) beside it in out/$NAME/"
    # MANIFEST.txt has `built <source>\t<installed path>` for every binary we
    # built; its .pdb is the link output's name with .pdb, and the debugger
    # looks for it by that name in the binary's own directory.
    grep '^built ' "$NOLF_GAME_OUT/MANIFEST.txt" | sort -u |
    while IFS=$'\t' read -r src dest; do
        src="${src#built }"
        pdb="$TREE/${src%.*}.pdb"
        if [ -f "$pdb" ]; then
            cp "$pdb" "$NOLF_GAME_OUT/$(dirname "$dest")/"
            echo "   $(dirname "$dest")/$(basename "$pdb")"
        else
            echo "   (no .pdb for $dest)"
        fi
        # the linker's map, for addresses no .pdb names (see nolfsrc_crash.h)
        map="$TREE/${src%.*}.map"
        [ -f "$map" ] && cp "$map" "$NOLF_GAME_OUT/$(dirname "$dest")/"
    done ;;
esac
echo
echo "== package: out/$NAME/ -> out/$NAME-private.tar.gz, -public.tar.gz"
bash "$TREE/build/package.sh" "out/$NAME"
echo "== done: out/$NAME/ -- play it with out/$NAME/linux-run.sh"
