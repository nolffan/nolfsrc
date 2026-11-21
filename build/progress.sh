#!/usr/bin/env bash
# The scoreboard: how much of the build is byte-identical to the retail game,
# held to what has already been recorded. See progress/README.md.
#
#   make progress           (on the host: runs `build/progress.sh check` in a
#                            container, with $NOLF_SOURCE)
#   make progress-update    (`build/progress.sh update`: record the run)
#   make progress-ci        (`build/progress.sh ci`: the check without the
#                            retail game)
#   build/progress.sh check|update|ci   (inside the build container)
#
# Scores what the last `make` left in src/engine/*/Release/ and
# src/lithshared/out/; it compiles nothing. Needs the installed retail game
# (build/retail.sh; its images are extracted into out/reference/ the first
# time) and, to score the
# lithshared libraries, NOLF_SOURCE: the NOLF Source Code v1.003 release.
#
#   check   ext/tools/progress.py, then ext/tools/ratchet.py check: fails if a
#           function recorded in progress/identical.txt no longer matches
#           (COMPLETE=1: also unless every function matches; COVERED=1:
#           unless nothing is unwritten or unplaced)
#   update  ext/tools/progress.py, then ext/tools/ratchet.py update: records the run
#           in progress/ and rewrites progress/README.md's table (ACCEPT_LOSSES=1 to
#           record a run that lost functions). Each check also records what
#           it compared against as hashes (--hashes), into progress/hashes/
#           when every check passes: what `ci` checks against
#   ci      the same checks without the retail game or its images, against
#           progress/hashes/ (ext/tools/hashcheck.py, `datacheck.py ci`,
#           `fpconst.py --targets --ci`, `vtabcheck.py --targets --ci`):
#           fails if a function, a resource, a constant, a string, a vtable
#           or a call recorded there is no longer what it was. It compiles
#           nothing and finds no new match (only `update` records one). A
#           build without the retail game (build/all.sh, NOLF_NODISCS=1) is
#           checked whole: its art is the committed retail art (assets/art/)
#   both    then ext/tools/datacheck.py: the text in our images' data against the
#           retail images'; fails on a difference progress/data-known.txt
#           does not list (update also drops the entries no longer needed);
#           and ext/tools/fpconst.py --targets: the float constants our placed
#           functions load through a (masked) relocation against retail's;
#           fails on any that differs; and ext/tools/vtabcheck.py --targets: our
#           vtables slot by slot, and the callees and globals our uniquely
#           placed functions reach, against retail's; fails on a
#           contradiction progress/vtab-known.txt does not list; and
#           build/moderndata.py check: assets/modern/ against the
#           src/modern/data/ recipes
#
# Before anything is scored, the cached retail images in out/reference/ are
# checked against progress/reference.sha256 (extracted again once if they
# differ, or if the tables' stamp does): a stale or altered cache would be
# scored as if it were retail.
set -e
cd "$(dirname "${BASH_SOURCE[0]}")/.."

mode="${1:-check}"
case "$mode" in
    check|update) ;;
    ci)
        rc=0
        echo "functions and resources (ext/tools/hashcheck.py, progress/hashes/):"
        python3 ext/tools/hashcheck.py check || rc=1
        echo
        python3 ext/tools/datacheck.py ci || rc=1
        echo
        echo "float constants (ext/tools/fpconst.py --ci):"
        python3 ext/tools/fpconst.py --targets --ci || rc=1
        echo
        echo "vtables and wiring (ext/tools/vtabcheck.py --ci):"
        python3 ext/tools/vtabcheck.py --targets --ci || rc=1
        echo
        if [ $rc = 0 ]; then
            echo "PASS: everything recorded in progress/hashes/ is unchanged (new matches are found only by make progress-update)"
        else
            echo "FAIL: see above"
        fi
        exit $rc ;;
    *) echo "usage: build/progress.sh check|update|ci" >&2; exit 2 ;;
esac
H=
[ "$mode" = update ] && H=--hashes
rm -rf out/progress/hashes

# the images, and the tables build/reference.sh derives from them
for f in lithtech.exe server.dll d3d.ren NOLF.exe NolfServ.exe ltmsg.dll sres.dll \
         CShell.dll Object.lto CRes.dll \
         server-functions.csv lithtech-functions.csv d3d-functions.csv \
         nolf-functions.csv nolfserv-functions.csv cshell-functions.csv object-functions.csv \
         server-vtables.csv lithtech-vtables.csv d3d-vtables.csv; do
    if [ ! -f "out/reference/$f" ]; then
        need=1
        break
    fi
done

# The cached retail images in out/reference/ are what is scored: they have
# to be the retail images progress/reference.sha256 names, every run, not
# merely present. The tables derived from them are stamped with what made
# them (that list and ext/tools/enumfuncs.py, by content): a different stamp
# extracts everything again.
refcheck() { (cd out/reference && sha256sum --quiet -c ../../progress/reference.sha256); }
stamp=$(cat progress/reference.sha256 ext/tools/enumfuncs.py | sha256sum | cut -d' ' -f1)
if [ -n "${need:-}" ] || ! refcheck 2>/dev/null || [ "$(cat out/reference/.stamp 2>/dev/null)" != "$stamp" ]; then
    echo "== retail images -> out/reference/ (for scoring only; missing, stale, or not the ones progress/reference.sha256 names)"
    bash build/reference.sh
    refcheck || { echo "error: out/reference/ still differs from progress/reference.sha256" >&2; exit 1; }
    echo "$stamp" > out/reference/.stamp
fi

python3 ext/tools/progress.py $H
echo
rc=0
if [ "$mode" = check ]; then
    python3 ext/tools/ratchet.py check ${COMPLETE:+--complete} ${COVERED:+--covered} || rc=$?
else
    python3 ext/tools/ratchet.py update ${ACCEPT_LOSSES:+--accept-losses} || rc=$?
fi
# ... and the text in the images' data against the retail text, held to the
# differences progress/data-known.txt lists (update: drops the ones that are
# gone)
echo
python3 ext/tools/datacheck.py "$mode" $H || rc=1
# ... and the float constants behind the masked relocations
echo
echo "float constants (ext/tools/fpconst.py):"
python3 ext/tools/fpconst.py --targets $H || rc=1
# ... and every vtable slot and every call and global our identically placed
# functions reach, against the shipped ones (progress/vtab-known.txt)
echo
echo "vtables and wiring (ext/tools/vtabcheck.py):"
python3 ext/tools/vtabcheck.py --targets $H || rc=1
# ... and what all of it rested on (the toolchains' digests checked), kept
# with the baseline when one is recorded
echo
python3 ext/tools/evidence.py || rc=1
[ "$mode" = update ] && cp out/progress/evidence.json progress/evidence.json
# ... and the modern build's changed retail files in assets/modern/ against
# what the src/modern/data/ recipes make (build/moderndata.py)
echo
echo "modern data (build/moderndata.py check):"
python3 build/moderndata.py check || rc=1
# ... and what the checks compared against, for `ci`, once all of them pass
if [ "$mode" = update ]; then
    if [ $rc = 0 ]; then
        mkdir -p progress/hashes
        cp out/progress/hashes/*.tsv progress/hashes/
        echo "recorded progress/hashes/ ($(du -sh progress/hashes | cut -f1)): what make progress-ci checks against"
    else
        echo "progress/hashes/ not recorded: a check failed"
    fi
fi
exit $rc
