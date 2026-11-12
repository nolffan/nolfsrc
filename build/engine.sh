#!/usr/bin/env bash
# Build the engine binaries from src/engine/: ltmsg.dll, sres.dll, NOLF.exe
# (the launcher), NolfServ.exe (the stand-alone server), server.dll,
# lithtech.exe and d3d.ren, one at a time, and
# check them. (build/all.sh builds everything at once, with build/compile.sh.)
# `server`, `lithtech` and `d3d` compile a target's objects, and each *-link
# target (`launcher link` for NOLF.exe) links the whole image.
#
# The compiles and links themselves are build/build.mk's (section 3), which
# has the compiler revision and flags of every binary and file -- they
# differ per binary, and for some files within one, because they reproduce
# what the original build used -- and the committed link inputs each link
# reads (src/engine/<target>/link/). Each link's report is
# src/engine/<target>/Release/<target>-link.txt.
#
# Each target can also be checked against the retail binaries, function by
# function (ext/tools/exematch.py), which is the reconstruction loop of ext/tools/guides/:
#   build/engine.sh server verify          compile, then match every object
#   build/engine.sh server-one smgr.cpp [symbol]
#                                          recompile ONE file and match it
#                                          (also lithtech-one, d3d-one)
#   build/engine.sh server-link test       link, then load it under Wine
# The checks read the retail images from out/reference/ (build/reference.sh);
# building never does.
#
# src/engine/server/*.cpp is compiled into BOTH server.dll and lithtech.exe;
# a full build (build/compile.sh) rebuilds both objects after a change, a
# *-one loop only the one it names.
#
# Usage: build/engine.sh <target> [verify|link|test]   (in the build container)
set -e
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"    # compile src/ without its variant blocks (build/env.sh)
cd "$REPO"

# The retail images the checks compare against (build/reference.sh).
REF="${NOLF_REFERENCE:-out/reference}"
ref() {  # ref <file> -> its path under $REF, or a loud error
    if [ ! -f "$REF/$1" ]; then
        echo "error: $REF/$1 missing -- run build/reference.sh" >&2
        return 1
    fi
    echo "$REF/$1"
}

