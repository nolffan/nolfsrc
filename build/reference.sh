#!/usr/bin/env bash
# Extract the retail engine images -- lithtech.exe, server.dll, d3d.ren,
# NOLF.exe, NolfServ.exe, ltmsg.dll -- the Game of the Year game DLLs -- CShell.dll,
# Object.lto, CRes.dll, out of NOLFGOTY.REZ -- and sres.dll, out of NOLF2.REZ --
# from the installed retail game
# (build/retail.sh) into
# out/reference/, with their function
# and vtable tables (<stem>-functions.csv, <stem>-vtables.csv), for the checks that
# compare our build against them: `build/engine.sh <target> verify`, the
# *-one loops, ext/tools/progress.py (`make progress` runs this itself) and the
# reconstruction tools. Nothing in the build reads them, and nothing in
# out/reference/ is ever run.
#
#   build/reference.sh            (in the build container, or on a host with
#                                  Python 3; no Wine involved)
set -e
cd "$(dirname "${BASH_SOURCE[0]}")/.."
source build/retail.sh
python3 build/retailextract.py reference "$(retail_dir)" out/reference

# The function table of each image (ext/tools/enumfuncs.py --csv), named by the
# image's stem in progress/targets.py (ext/tools/tgt.py): addr, size,
# how the start was found, and the name an error-tag literal or export gives
# it. The reconstruction tools read these (fndiff.py's sizes, tagmap.py,
# uncovered.py, ...); like the images, they are derived, never committed.
for pair in server:server.dll lithtech:lithtech.exe d3d:d3d.ren nolf:NOLF.exe \
            nolfserv:NolfServ.exe cshell:CShell.dll object:Object.lto; do
    python3 ext/tools/enumfuncs.py "out/reference/${pair#*:}" \
        --csv "out/reference/${pair%%:*}-functions.csv" | tail -1
done
# ... and its vtables, one row per slot (ext/tools/vtabaudit.py), which
# ext/tools/vtabcheck.py compares our linked images against.
for pair in server:server.dll lithtech:lithtech.exe d3d:d3d.ren; do
    python3 ext/tools/vtabaudit.py "out/reference/${pair#*:}" \
        --csv="out/reference/${pair%%:*}-vtables.csv" | tail -1
done
