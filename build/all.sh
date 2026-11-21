#!/usr/bin/env bash
# THE build: a runnable No One Lives Forever (GOTY) in out/nolf-legacy-release/, with every
# binary compiled from this repository's source and only DATA taken from the
# retail game.
#
#   make                    (on the host: builds the docker image, installs
#                            and checks the retail game, runs this in a
#                            container)
#   build/all.sh            (inside the build container, e.g. `make shell`)
#   make test               (then: play it -- build/playtest.sh on out/nolf-legacy-release/)
#
# Needs: the toolchain (ext/toolchains/, see ext/toolchains/README.md), Wine (the build
# container has it), and the retail Game of the Year Edition (Europe) as
# build/discs.py installs it from your two disc images (out/retail/nolf-eu-goty/,
# build/retail.sh). It only reads that directory: no retail game binary is
# run, linked or copied into the game, apart from the middleware of step 2.
#
# NOLF_NODISCS=1 builds without the retail game (the CI build, `make
# NODISCS=1`, for any variant). Every binary is the same as with the retail
# game: the art they are compiled with is committed (assets/art/). But
# step 1 copies no game data and step 2 no Miles or Bink: both write only
# the manifest's rows of those files (from build/retail-files.txt and
# build/retail-ranges.txt). The game directory is out/<name>-nodiscs/, with
# NOT-PLAYABLE.txt in it: without the .rez archives it does not play. Step
# 6 makes only its public archive, out/<name>-public.tar.gz, which is the
# playable form: unpacked, its install scripts copy the game data from the
# player's discs into it (build/mkinstall.py). It is the same archive as a
# build with the retail game makes.
#
# The steps, in order:
#   1. resources          game data from the retail game: the four .rez archives
#                         (with the retail game DLLs inside them zeroed),
#                         configs, text, movies. No binary.
#   2. third-party runtime  Miles (mss32.dll + its .flt/.m3d/.asi/.as6
#                         providers) and Bink (binkw32.dll), from the retail
#                         game.
#                         The ONLY binaries not built here: licensed
#                         middleware with no source, which lithtech.exe and
#                         NOLF.exe import -- the game cannot load without
#                         Miles. Isolated in this one step so they are easy
#                         to find and to replace -- which the modern build
#                         does (NOLF_MIDDLEWARE=open, set by build/variant.sh):
#                         OpenMiles' mss32.dll and our FFmpeg-based
#                         binkw32.dll, built from the ext/ submodules by
#                         build/ext.sh, and no Miles provider files (plus
#                         our opengl.ren, which ext.sh also builds).
#   2b. launcher art      the splash bitmap, palette, music, icon and cursors
#                         that launcher.rc compiles into NOLF.exe, the icons
#                         and wizard bitmap nolfserv.rc compiles into
#                         NolfServ.exe, and the icon and console font
#                         lithtech.rc compiles into lithtech.exe: the retail
#                         art, committed in assets/art/ (made from the retail
#                         binaries' resources by `make art`, and checked
#                         against them in every build with the retail game),
#                         copied into src/engine/{launcher,nolfserv,lithtech}/res/
#                         (data, not code).
#   2c. Game of the Year text  the GOTY strings and credits our CRes.dll is
#                         compiled with (the retail GOTY CRES.dll's, which
#                         Monolith's v1.003 source lacks), the same way, from
#                         assets/art/goty/ into src/game/ClientRes/goty/.
#   3. compile            every binary, by build/build.mk, all at once (one
#                         job per CPU; NOLFSRC_JOBS=<n> to change it) and only
#                         what changed since the last build (`make clean`
#                         starts from nothing):
#                         the lithshared static libraries, our GameSpy
#                         replacements included; the game DLLs CRes.dll,
#                         CShell.dll, Object.lto from src/game/ (the official
#                         source release), Final Release; the engine,
#                         ltmsg.dll, sres.dll, NOLF.exe, server.dll,
#                         lithtech.exe, d3d.ren from src/engine/.
#   4. assemble           the built binaries into out/nolf-legacy-release/, laid out like a
#                         retail install: the engine beside the data, the game
#                         DLLs in the custom/ rez directory the engine
#                         searches last-first (nolfcmds.txt: `-rez custom`),
#                         which the release launcher loads as the retail one
#                         did. The modern build instead packs them, with
#                         assets/modern/rez/ (our own data files), into
#                         nolfsrc.rez, the archive it loads last of the
#                         game's, with lithtools' lithrez (build/lithrez.sh).
#                         assets/modern/rez/ includes the retail files that
#                         the modern build changes (bug fixes, and changes
#                         as a mod makes them), made from the recipes of
#                         src/modern/data/ (build/moderndata.py, which this
#                         step uses to check them); its defaults.cfg and
#                         defctrls.cfg, assets/modern/gamedir/, replace the
#                         retail ones in step 1.
#   5. audit              every PE file in out/nolf-legacy-release/ listed with its origin;
#                         the build fails if any is unaccounted for or if a
#                         .rez archive still holds one.
#   6. package            out/nolf-legacy-release-private.tar.gz: the game directory as
#                         one file, to copy to the machine that plays it (unpack,
#                         then ./linux-run.sh -- see its header; Wine needed there),
#                         and -public.tar.gz: the same without the discs' bytes
#                         (their data, and the art in our binaries), which its
#                         install scripts copy from the player's disc images.
#
# Every other input is a committed source file (including the committed link
# inputs src/engine/*/link/*, src/engine/d3d/d3ddata.cpp and
# src/engine/launcher/libs/mss32.def). No step uses retail code: steps 2b
# and 2c check the committed art against the retail binaries' resources.
set -e
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Wine and the toolchain live in the build container: on the host, use make.
if ! command -v wine >/dev/null 2>&1; then
    echo "error: no wine here -- run \`make\` from the repository root (it runs this" >&2
    echo "       script in the build container), or run it inside \`make shell\`." >&2
    exit 1
