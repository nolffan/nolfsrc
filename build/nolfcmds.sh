#!/usr/bin/env bash
# The game directory's nolfcmds.txt, as a fresh build writes it:
#
#   build/nolfcmds.sh <retail|open> [modern]
#
# The retail NOLF.exe launcher writes nolfcmds.txt and runs
# "lithtech.exe -cmdfile nolfcmds.txt"; build/playtest.sh does the same with this one.
# Rez order is the GOTY install's (later entries override earlier ones), and
# then ours, last, so that nothing overrides it: custom/ -- our game DLLs --
# or, in the modern build (`modern`), its edits of the discs' data
# (nolfsrcdisc.rez) and then nolfsrc.rez -- our game DLLs and assets/ (a
# mod's archives, which the launcher adds, come after it). +DisableMovies skips the Bink intro
# logos, which retail binkw32.dll misrenders or stalls on under some Wine
# setups; the modern build's own Bink player (open middleware) plays them.
# build/all.sh writes it into the game directory (the launcher rewrites it
# when it is used); the archives have none, the launcher writing its own.
MOVIES=" +DisableMovies 1"
[ "${1:-retail}" = open ] && MOVIES=""
OURS="-rez custom"
[ "${2:-}" = modern ] && OURS="-rez nolfsrcdisc.rez -rez nolfsrc.rez"
printf -- '-rez nolf.rez -rez nolf2.rez -rez nolfu003.rez -rez nolfgoty.rez %s ++screenwidth 640 ++screenheight 480 ++bitdepth 16 ++windowed 1%s\r\n' "$OURS" "$MOVIES"
