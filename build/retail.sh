# Sourced by build/all.sh, build/reference.sh and the Makefile: the retail
# game the build takes its data from -- No One Lives Forever: Game of the Year
# Edition (Europe), installed by the ext/rez/ submodule from the redump images of
# its two discs (ext/rez/README.md) -- and whether it is the right one.
#
# RETAIL_RELEASE  ext/rez/'s name for it
# retail_dir      prints ext/rez/games/nolf-eu-goty, if it is installed
# retail_install  fetches the two disc images from archive.org and installs
#                 the release from them, unless it is installed already
#                 (ext/rez/fetch-isos.sh, ext/rez/install-games.sh; `make` runs it)
# retail_verify   checks every installed file against ext/rez/games.sha256 (the
#                 Makefile runs it before every build; NOLF_SKIP_RETAIL_CHECK=1
#                 skips it)
RETAIL_RELEASE=nolf-eu-goty

retail_rez() {
    local here
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    if [ ! -f "$here/ext/rez/install-games.sh" ]; then
        echo "error: ext/rez/ is empty -- the disc images and their installer are the ext/rez/" >&2
        echo "       submodule: run git submodule update --init (see ext/rez/README.md)" >&2
        return 1
    fi
    echo "$here/ext/rez"
}

retail_dir() {
    local rez d
    rez="$(retail_rez)" || return 1
    d="$rez/games/$RETAIL_RELEASE"
    if [ -f "$d/MANIFEST.txt" ] && [ -f "$d/lithtech.exe" ]; then
        echo "$d"; return 0
    fi
    echo "error: the retail game is not installed in $d" >&2
    echo "       run make (or, in make shell, build/retail.sh's retail_install)" >&2
    return 1
}

retail_install() {
    local rez
    rez="$(retail_rez)" || return 1
    [ -f "$rez/games/$RETAIL_RELEASE/MANIFEST.txt" ] && return 0
    echo "== fetching and installing $RETAIL_RELEASE (ext/rez/: about 0.8 GB from archive.org, once)"
    "$rez/fetch-isos.sh" "$RETAIL_RELEASE" && "$rez/install-games.sh" "$RETAIL_RELEASE"
}

retail_verify() {
    local rez d
    rez="$(retail_rez)" || return 1
    d="$(retail_dir)" || return 1
    [ "${NOLF_SKIP_RETAIL_CHECK:-0}" = 1 ] && return 0
    # a stamp skips re-hashing 1.1 GB on every build while the files are unchanged
    local stamp key
    stamp="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/out/.retail-verified"
    key="$(cd "$d" && find . -type f -printf '%p %s %T@\n' | sort | sha256sum | cut -d' ' -f1)"
    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$key" ]; then
        return 0
    fi
    echo "== checking $d against ext/rez/games.sha256"
    if ! python3 -I "$rez/tools/install.py" --verify "$rez/games" "$RETAIL_RELEASE"; then
        echo "error: the installed $RETAIL_RELEASE does not match ext/rez/games.sha256." >&2
        echo "       Reinstall it: ext/rez/install-games.sh --force $RETAIL_RELEASE" >&2
        return 1
    fi
    mkdir -p "$(dirname "$stamp")" && echo "$key" > "$stamp"
}
