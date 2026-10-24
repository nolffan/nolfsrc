# No One Lives Forever, built from source.
#
#   make          build the whole game into out/nolf-legacy-release/ (and its -private / -public .tar.gz)
#   make test     scripted playtest of out/nolf-legacy-release/ on two levels
#   make test-modern  the same playtest of out/nolf-modern-release/
#   make test-compare both, and each level's shots side by side, legacy | modern
#   make test-renderers  scenes of out/nolf-modern-release/ with each renderer, beside d3d7.ren's
#   make debug    the same game with symbols and debug logging, in out/nolf-legacy-debug/
#   make modern   the game with the retail bugs fixed (NOLFSRC_MODERN), in out/nolf-modern-release/
#   make progress how much of the build is byte-identical to the retail game;
#                 fails if a function recorded as identical stops matching
#   make help     everything else
#
# Needs only docker on the host and the submodules (`git submodule update
# --init`): tools/, the build's helpers; the compilers, toolchains/
# (toolchains/README.md); and rez/, which
# fetches the redump images of the Game of the Year discs from archive.org and
# installs the game from them (rez/README.md; `make retail`, which every build
# runs first, does it once). Every step runs in a throwaway container of the
# image docker/Dockerfile describes, with this checkout mounted at /repo, as
# you.

IMAGE     ?= nolfsrc-build
CONTAINER ?= nolfsrc-build
TEST_LEVELS ?= T01S01 M03S01
# The playtests' screen mode, e.g. 1920x1080 (empty: the game directory's own)
TEST_RES ?=
# make test-renderers: <level>:<script> scenes (build/scenes/), and the
# renderers, the first the reference the others are put beside
TEST_SCENES ?= T01S02:build/scenes/T01S02-bottle.txt
TEST_RENDERERS ?= d3d7.ren d3d9.ren opengl.ren
# Parallel compile jobs inside the build (build/build.mk); empty: one per CPU
JOBS ?=

# The NOLF Source Code v1.003 release, for scoring the lithshared libraries
# against its prebuilt .libs (progress/README.md). Optional.
NOLF_SOURCE ?=
SRC_MOUNT := $(if $(NOLF_SOURCE),-v $(realpath $(NOLF_SOURCE)):/nolfsource:ro -e NOLF_SOURCE=/nolfsource)

DOCKER_RUN = docker run --rm --init --name $(CONTAINER) \
	-u $(shell id -u):$(shell id -g) -e HOME=/tmp/home $(if $(JOBS),-e NOLFSRC_JOBS=$(JOBS)) \
	$(if $(NOLF_ARCHIVES),-e NOLF_ARCHIVES="$(NOLF_ARCHIVES)") \
	$(if $(PS2),-e NOLFSRC_PS2=1) \
	-v $(CURDIR):/repo -w /repo $(SRC_MOUNT) $(IMAGE)

TOOLCHAIN_FILES = toolchains/msvc-6.00-9782/VC98/Bin/CL.EXE \
	toolchains/msvc-6.00-8168/BIN/cl.exe \
	toolchains/msvc-6.00-8447/BIN/cl.exe \
	toolchains/icl-4.0/BIN/icl.exe

.PHONY: all game debug modern modern-debug image retail check-rez check-tools check-ext check-toolchain test test-modern test-compare test-renderers progress progress-update shell clean distclean help

all: game

help:
	@echo "make            build everything into out/nolf-legacy-release/ and its .tar.gz archives"
	@echo "make debug      the game with symbols and NOLFSRC_DEBUG logging, in out/nolf-legacy-debug/"
	@echo "make modern     the game with NOLFSRC_MODERN (the retail bugs fixed), in out/nolf-modern-release/"
	@echo "make modern-debug  both, in out/nolf-modern-debug/"
	@echo "                (with PS2=1, the modern ones also get the PS2 release's three"
	@echo "                \"Nine years ago\" levels, from the PS2 disc in rez/isos/ps2/; no public archive)"
	@echo "                (none of these is ever scored; build/variant.sh)"
	@echo "make test       scripted playtest of out/nolf-legacy-release/ ($(TEST_LEVELS)), under Xvfb"
	@echo "make test-modern  the same of out/nolf-modern-release/"
	@echo "make test-compare both, then out/play/compare-test-<level>-test-modern-<level>.jpg:"
	@echo "                each shot, legacy left and modern right (TEST_RES=1920x1080 for a wide screen)"
	@echo "make test-renderers  the scenes of TEST_SCENES in out/nolf-modern-release/ with each of"
	@echo "                TEST_RENDERERS, then out/play/compare-<scene>-d3d7-<scene>-<renderer>.jpg"
	@echo "make progress   score the build against the retail binaries; fails if a"
	@echo "                function recorded in progress/identical.txt stops matching"
	@echo "make progress-update  record the score in progress/ and README.md"
	@echo "make retail     fetch the two disc images and install the retail game from"
	@echo "                them (rez/, once; every build runs it), and check it"
	@echo "make shell      a shell in the build container (build/*.sh, tools/ work there)"
	@echo "make image      (re)build the docker image $(IMAGE)"
	@echo "make clean      remove out/ and every build output in the source tree"
	@echo "make distclean  clean, and remove the docker image too"
	@echo
	@echo "Variables: TEST_LEVELS=\"$(TEST_LEVELS)\", TEST_RES=<WxH>, JOBS=<n> (parallel compiles; default one per CPU)"
	@echo "           NOLF_ARCHIVES=\"private public\" (the archives build/package.sh writes)"
	@echo "           NOLF_SOURCE=<dir> (the NOLF Source Code v1.003 release, for"
	@echo "           make progress), COMPLETE=1, COVERED=1, ACCEPT_LOSSES=1 (progress/README.md)"

