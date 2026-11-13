#!/bin/sh
# linux-server.sh -- run a No One Lives Forever dedicated server (NolfServ.exe,
# built from source) with Wine.
#
#   ./linux-server.sh [options] [NolfServ.exe args...]
#
# Options:
#   --wizard              show the setup wizard (the default when there is no
#                         NetHost.txt yet: the wizard writes it)
#   --headless            run without a display: under xvfb-run, a virtual X
#                         server (the default when $DISPLAY is not set)
#   --help                this text
# Without --wizard the server starts at once with the settings the wizard
# last saved in NetHost.txt (-nowiz). Anything else is handed to NolfServ.exe:
# each saved setting can be given as `-<setting> <value>`, for example
#   ./linux-server.sh -NetSessionName "My server" -NetPort 27888 -NetMaxPlayers 8
# (the settings are listed in docs/INSTALL.md, "A dedicated server").
#
# Environment:
#   WINEPREFIX   the Wine prefix to use (default: $HOME/.wine-nolf, the one
#                linux-run.sh uses; $HOME/.wine-nolf-server when headless --
#                a prefix's Wine desktop lives on one display, and a game
#                started in a prefix whose desktop is on the server's
#                virtual display crashes as it confines the mouse)
#   WINE         the wine binary (default: wine)
#   WINEDEBUG    Wine's log channels (default: -all)
#
# The server runs in this directory: it loads the rez archives startup.txt
# lists (custom/ or, in the modern build, nolfsrc.rez -- the game DLLs --
# last), keeps its settings in
# NetHost.txt and, in the modern build, logs to modern.log. It answers on
# UDP port 27888 (or -NetPort) -- open it in a firewall for players on other
# machines. It runs until its window is closed (Stop Server, or the window's
# close box) or the process is ended.
set -e
cd "$(dirname "$0")"

WIZARD=
HEADLESS=
case "${DISPLAY:-}" in "") HEADLESS=1 ;; esac
[ -f NetHost.txt ] || WIZARD=1
while [ $# -gt 0 ]; do
    case "$1" in
        --wizard)   WIZARD=1 ;;
        --headless) HEADLESS=1 ;;
        --help|-h)  sed -n '2,/^set -e$/{/^set -e$/d;p}' "./${0##*/}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)          break ;;
    esac
    shift
done

if [ -n "$HEADLESS" ]; then
    WINEPREFIX="${WINEPREFIX:-$HOME/.wine-nolf-server}"
else
    WINEPREFIX="${WINEPREFIX:-$HOME/.wine-nolf}"
fi
WINEDEBUG="${WINEDEBUG:--all}"
export WINEPREFIX WINEDEBUG
WINE="${WINE:-wine}"
command -v "$WINE" >/dev/null 2>&1 || { echo "linux-server.sh: '$WINE' not found -- install Wine (32-bit support needed)" >&2; exit 1; }

if [ -n "$WIZARD" ] && [ -n "$HEADLESS" ] && [ -z "${DISPLAY:-}" ]; then
    echo "linux-server.sh: the setup wizard needs a display -- run it once where there is one, or start without it" >&2
    echo "                 (the settings then are NolfServ.exe's defaults and the command line's)" >&2
    WIZARD=
fi

# -nowiz is read only when another argument follows it (the retail parser
# expects a value after every option: RETAIL_BUGS.md), so a second one ends
# the line.
if [ -z "$WIZARD" ]; then
    set -- -nowiz "$@" -nowiz
fi

if [ -n "$HEADLESS" ]; then
    command -v xvfb-run >/dev/null 2>&1 || { echo "linux-server.sh: no display, and no xvfb-run (install xvfb) to make one" >&2; exit 1; }
    exec xvfb-run -a "$WINE" NolfServ.exe "$@"
fi
exec "$WINE" NolfServ.exe "$@"
