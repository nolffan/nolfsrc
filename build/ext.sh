#!/usr/bin/env bash
# The modern build's open-source middleware, built from the submodules in
# ext/ into out/ext/ (build/all.sh step 2 runs it when NOLF_MIDDLEWARE=open,
# which build/variant.sh sets for modern and modern-debug):
#
#   out/ext/mss32.dll     OpenMiles (ext/openmiles), with the Miles 6.0 export
#                         table, in place of RAD's Miles Sound System 6.0i
#   out/ext/binkw32.dll   our Bink player (src/modern/binkw32/) on FFmpeg's
#                         Bink decoders (ext/ffmpeg, static, only what Bink
#                         needs), in place of RAD's Bink 1.0h
#   out/ext/opengl.ren    our OpenGL renderer (src/modern/render/), installed
#                         beside d3d.ren; "RenderDll" "opengl.ren" picks it
#   out/ext/d3d9.ren      our Direct3D 9 renderer, the same core over its
#                         Direct3D 9 backend; "RenderDll" "d3d9.ren" picks it
#
# Everything is compiled with Zig (the build image has the version
# ext/openmiles/build.zig.zon asks for): OpenMiles is Zig, and `zig cc` is
# clang with its own mingw-w64 headers and libraries, cross-compiling for
# 32-bit Windows. Nothing is written into ext/ (OpenMiles is built from a
# patched copy, see below). Each piece is rebuilt only
# when its inputs change (stamps in out/ext/).
#
#   build/ext.sh          (inside the build container; build/all.sh calls it)
set -e
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"
OUT="${NOLF_EXT_OUT:-$REPO/out/ext}"
TARGET=x86-windows-gnu
export ZIG_GLOBAL_CACHE_DIR="$OUT/zig-cache" ZIG_LOCAL_CACHE_DIR="$OUT/zig-cache"

for d in ext/openmiles ext/ffmpeg; do
    if [ ! -f "$d/LICENSE" ] && [ ! -f "$d/LICENSE.md" ]; then
        echo "error: $d is empty -- run: git submodule update --init $d" >&2
        exit 1
    fi
done
if ! command -v zig >/dev/null 2>&1; then
    echo "error: no zig here -- run this in the build container (make shell)" >&2
    exit 1
fi
want="$(sed -n 's/^ *\.minimum_zig_version *= *"\([^"]*\)".*/\1/p' ext/openmiles/build.zig.zon)"
if [ "$(zig version)" != "$want" ]; then
    echo "error: ext/openmiles needs zig $want, the image has $(zig version) (docker/Dockerfile)" >&2
    exit 1
fi
mkdir -p "$OUT"

# stamp <name> <inputs...>: true (and nothing to do) when <name>'s inputs
# are what they were when it was last built
# (a submodule counts by its checked-out commit, read without git, which
# the image does not have)
gitrev() { local g="$1/.git" h
           [ -f "$g" ] && g="$1/$(sed -n 's/^gitdir: //p' "$g")"
           h="$(cat "$g/HEAD")"
           case "$h" in "ref: "*) cat "$g/${h#ref: }" ;; *) echo "$h" ;; esac; }
inputs() { for f in "$@"; do
               if [ -e "$f/.git" ]; then echo "$f $(gitrev "$f")"
               else sha256sum "$f"; fi
           done; zig version; }
fresh()  { local s="$OUT/.$1.stamp"; shift
           [ -f "$s" ] && [ "$(inputs "$@" | sha256sum)" = "$(cat "$s")" ]; }
stamp()  { local s="$OUT/.$1.stamp"; shift; inputs "$@" | sha256sum > "$s"; }