game: check-toolchain retail
	$(DOCKER_RUN) bash build/all.sh

debug: check-toolchain retail
	$(DOCKER_RUN) bash build/variant.sh $@

# the modern build also builds OpenMiles and its Bink player from ext/
# (build/ext.sh)
modern modern-debug: check-toolchain check-ext retail
	$(DOCKER_RUN) bash build/variant.sh $@

# IMAGE_BUILD=0: use $(IMAGE) as it is (CI builds it with a layer cache
# first, .github/workflows/public-archives.yml).
IMAGE_BUILD ?= 1
image:
ifeq ($(IMAGE_BUILD),0)
	@docker image inspect $(IMAGE) >/dev/null 2>&1 || \
	  { echo "error: IMAGE_BUILD=0, but there is no docker image $(IMAGE)" >&2; exit 1; }
else
	@docker build -q -t $(IMAGE) docker/ >/dev/null || \
	  { echo "error: could not build the docker image (docker/Dockerfile)" >&2; exit 1; }
endif
	@echo "== docker image $(IMAGE) ready"

# The retail game the build takes its data from (build/retail.sh): installed
# by rez/ the first time, checked against rez/games.sha256 every time.
retail: check-rez check-tools image
	@$(DOCKER_RUN) bash -c 'source build/retail.sh && retail_install && retail_verify'

check-rez:
	@[ -f rez/install-games.sh ] || { echo "error: rez/ is empty -- the disc images and their installer are the rez/ submodule: run git submodule update --init (see rez/README.md)" >&2; exit 1; }

check-tools:
	@[ -f tools/progress.py ] || { echo "error: tools/ is empty -- the build's helpers are the tools/ submodule: run git submodule update --init (see tools/README.md)" >&2; exit 1; }

check-ext:
	@for d in ext/openmiles ext/ffmpeg; do \
	  [ -f $$d/LICENSE ] || [ -f $$d/LICENSE.md ] || { echo "error: $$d is empty -- the modern build's middleware is the ext/ submodules: run git submodule update --init" >&2; exit 1; }; \
	done
	@[ -f ext/nolf1-modernizer/ASSETS/CHARS/MODELS/INTHERO_CASUAL.abc ] || { echo "error: ext/nolf1-modernizer is empty -- the modern build takes two menu models from the NOLF Modernizer, the ext/ submodule: run git submodule update --init" >&2; exit 1; }

check-toolchain:
	@for f in $(TOOLCHAIN_FILES); do \
	  [ -f "$$f" ] || { echo "error: $$f is missing -- the compilers are the toolchains/ submodule: run git submodule update --init (see toolchains/README.md)" >&2; exit 1; }; \
	done

# $(call playtest,<game directory>,<run name prefix>): the scripted playtest
# on every level of TEST_LEVELS, its runs in out/play/<prefix><level>/
define playtest
	@[ -f $(1)/lithtech.exe ] || { echo "error: no $(1)/ -- build it first" >&2; exit 1; }
	@rc=0; for w in $(TEST_LEVELS); do \
	  echo "== playtest: $(1) Worlds\\$$w (tools/playtest-basic.txt)"; \
	  $(DOCKER_RUN) env PLAYTEST_SCRIPT=tools/playtest-basic.txt PLAYTEST_NAME=$(2)$$w \
	    $(if $(TEST_RES),PLAYTEST_RES=$(TEST_RES)) \
	    bash build/playtest.sh $(1) "Worlds\\$$w" || rc=1; \
	done; \
	if [ $$rc = 0 ]; then echo "== every level ALIVE (shots in out/play/$(2)*/)"; \
	else echo "== FAILED (see out/play/$(2)*/result.txt, wine.log)"; fi; exit $$rc
endef

test: check-tools image
	$(call playtest,out/nolf-legacy-release,test-)

