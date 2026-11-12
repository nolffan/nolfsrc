# Source this to get the VC6-under-Wine build environment. It runs inside the
# build container (docker/Dockerfile; `make shell` gives you one), where Wine
# is on PATH and the toolchain is ext/toolchains/ in this checkout
# (ext/toolchains/README.md).

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MSVC="$REPO/ext/toolchains/msvc-6.00-9782"
if [ ! -f "$MSVC/VC98/Bin/CL.EXE" ]; then
    echo "error: $MSVC/VC98/Bin/CL.EXE not found -- the toolchain is missing (ext/toolchains/README.md)" >&2
    return 1 2>/dev/null || exit 1
fi
if ! command -v wine >/dev/null 2>&1; then
    echo "error: wine is not on PATH -- run this inside the build container (make shell)" >&2
    return 1 2>/dev/null || exit 1
fi
WINE=wine
WINESERVER=wineserver

source "$REPO/build/project.sh"

export WINEDEBUG=-all
export WINEDLLOVERRIDES="mscoree,mshtml="

# Windows-side view of the paths: Z: is / ...
#
# ... except for the game DLLs. VC6's output for CShell.dll and Object.lto
# depends on where the compiler finds things: the same source built in two
# directories whose paths differ in length gave two different binaries. So
# build/build.sh sets NOLFSRC_DRIVES=1 before sourcing this, and then the
# checkout's src/ is Wine's drive S: and its ext/toolchains/ drive T: (links in
# the prefix's dosdevices/), and the compilers see the same paths --
# S:\game\ObjectDLL as their working directory, T:\msvc-6.00-9782\... in INCLUDE --
# wherever the checkout is (the main one, a worktree, out/release-tree/).
# Everything else keeps Z:, and the links are removed for it: Intel C/C++
# builds a different d3d.ren from a working directory on a drive of its own,
# and the engine's and libraries' output never depended on the path.
WINEPFX="${WINEPREFIX:-$HOME/.wine}"
if [ "${NOLFSRC_DRIVES:-0}" = 1 ]; then
    [ -d "$WINEPFX/dosdevices" ] || "$WINE" cmd /c exit >/dev/null 2>&1 || true
    ln -sfn "$REPO/src" "$WINEPFX/dosdevices/s:"
    ln -sfn "$REPO/ext/toolchains" "$WINEPFX/dosdevices/t:"
else
    rm -f "$WINEPFX/dosdevices/s:" "$WINEPFX/dosdevices/t:"
fi
wpath() {  # wpath <unix path> -> its Windows path
    local p="$1"
    case "${NOLFSRC_DRIVES:-0}:$p" in
        1:"$REPO/src"|1:"$REPO/src"/*)             p="s:${p#"$REPO/src"}" ;;
        1:"$REPO/ext/toolchains"|1:"$REPO/ext/toolchains"/*) p="t:${p#"$REPO/ext/toolchains"}" ;;
        *)                                         p="z:$p" ;;
    esac
    echo "${p//\//\\}"
}
WREPO="$(wpath "$REPO")"
WVC="$(wpath "$MSVC")\\VC98"
WMSDEV="$(wpath "$MSVC")\\Common\\MSDev98"

# cl.exe / link.exe / rc.exe environment (wine imports these env vars)
export WINEPATH="$WVC\\Bin;$WMSDEV\\Bin"
export INCLUDE="$WVC\\INCLUDE;$WVC\\MFC\\INCLUDE;$WVC\\ATL\\INCLUDE"
export LIB="$WVC\\LIB;$WVC\\MFC\\LIB"

# src/nolfsrc/ (nolfsrc_debug.h) is on every compiler's include path, last,
# so any file of any target can include it. It holds nothing else, so it
# cannot change which header another #include finds.
WNOLFSRC="$(wpath "$REPO/src/nolfsrc")"
INCLUDE="$INCLUDE;$WNOLFSRC"

# Extra flags for every compile and every link, for the debug and modern
# builds (build/variant.sh sets them). Empty otherwise -- and the release
# build's command lines are then exactly what they always were.
NOLFSRC_CFLAGS="${NOLFSRC_CFLAGS:-}"
NOLFSRC_LDFLAGS="${NOLFSRC_LDFLAGS:-}"
NOLFSRC_RCFLAGS="${NOLFSRC_RCFLAGS:-}"    # resource scripts: /d NAME
# They are only allowed in the trees build/variant.sh makes (out/build-*/):
# anywhere else they would put NOLFSRC_DEBUG or NOLFSRC_MODERN code into the
# objects `make progress` scores, which must only ever be the release build.
if [ -n "$NOLFSRC_CFLAGS$NOLFSRC_LDFLAGS$NOLFSRC_RCFLAGS" ]; then
    case "$REPO" in
        */out/build-*) ;;
        *) echo "error: NOLFSRC_CFLAGS / LDFLAGS / RCFLAGS are set, but this is not a" >&2
           echo "       build/variant.sh tree ($REPO). Unset them; use make debug/modern." >&2
           return 1 2>/dev/null || exit 1 ;;
    esac
fi

# The compile flags go AFTER the command's own (a later flag wins: /Oy- must
# follow the /O2 that implies /Oy), but before a /link, after which the
# rest of the line belongs to the linker. Sets NOLFSRC_ARGS.
nolfsrc_cl_args() {
    local a linked=0
    NOLFSRC_ARGS=()
    for a in "$@"; do
        if [ $linked = 0 ] && [ "${a,,}" = "/link" ]; then
            NOLFSRC_ARGS+=($NOLFSRC_CFLAGS); linked=1
        fi
        NOLFSRC_ARGS+=("$a")
    done
    [ $linked = 1 ] || NOLFSRC_ARGS+=($NOLFSRC_CFLAGS)
}

