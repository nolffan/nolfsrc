# What the shared ext/tools/ need to know about this project, for the scripts
# that call them (build/env.sh and build/variant.sh source this). The tools
# default to No One Lives Forever 2's layout; these name NOLF1's.

# ext/tools/releasesrc.py: the variant macros the release build resolves away,
# the directories the build writes its outputs into, and the files it
# generates beside their sources (the engines' link inputs, flex's and
# bison's output, CRes.dll's Game of the Year script, the lithshared
# libraries).
export RELEASESRC_VARIANTS="NOLFSRC_MODERN NOLFSRC_DEBUG"
export RELEASESRC_OUTPUT_DIRS="Release Final_Release Debug"
export RELEASESRC_GENERATED='^(engine/[^/]+/link/.*\.(obj|lib|cmd)|game/ObjectDLL/(AnimationLex\.cpp|AnimationParse\.cpp|AnimationLexSymbols\.h)|game/ClientRes/goty/cres_goty\.rc|lithshared/out/.*)$'

# ext/tools/mkdeps.py: the project's own headers, last on every INCLUDE path.
export MKDEPS_PROJECT_INCLUDE=src/nolfsrc

# nolfsrc_tree_layout <tree>: build/ and the tools copied into one of the
# build's own trees (out/release-tree/, out/build-<variant>/), the tools at
# <tree>/ext/tools/ so that they take <tree> for the project (as layout.py
# works it out), and every other submodule of ext/ (the compilers, the retail
# game where build/retail.sh looks for it, the modern build's middleware) a
# link to this checkout's. A tree made before the submodules moved to ext/
# had a link for ext/ itself and tools/, toolchains and rez at its top:
# those go first.
nolfsrc_tree_layout() {
    local repo tree="$1" d n
    repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    if [ -L "$tree/ext" ]; then rm "$tree/ext"; fi
    rm -rf "$tree/tools"
    rm -f "$tree/toolchains" "$tree/rez"
    mkdir -p "$tree/ext"
    [ -d "$tree/ext" ] && [ ! -L "$tree/ext" ] || { echo "error: $tree/ext is not a directory" >&2; return 1; }
    rm -rf "$tree/build" "$tree/ext/tools"
    tar -C "$repo" -cf - --exclude='__pycache__' build ext/tools | tar -C "$tree" -xf - || return 1
    for d in "$repo"/ext/*; do
        n="$(basename "$d")"
        [ "$n" = tools ] || [ -L "$tree/ext/$n" ] || ln -s "../../../ext/$n" "$tree/ext/$n" || return 1
    done
}