test-modern: check-tools image
	$(call playtest,out/nolf-modern-release,test-modern-)

# The two one after the other (their containers share a name), then the
# shots side by side, 640x480 each (the contact sheet's are 320x240)
test-compare: check-tools image
	@rc=0; $(MAKE) --no-print-directory test || rc=1; \
	$(MAKE) --no-print-directory test-modern || rc=1; \
	for w in $(TEST_LEVELS); do \
	  echo "== compare: Worlds\\$$w, legacy | modern"; \
	  $(DOCKER_RUN) env PLAYTEST_COMPARE_SIZE=640x480 PLAYTEST_COMPARE_EXT=jpg bash build/playtest.sh compare test-$$w test-modern-$$w; \
	done; echo "== side by side: out/play/compare-test-*-test-modern-*.jpg"; exit $$rc

# Each scene (TEST_SCENES) of the modern build with each renderer, then the
# first renderer's shots beside each other's: the same build, so the same
# timing, and d3d7.ren (DirectX 7) is the reference d3d9.ren and opengl.ren
# are held to. A renderer the game does not load (it falls back to its
# default, and saves that in autoexec.cfg) fails the run
test-renderers: check-tools image
	@[ -f out/nolf-modern-release/lithtech.exe ] || { echo "error: no out/nolf-modern-release/ -- run make modern first" >&2; exit 1; }
	@mkdir -p out/play; rc=0; for sc in $(TEST_SCENES); do \
	  w=$${sc%%:*}; s=$${sc#*:}; n=$$(basename $$s .txt); ref=; \
	  for r in $(TEST_RENDERERS); do \
	    rn=$${r%.ren}; [ -n "$$ref" ] || ref=$$rn; \
	    echo "\"RenderDll\" \"$$r\"" > out/play/renderer-$$rn.cfg; \
	    echo "== scene: $$n (Worlds\\$$w) with $$r"; \
	    $(DOCKER_RUN) env PLAYTEST_SCRIPT=$$s PLAYTEST_NAME=$$n-$$rn PLAYTEST_CFG=out/play/renderer-$$rn.cfg \
	      $(if $(TEST_RES),PLAYTEST_RES=$(TEST_RES)) \
	      bash build/playtest.sh out/nolf-modern-release "Worlds\\$$w" || rc=1; \
	    used=$$(grep -i '^"RenderDll"' out/play/$$n-$$rn/autoexec.cfg | tail -1 | cut -d'"' -f4); \
	    [ "$$used" = "$$r" ] || { echo "== FAILED: $$n ran with $${used:-no renderer}, not $$r"; rc=1; }; \
	  done; \
	  for r in $(TEST_RENDERERS); do \
	    rn=$${r%.ren}; [ $$rn = $$ref ] && continue; \
	    echo "== compare: $$n, $$ref | $$rn"; \
	    $(DOCKER_RUN) env PLAYTEST_COMPARE_SIZE=640x480 PLAYTEST_COMPARE_EXT=jpg bash build/playtest.sh compare $$n-$$ref $$n-$$rn; \
	  done; \
	done; echo "== side by side: out/play/compare-*-$(firstword $(TEST_RENDERERS:.ren=))-*.jpg"; exit $$rc

progress: retail
	$(DOCKER_RUN) env COMPLETE=$(COMPLETE) COVERED=$(COVERED) bash build/progress.sh check

progress-update: retail
	$(DOCKER_RUN) env ACCEPT_LOSSES=$(ACCEPT_LOSSES) bash build/progress.sh update

shell: image
	docker run --rm -it --init --name $(CONTAINER)-shell \
	  -u $(shell id -u):$(shell id -g) -e HOME=/tmp/home \
	  -v $(CURDIR):/repo -w /repo $(SRC_MOUNT) $(IMAGE) bash

clean:
	rm -rf out
	rm -rf src/engine/*/Release src/lithshared/*/Release src/lithshared/*/Debug src/lithshared/out \
	  src/game/*/Release src/game/*/Final_Release src/game/*/Debug \
	  src/engine/launcher/res src/engine/nolfserv/res src/engine/lithtech/res src/game/ClientRes/goty
	rm -f src/engine/*/link/aliases.obj src/engine/*/link/vtables.obj \
	  src/engine/*/link/ddraw7.lib src/engine/*/link/mss32dec.lib \
	  src/engine/d3d/link/d3dviews.obj src/engine/*/link/*.cmd src/engine/nolfserv/link/server.lib
	rm -f src/game/ObjectDLL/AnimationLex.cpp src/game/ObjectDLL/AnimationParse.cpp \
	  src/game/ObjectDLL/animationparse.cpp src/game/ObjectDLL/AnimationLexSymbols.h \
	  src/game/ClientRes/goty/cres_goty.rc

distclean: clean
	-docker rmi $(IMAGE)