# --- OpenMiles -> out/ext/mss32.dll ------------------------------------------
# Built from a copy (out/ext/openmiles-src/). ext/openmiles is our fork's
# miles6-fixes branch, upstream's release with our fixes as commits; any
# ext/patches/openmiles/*.patch would be applied to the copy, in order (none
# today).
PATCHES=$(ls ext/patches/openmiles/*.patch 2>/dev/null || true)
if fresh openmiles ext/openmiles build/ext.sh $PATCHES; then
    echo "== OpenMiles: up to date ($OUT/mss32.dll)"
else
    echo "== OpenMiles: ext/openmiles -> $OUT/mss32.dll (Miles 6.0 exports, as NOLF's 6.0i)"
    rm -rf "$OUT/openmiles" "$OUT/openmiles-src"
    mkdir -p "$OUT/openmiles-src"
    tar -C ext/openmiles --exclude=.git -cf - . | tar -C "$OUT/openmiles-src" -xf -
    for p in $PATCHES; do
        echo "   patch: $p"
        patch -s -p1 -d "$OUT/openmiles-src" < "$p"
    done
    ( cd "$OUT/openmiles-src" &&
      zig build -Dtarget=x86-windows -Doptimize=ReleaseFast -Dmss-version=6.0 \
          --prefix "$OUT/openmiles" --cache-dir "$OUT/zig-cache/openmiles" )
    dll="$(find "$OUT/openmiles" -iname 'mss32.dll' | head -1)"
    [ -n "$dll" ] || { echo "error: OpenMiles built no mss32.dll" >&2; exit 1; }
    cp "$dll" "$OUT/mss32.dll"
    stamp openmiles ext/openmiles build/ext.sh $PATCHES
fi

# --- FFmpeg -> out/ext/ffmpeg/ (static libraries) -----------------------------
if fresh ffmpeg ext/ffmpeg build/ext.sh; then
    echo "== FFmpeg: up to date ($OUT/ffmpeg)"
else
    echo "== FFmpeg: ext/ffmpeg -> $OUT/ffmpeg (Bink decoders only, static)"
    rm -rf "$OUT/ffmpeg-build" "$OUT/ffmpeg"
    mkdir -p "$OUT/ffmpeg-build"
    ( cd "$OUT/ffmpeg-build" &&
      "$REPO/ext/ffmpeg/configure" --prefix="$OUT/ffmpeg" \
          --enable-cross-compile --target-os=mingw32 --arch=x86 \
          --cc="zig cc -target $TARGET" --ar="zig ar" --ranlib="zig ranlib" \
          --nm=nm --disable-stripping \
          --disable-everything --disable-autodetect --disable-programs \
          --disable-doc --disable-network --disable-x86asm \
          --disable-avdevice --disable-avfilter --disable-swresample \
          --enable-static --disable-shared --enable-w32threads \
          --enable-decoder=bink,binkaudio_rdft,binkaudio_dct \
          --enable-demuxer=bink --enable-protocol=file \
          > configure.log 2>&1 ||
          { tail -20 configure.log; [ -f ffbuild/config.log ] && tail -40 ffbuild/config.log; exit 1; }
      make -j"$(nproc)" > make.log 2>&1 || { tail -40 make.log; exit 1; }
      make install > install.log 2>&1 || { tail -20 install.log; exit 1; } )
    stamp ffmpeg ext/ffmpeg build/ext.sh
fi

# --- our Bink player -> out/ext/binkw32.dll ----------------------------------
B=src/modern/binkw32
if fresh binkw32 ext/ffmpeg build/ext.sh "$B/binkw32.c" "$B/binkw32.def"; then
    echo "== binkw32.dll: up to date ($OUT/binkw32.dll)"
else
    echo "== binkw32.dll: $B/ + FFmpeg -> $OUT/binkw32.dll"
    zig cc -target $TARGET -O2 -s -shared -o "$OUT/binkw32.dll" \
        -I"$OUT/ffmpeg/include" "$B/binkw32.c" "$B/binkw32.def" \
        -L"$OUT/ffmpeg/lib" -lavformat -lavcodec -lswscale -lavutil \
        -lwinmm -lbcrypt
    rm -f "$OUT/binkw32.lib" "$OUT/binkw32.pdb"
    stamp binkw32 ext/ffmpeg build/ext.sh "$B/binkw32.c" "$B/binkw32.def"
fi

# --- our OpenGL renderer -> out/ext/opengl.ren --------------------------------
# The renderer core (src/modern/render/) over its OpenGL backend (gl/).
R=src/modern/render
GSRC=$(ls "$R"/*.cpp "$R"/*.h "$R"/gl/*.cpp "$R"/gl/*.def "$R"/gl/*.rc | sort)
if fresh opengl build/ext.sh $GSRC; then
    echo "== opengl.ren: up to date ($OUT/opengl.ren)"
else
    echo "== opengl.ren: $R/ + $R/gl/ -> $OUT/opengl.ren"
    # C++ without exceptions, RTTI or the C++ library: the renderer is the
    # engine's C interface, and the DLL then needs nothing but Windows' own
    # libraries (opengl32 included, which Wine maps to the host's OpenGL).
    zig c++ -target $TARGET -O2 -s -shared -std=c++17 -fno-exceptions -fno-rtti -nostdlib++ \
        -Wall -Wno-unused-function -o "$OUT/opengl.ren" \
        $(ls "$R"/*.cpp "$R"/gl/*.cpp | sort) "$R/gl/opengl.rc" "$R/gl/opengl.def" \
        -lopengl32 -lgdi32 -luser32
    rm -f "$OUT/opengl.lib" "$OUT/opengl.pdb"
    stamp opengl build/ext.sh $GSRC
fi

# --- our Direct3D 9 renderer -> out/ext/d3d9.ren ------------------------------
# The same core over its Direct3D 9 backend (d3d9/): DXVK under Proton.
DSRC=$(ls "$R"/*.cpp "$R"/*.h "$R"/d3d9/*.cpp "$R"/d3d9/*.def "$R"/d3d9/*.rc | sort)
if fresh d3d9 build/ext.sh $DSRC; then
    echo "== d3d9.ren: up to date ($OUT/d3d9.ren)"
else
    echo "== d3d9.ren: $R/ + $R/d3d9/ -> $OUT/d3d9.ren"
    zig c++ -target $TARGET -O2 -s -shared -std=c++17 -fno-exceptions -fno-rtti -nostdlib++ \
        -Wall -Wno-unused-function -o "$OUT/d3d9.ren" \
        $(ls "$R"/*.cpp "$R"/d3d9/*.cpp | sort) "$R/d3d9/d3d9.rc" "$R/d3d9/d3d9.def" \
        -ld3d9 -lgdi32 -luser32
    rm -f "$OUT/d3d9.lib" "$OUT/d3d9.pdb"
    stamp d3d9 build/ext.sh $DSRC
fi
