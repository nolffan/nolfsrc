#!/usr/bin/env bash
# The game directory as one file, to copy to the machine that plays it:
#
#   build/package.sh out/nolf-<legacy|modern>-<release|debug>
#
# writes two archives, each unpacking to <name>/ (then <name>/linux-run.sh --
# see its header; Wine needed there):
#
#   out/<name>-private.tar.gz   the whole directory, the retail game's data
#                               included: for your own machines, and anyone
#                               developing on the build
#   out/<name>-public.tar.gz    the same without anything that came from the
#                               discs, which its linux-install.sh /
#                               windows-install.bat / mac-install.command
#                               copy out of the player's
#                               own disc images (build/mkinstall.py): the one
#                               to give to other people. Its SHA256SUMS lists
#                               every file of the archive and every file the
#                               scripts write; they refuse a directory that is
#                               not the archive unpacked. It needs the images
#                               in rez/isos/; without them it is skipped.
#
# NOLF_ARCHIVES (default "private public") says which to write; with
# "public" alone (the CI build, .github/workflows/public-archives.yml) the
# private one is not made, and no disc images is an error, not a skip.
#
# build/all.sh runs it for the release build (step 6) and build/variant.sh
# for each variant, once its data and symbols are in place.
#
# Links are stored as the files they point to (-h). Each archive is a fresh
# install, whatever has been played in the directory: no saves (an empty
# Save/), no logs, no dedicated-server settings (NetHost.txt, which
# NolfServ.exe writes), autoexec.cfg as the shipped defaults.cfg, no
# nolfcmds.txt (the launcher writes it on every Launch, before the engine
# reads it; the game directory's copy is for build/playtest.sh), and none of
# the per-machine state linux-run.sh creates (cd/; run-cmds.txt, which the
# script before it wrote). Each is written beside and renamed into
# place, so a failed run never leaves a truncated one; the <name>.tar.gz and
# plain .tar older builds made are removed.
set -e
OUT="${1:?usage: build/package.sh <game directory>}"
OUT="${OUT%/}"
NAME="$(basename "$OUT")"
PARENT="$(dirname "$OUT")"
PKG="$PARENT/$NAME-private.tar.gz"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCHIVES=" ${NOLF_ARCHIVES:-private public} "

# The fresh files, staged beside the directory and renamed into it.
STAGE=".pkg-$NAME"
rm -rf "$PARENT/$STAGE"
mkdir -p "$PARENT/$STAGE"
trap 'rm -rf "$PARENT/$STAGE"' EXIT
cp "$OUT/defaults.cfg" "$PARENT/$STAGE/autoexec.cfg"

rm -f "$PARENT/$NAME.tar" "$PARENT/$NAME.tar.gz"
if [[ $ARCHIVES == *" private "* ]]; then
    tar -C "$PARENT" -h -cf - \
        --exclude="$NAME/Save/*" --exclude="$NAME/autoexec.cfg" --exclude="$NAME/nolfcmds.txt" \
        --exclude="$NAME/NetHost.txt" \
        --exclude='*.log' --exclude=run-cmds.txt --exclude=cd --exclude='*.part' \
        --transform="s|^$STAGE/|$NAME/|" \
        "$NAME" "$STAGE/autoexec.cfg" |
        gzip -6 > "$PKG.part"
    mv "$PKG.part" "$PKG"
    echo "   $PKG: $(du -h "$PKG" | cut -f1) (unpacks to $NAME/)"
fi
[[ $ARCHIVES == *" public "* ]] || exit 0
# Nor of a game directory with the PS2 levels (make modern PS2=1): they are
# made from the PS2 disc, which the install scripts do not read.
if [ -f "$OUT/custom/PS2-DATA.txt" ]; then
    echo "   $PARENT/$NAME-public.tar.gz: skipped (the game directory has the PS2 levels, custom/PS2-DATA.txt)"
    rm -f "$PARENT/$NAME-public.tar.gz"
    exit 0
fi

# The public archive: the scripts beside the rest, the disc files left out.
PUB="$PARENT/$NAME-public.tar.gz"
rc=0
python3 "$HERE/mkinstall.py" "$OUT" "$PARENT/$STAGE" || rc=$?
if [ $rc = 3 ] && [ -n "${NOLF_ARCHIVES:-}" ]; then
    echo "   $PUB: no disc images, but NOLF_ARCHIVES asks for it" >&2
    exit 1
elif [ $rc = 3 ]; then
    echo "   $PUB: skipped (no disc images)"
    rm -f "$PUB"
elif [ $rc != 0 ]; then
    exit $rc
else
    sed "s|^|$NAME/|" "$PARENT/$STAGE/disc-files.txt" > "$PARENT/$STAGE/exclude.txt"
    tar -C "$PARENT" -h -cf "$PARENT/$STAGE/public.tar" \
        --exclude="$NAME/Save/*" --exclude="$NAME/nolfcmds.txt" --exclude="$NAME/NetHost.txt" \
        --exclude='*.log' --exclude=run-cmds.txt --exclude=cd --exclude='*.part' \
        --anchored --no-wildcards --exclude-from="$PARENT/$STAGE/exclude.txt" \
        --transform="s|^$STAGE/|$NAME/|" \
        "$NAME" "$STAGE/linux-install.sh" "$STAGE/windows-install.bat" \
        "$STAGE/mac-install.command"
    # Last, SHA256SUMS: every file of the archive and every file the install
    # scripts write, but autoexec.cfg, which the game rewrites. The scripts
    # check the directory against it before they install, and
    # `sha256sum -c SHA256SUMS` checks an install.
    python3 - "$PARENT/$STAGE/public.tar" "$PARENT/$STAGE/disc-sums.txt" \
        > "$PARENT/$STAGE/SHA256SUMS" <<'PY'
import hashlib, sys, tarfile
lines = []
with tarfile.open(sys.argv[1]) as t:
    for m in t:
        rel = m.name.split("/", 1)[1] if "/" in m.name else ""
        if m.isfile() and rel:
            lines.append(f"{hashlib.sha256(t.extractfile(m).read()).hexdigest()}  {rel}")
lines += open(sys.argv[2]).read().splitlines()
names = [l[66:] for l in lines]
if len(set(n.lower() for n in names)) != len(names):
    sys.exit("package.sh: a file twice in SHA256SUMS")
for l in sorted(lines, key=lambda l: l[66:].lower()):
    sys.stdout.write(l + "\n")
PY
    tar -C "$PARENT" -rf "$PARENT/$STAGE/public.tar" --transform="s|^$STAGE/|$NAME/|" "$STAGE/SHA256SUMS"
    gzip -6 < "$PARENT/$STAGE/public.tar" > "$PUB.part"
    mv "$PUB.part" "$PUB"
    echo "   $PUB: $(du -h "$PUB" | cut -f1) (unpacks to $NAME/; its data from the discs)"
fi