fi
export WINEDEBUG=-all

cd "$REPO"
source build/retail.sh
OUT="${NOLF_GAME_OUT:-out/nolf-legacy-release}"
NODISCS=
if [ "${NOLF_NODISCS:-0}" = 1 ]; then
    NODISCS=1
    RETAIL=-
    # not the game directory of a real build: this one does not play
    # (build/variant.sh gives its own, out/<name>-nodiscs)
    OUT="${NOLF_GAME_OUT:-out/nolf-legacy-release-nodiscs}"
else
    RETAIL="$(retail_dir)"
    retail_verify
fi
T0=$(date +%s)
step() { echo; echo "######## $* ($(( $(date +%s) - T0 ))s)"; }
need() {  # need <file> <what> [log]: a build output must exist
    [ -f "$1" ] && return 0
    echo "error: $2 was not built ($1 missing)" >&2
    [ -n "${3:-}" ] && [ -f "$3" ] && grep -E "error|unresolved" "$3" | head -20 >&2
    exit 1
}

if [ -n "$NODISCS" ]; then
    step "1. resources: without the retail game, only the manifest rows of its data -> $OUT"
    mkdir -p "$OUT"
    cat > "$OUT/NOT-PLAYABLE.txt" <<EOF
This directory was built without the retail game (NODISCS=1). It has no
game data (no .rez archives, no movies, no Miles): it does not play.
The playable form is its public archive, $(basename "$OUT" -nodiscs)-public.tar.gz
beside it: unpack it into an empty directory, then run its install script
with your two disc images (docs/INSTALL.md).
EOF
else
    step "1. resources: game data from $RETAIL -> $OUT"
    rm -f "$OUT/NOT-PLAYABLE.txt"
