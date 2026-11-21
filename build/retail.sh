# Sourced by build/all.sh, build/reference.sh and the Makefile: the retail
# game the build takes its data from -- No One Lives Forever: Game of the Year
# Edition (Europe), installed from your own images of its two discs by
# build/discs.py -- and whether it is the right one.
#
# RETAIL_RELEASE  the name of the install directory
# NOLF_RETAIL     where it is installed: out/retail/nolf-eu-goty by default
#                 (build/variant.sh sets it to the checkout's, for its tree)
# retail_dir      prints out/retail/nolf-eu-goty, if it is installed
# retail_install  installs the release from the disc images in isos/ (or
#                 $NOLF_ISOS), unless it is installed already from the same
#                 build/retail-files.txt (`make retail` runs it; every build
#                 does)
# retail_verify   checks every installed file against build/retail-files.txt
#                 (the Makefile runs it before every build;
#                 NOLF_SKIP_RETAIL_CHECK=1 skips it)
RETAIL_RELEASE=nolf-eu-goty

retail_repo() { (cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd); }

retail_root() { echo "${NOLF_RETAIL:-$(retail_repo)/out/retail/$RETAIL_RELEASE}"; }

retail_dir() {
    local d
    d="$(retail_root)"
    if [ -f "$d/.retail-files" ] && [ -f "$d/lithtech.exe" ]; then
        echo "$d"; return 0
    fi
    echo "error: the retail game is not installed in $d" >&2
    echo "       run make retail (it needs your two disc images in isos/: see docs/BUILDING.md)" >&2
    return 1
}

retail_install() {
    local d list
    d="$(retail_root)"
    list="$(retail_repo)/build/retail-files.txt"
    if [ -f "$d/.retail-files" ] && cmp -s "$d/.retail-files" "$list"; then
        return 0
    fi
    echo "== installing $RETAIL_RELEASE from your disc images (build/discs.py, once)"
    python3 "$(retail_repo)/build/discs.py" install "$d"
}

retail_verify() {
    local d
    d="$(retail_dir)" || return 1
    [ "${NOLF_SKIP_RETAIL_CHECK:-0}" = 1 ] && return 0
    # a stamp skips re-hashing 1.1 GB on every build while the files are unchanged
    local stamp key
    stamp="$(retail_repo)/out/.retail-verified"
    key="$( { cat "$(retail_repo)/build/retail-files.txt";
              cd "$d" && find . -type f -printf '%p %s %T@\n' | sort; } | sha256sum | cut -d' ' -f1)"
    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$key" ]; then
        return 0
    fi
    echo "== checking $d against build/retail-files.txt"
    if ! python3 "$(retail_repo)/build/discs.py" verify "$d"; then
        echo "error: the installed $RETAIL_RELEASE does not match build/retail-files.txt." >&2
        echo "       Install it again: rm -rf $d && make retail" >&2
        return 1
    fi
    mkdir -p "$(dirname "$stamp")" && echo "$key" > "$stamp"
}