run_cl()   { nolfsrc_cl_args "$@"; "$WINE" "$MSVC/VC98/Bin/CL.EXE" "${NOLFSRC_ARGS[@]}"; }

# Compiler-revision override: CL_REV=8168|8447 makes run_cl use
# ext/toolchains/msvc-6.00-<rev> (with its own CRT headers) instead of the
# default msvc-6.00-9782 (cl 12.00.8804, back end 12.00.9782). The build
# scripts set it per target, to the revision each original binary was built
# with.
CLDIR=""
if [ -n "${CL_REV:-}" ] && [ -d "$REPO/ext/toolchains/msvc-6.00-$CL_REV" ]; then
    CLDIR="$REPO/ext/toolchains/msvc-6.00-$CL_REV"
elif [ -n "${CL_REV:-}" ] && [ -d "$REPO/ext/toolchains/msvc-$CL_REV" ]; then
    CLDIR="$REPO/ext/toolchains/msvc-$CL_REV"
fi
if [ -n "$CLDIR" ]; then
    WCLDIR="$(wpath "$CLDIR")"
    run_cl() { INCLUDE="$WCLDIR\\INCLUDE;$WCLDIR\\MFC\\INCLUDE;$WNOLFSRC" \
               nolfsrc_cl_args "$@"; "$WINE" "$CLDIR/BIN/cl.exe" "${NOLFSRC_ARGS[@]}"; }
fi
# d3d.ren is built with Intel C/C++ 4.0 (ext/toolchains/icl-4.0), as the original
# was; icl uses msvc-6.00-8168's headers and libraries beside its own.
ICLDIR="$REPO/ext/toolchains/icl-4.0"
run_icl() {
    if [ ! -f "$ICLDIR/BIN/icl.exe" ]; then
        echo "error: $ICLDIR/BIN/icl.exe not present." >&2
        echo "       d3d.ren needs Intel C/C++ 4.0; see ext/toolchains/README.md" >&2
        return 1
    fi
    local WI WV
    WI="$(wpath "$ICLDIR")"
    WV="$(wpath "$REPO/ext/toolchains/msvc-6.00-8168")"
    nolfsrc_cl_args "$@"
    INCLUDE="$WI\\INCLUDE;$WV\\INCLUDE;$WNOLFSRC" LIB="$WI\\LIB;$WV\\LIB" \
        "$WINE" "$ICLDIR/BIN/icl.exe" "${NOLFSRC_ARGS[@]}"
}

# The release build compiles src/ WITHOUT its NOLFSRC_MODERN / NOLFSRC_DEBUG
# blocks, not merely with them switched off: a block the compiler skips
# still moves everything below it, and VC6 lets the file's layout decide
# where some out-of-line inline functions go, so enough skipped blocks can
# change the release binary (ext/tools/releasesrc.py). build/libs.sh, build.sh
# and engine.sh call this first, with their own arguments: in a checkout it
# brings out/release-tree/ up to date -- src/ with those blocks resolved as
# the release build sees them, the same build/ and ext/tools/, this checkout's
# build outputs -- runs the script there, copies the build outputs back into
# this checkout's src/ (where `make progress` and the checks read them) and
# exits with the script's status. In a build/variant.sh tree (out/build-*/,
# which must see the blocks), or inside the release tree, it does nothing.
nolfsrc_release_tree() {
    [ -n "${NOLFSRC_IN_RELEASE_TREE:-}" ] && return 0
    case "$REPO" in */out/build-*) return 0 ;; esac
    local tree="$REPO/out/release-tree" script rc=0
    script="$(basename "${BASH_SOURCE[1]}")"
    mkdir -p "$tree"
    python3 "$REPO/ext/tools/releasesrc.py" sync "$REPO/src" "$tree/src" || exit 1
    python3 "$REPO/ext/tools/releasesrc.py" outputs "$REPO/src" "$tree/src" || exit 1
    nolfsrc_tree_layout "$tree" || exit 1
    ( cd "$tree" && NOLFSRC_IN_RELEASE_TREE=1 \
        NOLF_REFERENCE="$(cd "$REPO" && realpath -m "${NOLF_REFERENCE:-out/reference}")" \
        bash "build/$script" "$@" ) || rc=$?
    python3 "$REPO/ext/tools/releasesrc.py" outputs "$tree/src" "$REPO/src" || exit 1
    exit $rc
}

# Compile and link through build/build.mk, in this tree (run it after
# nolfsrc_release_tree): every listed target, NOLFSRC_JOBS at a time (default:
# one per CPU), each job's output kept together.
nolfsrc_make() {
    make -C "$REPO" -f build/build.mk --no-print-directory -Otarget \
        -j"${NOLFSRC_JOBS:-$(nproc)}" "$@"
}

run_link() { "$WINE" "$MSVC/VC98/Bin/LINK.EXE" $NOLFSRC_LDFLAGS "$@"; }
run_lib()  { "$WINE" "$MSVC/VC98/Bin/LIB.EXE"  "$@"; }
run_rc()   { "$WINE" "$MSVC/Common/MSDev98/Bin/RC.EXE" $NOLFSRC_RCFLAGS "$@"; }
