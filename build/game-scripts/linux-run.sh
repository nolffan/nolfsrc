#!/bin/sh
# linux-run.sh -- start No One Lives Forever (built from source) with Wine,
# through NOLF.exe: direct start in modern builds, the launcher in legacy
# builds (built from src/engine/launcher).
#
#   ./linux-run.sh [launcher args...]
#
# Modern builds start immediately and wait for the game to exit. Open
# NOLFLauncher.exe in the same prefix for the interactive launcher.
# Legacy builds: click "Launch" and pick a performance setting on first run.
# The launcher writes nolfcmds.txt and starts lithtech.exe itself; Display
# and Advanced there set the renderer, the resolution and the engine's
# options. Arguments are handed to the launcher, which passes them on to the
# engine, e.g.
#   ./linux-run.sh ++windowed 1          a window, whatever Display says
#   ./linux-run.sh +DisableSound 1       no sound
#   ./linux-run.sh --help                this text
#
# The modern build starts as it was last left (Options > Display: full
# screen, a window or a borderless window, and its resolution); a fresh
# install starts full screen, at a resolution that fits the desktop.
# The release build always starts in a 640x480 window: its renderer's full
# screen fails under many Wine setups ("Couldn't set D3D Emulation mode").
#
# Environment:
#   WINEPREFIX   the Wine prefix to use (default: $HOME/.wine-nolf, created on
#                first run, so your own prefix is left alone)
#   WINE         the wine binary (default: wine)
#   WINESERVER   the wineserver binary (default: wineserver; the release
#                build restarts it once, the first time it sets up its
#                CD-ROM drive)
#   WINEDEBUG    Wine's log channels (default: fixme-all, which silences
#                DirectMusic's chatter)
#
# The launcher expects what the retail installer left behind, and this script
# sets that up inside its Wine prefix:
#   * a CD-ROM drive holding \DATA\NOLF.REZ -- its disc check. Drive N: is
#     mapped to the cd/ directory beside this script, which holds
#     DATA/NOLF.REZ as a link to nolf.rez. Not in the modern build, whose
#     launcher has no disc check (MODERN.md M-P1-15).
#   * the installer's registry values under HKLM\Software\Monolith
#     Productions\No One Lives Forever\1.0: the extra rez archives the
#     launcher adds to the command line (the update, then the GOTY content
#     and -- ours -- the custom/ directory holding the game DLLs built from
#     source), and movies off (on in the modern build, whose own Bink
#     player plays them).
# On the first run autoexec.cfg -- the game's settings, rewritten on every
# exit -- starts as a copy of defaults.cfg.
#
# Sound: the game uses the Miles Sound System (mss32.dll) through
# DirectSound, so it needs Wine's audio working on this machine (winecfg >
# Audio). The music is DirectMusic, which Wine only partly implements: its
# own cannot load the music's styles. The modern build has Microsoft's
# DirectMusic in this directory (ms_*.dll and lithtech.exe.manifest) and
# needs nothing more. For the release build there is no music until
# Microsoft's DirectMusic is installed in the prefix (not winetricks'
# `directmusic`, which also replaces Wine's DirectSound, and the sound
# effects repeat):
#   WINEPREFIX=~/.wine-nolf winetricks dmband dmcompos dmime dmloader \
#       dmscript dmstyle dmsynth dmusic dmusic32 dswave gmdls
set -e
cd "$(dirname "$0")"
case "${1:-}" in
--help|-h) sed -n '2,/^set -e$/{/^set -e$/d;p}' "./${0##*/}" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac
HERE="$(pwd)"
WINEPREFIX="${WINEPREFIX:-$HOME/.wine-nolf}"
WINEDEBUG="${WINEDEBUG:-fixme-all}"
export WINEPREFIX WINEDEBUG
WINE="${WINE:-wine}"
command -v "$WINE" >/dev/null 2>&1 || { echo "linux-run.sh: '$WINE' not found -- install Wine (32-bit support needed)" >&2; exit 1; }

# The modern build: its own Bink player (MANIFEST.txt lists it as EXT).
MODERN=0
grep -q '^EXT .*	binkw32\.dll$' MANIFEST.txt 2>/dev/null && MODERN=1

[ -f autoexec.cfg ] || cp defaults.cfg autoexec.cfg
mkdir -p Save

# a prefix to set up: make it now if it does not exist yet
[ -d "$WINEPREFIX/dosdevices" ] || "$WINE" wineboot -i >/dev/null 2>&1 || true
if [ $MODERN = 0 ]; then
    mkdir -p cd/DATA
    ln -sf ../../nolf.rez cd/DATA/NOLF.REZ
    ln -sfn "$HERE/cd" "$WINEPREFIX/dosdevices/n:"
fi

KEY='HKLM\Software\Monolith Productions\No One Lives Forever\1.0'
# Both registry views: in a 64-bit prefix `wine reg` writes the 64-bit one,
# while NOLF.exe, a 32-bit program, reads HKLM\Software through the 32-bit
# one (Wow6432Node) -- written only there, it would find no update or GOTY
# archives and leave nolfu003.rez and nolfgoty.rez off the command line.
# (A 32-bit prefix has one view; the /reg:64 write then fails, harmlessly.)
reg() { "$WINE" reg add "$@" /reg:32 /f >/dev/null 2>&1
        "$WINE" reg add "$@" /reg:64 /f >/dev/null 2>&1 || true; }
# The release build's movies: off, as RAD's binkw32.dll misrenders or
# stalls under many Wine setups (the modern build has its own player). The
# release build's window: see the header.
MOVIES_OFF=1
MODE="++windowed 1 ++screenwidth 640 ++screenheight 480 ++bitdepth 16"
if [ $MODERN = 1 ]; then
    MODE=""
fi
# (The modern launcher reads none of this: it keeps its settings in
# launcher.ini, and finds the archives itself.)
if [ $MODERN = 0 ]; then
    reg "$KEY" /v 'Update Command Line'  /t REG_SZ /d '-rez nolfu003.rez'
    reg "$KEY" /v 'Content Command Line' /t REG_SZ /d '-rez nolfgoty.rez -rez custom'
    reg "$KEY" /v 'Disable Movies'       /t REG_DWORD /d $MOVIES_OFF
fi
# The release build's CD-ROM drive (n:, see above). Wine reads the drive
# types when its server starts, so the first time the drive is set the
# server is restarted, or that run would still find no CD-ROM. Only then:
# a restart ends every program in the prefix, a dedicated server started
# with linux-server.sh among them.
if [ $MODERN = 0 ] && ! "$WINE" reg query 'HKLM\Software\Wine\Drives' /v 'n:' 2>/dev/null | grep -qi cdrom; then
    reg 'HKLM\Software\Wine\Drives' /v 'n:' /t REG_SZ /d cdrom
    "${WINESERVER:-wineserver}" -k >/dev/null 2>&1 || true
    sleep 1
fi

exec "$WINE" NOLF.exe $MODE "$@"
