# The build image: Ubuntu 24.04 with its distro Wine (32-bit), Python 3, and
# what the scripted playtest needs (Xvfb, xdotool, xwd, ImageMagick, Mesa),
# what ext/rez/ needs to fetch the disc images and install the game from them
# (curl; a C compiler, make and zlib for its vendored unshield), objdump
# (binutils), which the reconstruction tools disassemble with, and what the
# modern build's open-source middleware in ext/ is built with (build/ext.sh):
# Zig, pinned to the version ext/openmiles declares -- the compiler for
# OpenMiles, and (`zig cc`, clang with its own mingw-w64 headers and
# libraries) the 32-bit Windows C cross-compiler for FFmpeg and our
# binkw32.dll. And g++, CMake and Ninja for lithtools' lithrez
# (build/lithrez.sh), which packs our game DLLs and assets/ into nolfsrc.rez.
#
# The Makefile builds it as `nolfsrc-build` and runs every step in a
# throwaway container with the checkout mounted at /repo, as the invoking
# user. Nothing from the repository, the toolchain or the game is baked into
# the image.
#
# Why a container at all: Wine is the only way to run the 2000-era
# compilers, and Ubuntu 24.04's Wine 9.0 is the one this build is known to
# work with -- some newer host userspaces break every current Wine release.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive PYTHONDONTWRITEBYTECODE=1
RUN dpkg --add-architecture i386 \
 && apt-get update -qq \
 && apt-get install -y -qq \
        wine wine32:i386 \
        python3 binutils \
        xvfb xauth xdotool x11-apps imagemagick \
        libgl1:i386 libglu1-mesa:i386 libgl1-mesa-dri:i386 \
        procps \
        curl ca-certificates gcc make libc6-dev zlib1g-dev \
        g++ cmake ninja-build \
        xz-utils patch \
 && rm -rf /var/lib/apt/lists/*

ARG ZIG_VERSION=0.16.0
ARG ZIG_SHA256=70e49664a74374b48b51e6f3fdfbf437f6395d42509050588bd49abe52ba3d00
RUN curl -fsSL -o /tmp/zig.tar.xz \
        "https://ziglang.org/download/${ZIG_VERSION}/zig-x86_64-linux-${ZIG_VERSION}.tar.xz" \
 && echo "${ZIG_SHA256}  /tmp/zig.tar.xz" | sha256sum -c - \
 && mkdir /opt/zig && tar -C /opt/zig --strip-components=1 -xJf /tmp/zig.tar.xz \
 && ln -s /opt/zig/zig /usr/local/bin/zig && rm /tmp/zig.tar.xz

COPY entry.sh /usr/local/bin/nolf-entry
RUN chmod 755 /usr/local/bin/nolf-entry
ENTRYPOINT ["/usr/local/bin/nolf-entry"]
CMD ["bash"]