fi
python3 build/retailextract.py resources "$RETAIL" "$OUT"
# The modern build (build/variant.sh): our archive (step 4); every other
# build keeps the game DLLs in custom/
MODERN=
case "${NOLFSRC_CFLAGS:-}" in *NOLFSRC_MODERN*) MODERN=modern ;; esac
if [ -n "$MODERN" ]; then
    # The modern build's own defaults.cfg and defctrls.cfg, which the engine
    # reads from the game directory, in place of the retail ones that
    # `resources` copied (assets/README.md); the manifest row says so.
    for f in assets/modern/gamedir/*; do
        n="$(basename "$f")"
        cp "$f" "$OUT/$n"
        sed -i "/\t${n//./\\.}\$/d" "$OUT/MANIFEST.txt"
        printf 'ours %s\t%s\n' "$f" "$n" >> "$OUT/MANIFEST.txt"
        echo "   $n  <-  $f"
    done
fi
# autoexec.cfg starts as the shipped defaults (the engine rewrites it on
# exit); a rebuild keeps the player's.
[ -f "$OUT/autoexec.cfg" ] || [ -n "$NODISCS" ] || cp "$OUT/defaults.cfg" "$OUT/autoexec.cfg"
mkdir -p "$OUT/Save"
[ -n "$MODERN" ] || mkdir -p "$OUT/custom"
# nolfcmds.txt: the command file the engine starts with, as a fresh build
# writes it (the launcher rewrites it; build/playtest.sh uses it),
# build/nolfcmds.sh.
bash build/nolfcmds.sh "${NOLF_MIDDLEWARE:-retail}" $MODERN > "$OUT/nolfcmds.txt"
# startup.txt: the rez stack NolfServ.exe, the dedicated server, loads
# (ServerDlg.cpp, AddResources: Rez0, Rez1, ... to the first one missing).
# Ours, written whole: the retail file's nine names, Rez0-Rez8 (with the
# names the GOTY install has no archive of, as the retail file has them),
# then ours last, as nolfcmds.txt has them
# for the engine: custom/, with our game DLLs; in the modern build
# nolfsrc.rez.
if [ -n "$MODERN" ]; then OURS=nolfsrc.rez; else OURS=custom; fi
n=0
{ printf '[Rez]\r\n'
  for r in nolf nolf.rez nolf2.rez nolfdll.rez nolfl.rez nolfu003.rez nolfc001.rez \
           nolfc002.rez nolfgoty.rez "$OURS"; do
      printf 'Rez%d = "%s"\r\n' $n "$r"
      n=$((n + 1))
  done; } > "$OUT/startup.txt"
sed -i '/\tstartup\.txt$/d' "$OUT/MANIFEST.txt"
printf 'ours build/all.sh\tstartup.txt\n' >> "$OUT/MANIFEST.txt"
# linux-run.sh: start the game with the host's Wine, through our NOLF.exe;
# linux-server.sh: a dedicated server, NolfServ.exe (build/game-scripts/;
# usage in their headers).
rm -f "$OUT/run.sh" "$OUT/run-launcher.sh"   # their names before the linux- prefix
rm -f "$OUT/linux-run-launcher.sh"           # linux-run.sh's name before it replaced
                                             # the launcher-less script
cp build/game-scripts/linux-run.sh build/game-scripts/linux-server.sh "$OUT/"
chmod +x "$OUT/linux-run.sh" "$OUT/linux-server.sh"

if [ "${NOLF_MIDDLEWARE:-retail}" = open ]; then
    step "2. open runtime: OpenMiles, our Bink player on FFmpeg and our renderers (build/ext.sh)"
    bash build/ext.sh
    python3 build/retailextract.py openruntime "${NOLF_EXT_OUT:-out/ext}" "$OUT"
else
    if [ -n "$NODISCS" ]; then
        step "2. third-party runtime: without the retail game, only the manifest rows of Miles + Bink"
    else
        step "2. THIRD-PARTY RUNTIME: Miles + Bink from $RETAIL (the only binaries not built here)"
    fi
    python3 build/retailextract.py thirdparty "$RETAIL" "$OUT"
fi

step "2b, 2c. the launcher's, the server's and the engine's art, and the Game of the Year text: assets/art/ -> src/engine/{launcher,nolfserv,lithtech}/res/, src/game/ClientRes/goty/"
# (with the retail game: first checked against the retail binaries'
# resources, as make art-check does)
[ -n "$NODISCS" ] || python3 build/retailextract.py art check "$RETAIL"
python3 build/retailextract.py art install

step "3. compile: libraries, game DLLs, engine (build/compile.sh: build/build.mk, ${NOLFSRC_JOBS:-$(nproc)} jobs)"
GAME_CONFIG=final bash build/compile.sh all
for f in src/engine/ltmsg/Release/ltmsg.dll src/engine/sres/Release/sres.dll \
         src/engine/launcher/Release/NOLF.exe src/engine/nolfserv/Release/NolfServ.exe \
         src/engine/server/Release/server.dll \
         src/engine/lithtech/Release/lithtech.exe src/engine/d3d/Release/d3d.ren \
         src/game/ClientRes/Release/CRes.dll src/game/ClientShellDLL/Final_Release/CShell.dll \
         src/game/ObjectDLL/Final_Release/Object.lto; do
    need "$f" "$(basename "$f")"
done

step "4. assemble: the built binaries -> $OUT"
if [ -n "$MODERN" ]; then
    # What older modern builds left: in custom/, where builds before
    # nolfsrc.rez put the game DLLs and their data edits (every launcher
    # loads it, as the retail one always did, before the update's and GOTY's
    # archives), by the manifest's rows and custom/MODERN-DATA.txt's list;
    # and nolfsrcdisc.rez, MODERN-DATA.txt and the <file>.retail copies,
    # which builds before assets/modern/ made from the discs. Anything else
    # -- a player's own files, or Custom/*.rez for the launcher's custom
    # archives -- stays, and so does custom/ then. Never for a legacy game
    # directory, whose game DLLs live there.
    python3 build/retailextract.py oldmodern "$OUT"
else
    # our archives, which only the modern build has (a legacy directory
    # built while the legacy build had them too)
    rm -f "$OUT/nolfsrc.rez" "$OUT/nolfsrcdisc.rez"
fi
# The manifest's rows for files no longer here, and for our archives and
# their entries (written again below), go.
python3 build/retailextract.py prune "$OUT"
install_built() {  # install_built <built file> <dest relative to $OUT>
    cp "$1" "$OUT/$2"
    printf 'built %s\t%s\n' "$1" "$2" >> "$OUT/MANIFEST.txt"
    echo "   $2  <-  $1"
}
install_built src/engine/lithtech/Release/lithtech.exe   lithtech.exe
install_built src/engine/server/Release/server.dll       server.dll
# The modern build's default renderer is d3d9.ren (src/modern/render/); our
# d3d.ren, with the modern fixes, ships beside it as d3d7.ren.
case "${NOLFSRC_CFLAGS:-}" in
    *NOLFSRC_MODERN*) rm -f "$OUT/d3d.ren"         # from a build before the rename
                      [ -f "$OUT/MANIFEST.txt" ] && sed -i '/\td3d\.ren$/d' "$OUT/MANIFEST.txt"
                      install_built src/engine/d3d/Release/d3d.ren d3d7.ren ;;
    *)                install_built src/engine/d3d/Release/d3d.ren d3d.ren ;;
esac
install_built src/engine/launcher/Release/NOLF.exe       NOLF.exe
case "${NOLFSRC_CFLAGS:-}" in
    *NOLFSRC_MODERN*) need src/engine/launcher/Release/NOLFLauncher.exe NOLFLauncher.exe
                      install_built src/engine/launcher/Release/NOLFLauncher.exe NOLFLauncher.exe ;;
esac
install_built src/engine/nolfserv/Release/NolfServ.exe   NolfServ.exe
install_built src/engine/ltmsg/Release/ltmsg.dll         ltmsg.dll
if [ -z "$MODERN" ]; then
    # the game DLLs and sres.dll go in the custom/ rez directory, which
    # overrides the (zeroed) copies inside the archives
    install_built src/game/ClientRes/Release/CRes.dll            custom/cres.dll
    install_built src/game/ClientShellDLL/Final_Release/CShell.dll custom/cshell.dll
    install_built src/game/ObjectDLL/Final_Release/Object.lto    custom/object.lto
    install_built src/engine/sres/Release/sres.dll           custom/sres.dll
else
    # The modern build: the game DLLs and sres.dll go in nolfsrc.rez, which
    # overrides the (zeroed) copies inside the retail archives -- the engine and
    # the dedicated server copy them out of the rez stack, as they did the
    # retail ones -- with our own data files, assets/modern/rez/
    # (assets/README.md). Staged in out/stage/<game directory>/, never in the
    # game directory; the manifest has a row for each DLL in the archive.
    STAGE="$(dirname "$OUT")/stage/$(basename "$OUT")"
    rm -rf "$STAGE"
    mkdir -p "$STAGE/nolfsrc"
    stage_built() {  # stage_built <built file> <name in nolfsrc.rez>
        cp "$1" "$STAGE/nolfsrc/$2"
        printf 'built %s\t%s\n' "$1" "nolfsrc.rez:$2" >> "$OUT/MANIFEST.txt"
        echo "   nolfsrc.rez:$2  <-  $1"
    }
    stage_built src/game/ClientRes/Release/CRes.dll              cres.dll
    stage_built src/game/ClientShellDLL/Final_Release/CShell.dll cshell.dll
    stage_built src/game/ObjectDLL/Final_Release/Object.lto      object.lto
    stage_built src/engine/sres/Release/sres.dll                 sres.dll
    a=assets/modern/rez
    # (a name already staged, in any case, would silently replace that file)
    dup="$( { (cd "$STAGE/nolfsrc" && find . -type f); (cd "$a" && find -L . -type f); } |
            tr '[:upper:]' '[:lower:]' | sort | uniq -d)"
    [ -z "$dup" ] || { echo "error: $a/ has a file nolfsrc.rez already has: $dup" >&2; exit 1; }
    cp -rL "$a/." "$STAGE/nolfsrc/"
    echo "   nolfsrc.rez  <-  $a/ ($(find -L "$a" -type f | wc -l) files)"

    # assets/modern/: the retail files the modern build changes must be
    # what the src/modern/data/ recipes make of the discs' data (not
    # without the retail game: make progress checks it too)
    if [ -n "$NODISCS" ]; then
        echo "   assets/modern/: not checked against the recipes without the retail game (make progress checks it)"
    else
        python3 build/moderndata.py check
    fi

    step "4b. archive: $STAGE/nolfsrc/ -> nolfsrc.rez (lithtools' lithrez)"
    bash build/lithrez.sh
    # One fixed time (27 April 2001, the 1.003 update's readme's date) on every
    # entry, so that every build of the same inputs makes the same bytes.
    pack() {  # pack <staged directory> <archive> <manifest origin>
        rm -f "$OUT/$2.part"
        out/lithrez/lithrez c "$OUT/$2.part" "$1" --time 988329600 >/dev/null
        mv "$OUT/$2.part" "$OUT/$2"
        printf '%s\t%s\n' "$3" "$2" >> "$OUT/MANIFEST.txt"
        echo "   $2  <-  $1/ ($(find "$1" -type f | wc -l) files, $(stat -c %s "$OUT/$2") bytes)"
    }
    pack "$STAGE/nolfsrc" nolfsrc.rez "packed by lithrez: the game DLLs, assets/"
fi

step "5. audit: every PE file in $OUT"
python3 build/retailextract.py audit "$OUT"

# (NOLF_PACKAGE=0 skips it: build/variant.sh packages its game directory
# itself, once the variant's data and symbols are in)
if [ "${NOLF_PACKAGE:-1}" = 1 ] && [ -n "$NODISCS" ]; then
    step "6. package: $OUT -> $(dirname "$OUT")/$(basename "$OUT" -nodiscs)-public.tar.gz (no private archive without the retail game)"
    bash "$REPO/build/package.sh" "$OUT" "$(basename "$OUT" -nodiscs)"
elif [ "${NOLF_PACKAGE:-1}" = 1 ]; then
    step "6. package: $OUT -> $OUT-private.tar.gz, -public.tar.gz"
    bash "$REPO/build/package.sh" "$OUT"
fi

if [ -n "$NODISCS" ]; then
    step "done: $OUT ($(( $(date +%s) - T0 ))s) -- built without the retail game: it does not play (NOT-PLAYABLE.txt); its public archive, installed from the discs, does; check the build with make progress-ci"
else
    step "done: $OUT ($(( $(date +%s) - T0 ))s) -- play it with $OUT/linux-run.sh, or test it with make test"
fi