# match <image> <obj|lib>... [-- symbol-filter]: ext/tools/exematch.py, its report
# optionally narrowed to the lines naming one symbol (and the totals)
match() {
    local img="$1"; shift
    local args=() filter=""
    while [ $# -gt 0 ]; do
        if [ "$1" = "--" ]; then filter="${2:-}"; break; fi
        args+=("$1"); shift
    done
    if [ -n "$filter" ]; then
        python3 ext/tools/exematch.py "$img" "${args[@]}" | grep -Ei "${filter}|^matched"
    else
        python3 ext/tools/exematch.py "$img" "${args[@]}"
    fi
}

# one <target> <tu.cpp> [symbol]: the reconstruction loop -- recompile ONE
# file (whatever build/build.mk's flags for it are) and match only its object
one() {
    local t="$1" tu="${2:?usage: engine.sh $1-one <tu.cpp> [symbol-filter]}" img="$3" obj
    obj="src/engine/$t/Release/$(basename "${tu//\\//}" .cpp).obj"
    # a failed compile under Wine can exit 0 and leave the old object in
    # place, so a broken edit would measure as no change
    rm -f "$obj"
    nolfsrc_make "$obj" || exit 1
    match "$(ref "$img")" "$obj" -- "${4:-}"
}

# test_load <dir> <dll-or-ren> <loader.c> [extra files]: load the linked
# binary under Wine with its preferred base occupied (so the loader relocates
# it) through one of the ext/tools/ test programs
test_load() {
    local d="out/$1" bin="$2" src="$3"; shift 3
    mkdir -p "$d"
    cp "ext/tools/$src" "$bin" "$@" "$d/" || exit 1
    ( cd "$d" && run_cl /nologo /O1 "$src" \
          /link /out:"${src%.c}.exe" kernel32.lib user32.lib ole32.lib >/dev/null &&
      "$WINE" "${src%.c}.exe" "$(basename "$bin")" rebase 2>/dev/null ) || exit 1
}

case "${1:-}" in
    ltmsg)    nolfsrc_make ltmsg
              if [ "${2:-}" = verify ]; then
                  # resources and the C runtime (with its own default
                  # DllMain): the resources are compared payload for
                  # payload, the code as a whole (both relocation-normalised)
                  python3 ext/tools/bindiff.py src/engine/ltmsg/Release/ltmsg.dll "$(ref ltmsg.dll)"
                  python3 ext/tools/rsrcdiff.py src/engine/ltmsg/Release/ltmsg.dll "$(ref ltmsg.dll)"
              fi ;;
    sres)     nolfsrc_make sres ;;
    launcher) case "${2:-}" in
                  link)   nolfsrc_make launcher ;;
                  # every launcher function, matched in the retail NOLF.exe;
                  # the whole-exe link is refreshed first: ext/tools/progress.py
                  # uses it as a second, unmasked check
                  verify) nolfsrc_make launcher
                          D=src/engine/launcher/Release
                          python3 ext/tools/exematch.py "$(ref NOLF.exe)" \
                              $D/launcher.obj $D/gamedlg.obj $D/gamehelp.obj $D/dlgconfig.obj $D/dlgcustom.obj \
                              $D/dlghelp.obj $D/dlgsound.obj $D/dlgdisplay.obj $D/preview.obj $D/cdcheck.obj \
                              $D/dib.obj $D/dibmgr.obj $D/dibpal.obj $D/RezFind.obj $D/Utils.obj \
                              src/lithshared/out/RegMgr.lib ;;
                  *)      nolfsrc_make launcher-objs ;;
              esac ;;
    nolfserv) case "${2:-}" in
                  link)   nolfsrc_make nolfserv ;;
                  # every NolfServ.exe function of ours, matched in the
                  # retail image (the libraries it links included)
                  verify) nolfsrc_make nolfserv-objs
                          match "$(ref NolfServ.exe)" src/engine/nolfserv/Release/*.obj \
                              src/lithshared/out/CryptMgr.lib src/lithshared/out/RegMgr.lib \
                              src/lithshared/out/ButeMgr.lib ;;
                  *)      nolfsrc_make nolfserv-objs ;;
              esac ;;
    server|lithtech|d3d)
              nolfsrc_make "$1-objs"
              if [ "${2:-}" = verify ]; then
                  case "$1" in server) img=server.dll ;; lithtech) img=lithtech.exe ;; d3d) img=d3d.ren ;; esac
                  match "$(ref $img)" src/engine/$1/Release/*.obj
              fi ;;
    # The reconstruction loop: recompile ONE file, match only its object.
    #   build/engine.sh server-one smgr.cpp PlaySound
    #   build/engine.sh lithtech-one smgr.cpp PlaySound   (a bare name not in
    #                    src/engine/lithtech is server's, compiled for lithtech)
    #   build/engine.sh d3d-one d3d.cpp RenderDLLSetup
    server-one)   one server "${2:-}" server.dll "${3:-}" ;;
    lithtech-one) one lithtech "${2:-}" lithtech.exe "${3:-}" ;;
    d3d-one)      one d3d "${2:-}" d3d.ren "${3:-}" ;;
    #   build/engine.sh nolfserv-one ServerDlg.cpp OnTimer
    nolfserv-one) one nolfserv "${2:-}" NolfServ.exe "${3:-}" ;;
    server-link|lithtech-link|d3d-link)
              t="${1%-link}"
              nolfsrc_make "$t" || exit 1
              # "server-link test": CreateServer and DeleteServer on the loaded
              # DLL (needs ltmsg.dll beside it); "d3d-link test": list the
              # display modes and have RenderDLLSetup fill a RenderStruct
              if [ "${2:-}" = test ]; then
                  case "$t" in
                      server) nolfsrc_make ltmsg
                              test_load linktest src/engine/server/Release/server.dll loadtest.c \
                                  src/engine/ltmsg/Release/ltmsg.dll ;;
                      d3d)    test_load d3dtest src/engine/d3d/Release/d3d.ren d3dloadtest.c ;;
                  esac
              fi ;;
    *) echo "usage: $0 ltmsg [verify] | sres | launcher [link|verify] | nolfserv [link|verify]"
       echo "       $0 server|lithtech|d3d [verify]"
       echo "       $0 server-link [test] | lithtech-link | d3d-link [test]"
       echo "       $0 server-one|lithtech-one|d3d-one|nolfserv-one <file.cpp> [symbol]"; exit 1 ;;
esac
