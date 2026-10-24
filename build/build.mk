# build/build.mk -- every compile and link of the game, as one Makefile.
#
# The libraries (src/lithshared/), the game DLLs (src/game/) and the engine
# (src/engine/) are one dependency graph here, so `make -j` compiles
# objects of every binary at once and a rebuild redoes only what changed: an
# object is rebuilt when its source, a header it includes (tools/mkdeps.py
# finds them: VC6 cannot list them), or its command line changes (the line
# is kept in <object>.cmd), a binary when any of its inputs does.
#
# It is run from the root of the tree being built, never from a checkout:
# build/libs.sh, build/build.sh and build/engine.sh run it in
# out/release-tree/ (src/ without its variant blocks, build/env.sh) and
# build/variant.sh in out/build-<variant>/, as
#
#   make -f build/build.mk -j<N> [target...]
#
# Targets: all (the default); libs; cres cshell object (game); ltmsg sres
# launcher nolfserv server lithtech d3d (engine); <x>-objs for a binary's
# objects without the link (server, lithtech, d3d, launcher, nolfserv); any
# single object.
#
# Every command line is the one the original build scripts ran, flag for
# flag, one source per compile instead of a batch per directory: the
# objects come out byte for byte the same (tools/buildcmp.py checks), and
# the compiles can run in parallel. The flags reproduce what each original
# binary was built with:
#   lithshared libraries    cl 12.00.8447 /O2
#   CRes, CShell, Object    cl 12.00 (toolchains/msvc-6.00-9782), /O2, with a
#                           precompiled header, from fixed Wine drives (below)
#   ltmsg, sres, launcher   cl 12.00, /O2
#   NolfServ.exe            cl 12.00, /O2 /GR, MFC, precompiled header
#   server.dll              cl 12.00.8447 /O1; localdriver.cpp /Os
#   lithtech.exe            cl 12.00.8447 /O2; a few files with their own flags
#   d3d.ren                 Intel C/C++ 4.0 (toolchains/icl-4.0), per-file
#                           flag groups; new files encode their group in
#                           their name (w<N>_*_<flags>.cpp)
# src/engine/server/*.cpp is compiled into both server.dll and lithtech.exe.
#
# From the environment (build/variant.sh sets them; empty for the release
# build, whose command lines are then exactly the original ones):
#   NOLFSRC_CFLAGS   appended to every compile (/D NOLFSRC_MODERN, /Z7 ...)
#   NOLFSRC_LDFLAGS  put first on every link
#   NOLFSRC_RCFLAGS  put first on every resource compile
#   GAME_CONFIG      final (the default: _REZFILE/_FINAL, Final_Release/,
#                    what the game runs from .rez archives with) or release

ROOT := $(CURDIR)
SHELL := /bin/bash
.SUFFIXES:
.DELETE_ON_ERROR:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
.DEFAULT_GOAL := all

ifeq ($(wildcard $(ROOT)/src/engine/lithtech),)
$(error run build/build.mk from the root of a tree, as build/engine.sh does)
endif

empty :=
space := $(empty) $(empty)
tab := $(empty)	$(empty)
define nl


endef
# one shell word per make word, quoted: Windows paths keep their backslashes
q = $(foreach w,$(1),'$(w)')

NOLFSRC_CFLAGS  ?=
NOLFSRC_LDFLAGS ?=
NOLFSRC_RCFLAGS ?=
GAME_CONFIG     ?= final
MODERN := $(if $(findstring NOLFSRC_MODERN,$(NOLFSRC_CFLAGS)),1)

# ---------------------------------------------------------------------------
# Wine and the toolchains
#
# Two Wine prefixes, one per way of seeing the tree, each with its own
# wineserver. VC6's output for CShell.dll and Object.lto depends on where the
# compiler finds things (the same source built in two directories whose
# paths differ in length gave two binaries), so the game DLLs are built with
# the tree's src/ as drive S: and toolchains/ as drive T:, and the compilers
# see the same paths wherever the tree is. Everything else must NOT see those
# drives: Wine names a working directory by the deepest drive that holds it,
# and Intel C/C++ builds a different d3d.ren from S:\engine\d3d than from
# Z:\...\src\engine\d3d. The prefixes live in the tree (.wine/) and are kept
# between builds.
export WINEDEBUG := -all
export WINEDLLOVERRIDES := mscoree,mshtml=
WINE := wine
PFX_PLAIN  := $(ROOT)/.wine/plain
PFX_DRIVES := $(ROOT)/.wine/drives

TC      := $(ROOT)/toolchains
VC9782  := $(TC)/msvc-6.00-9782
CL      := $(VC9782)/VC98/Bin/CL.EXE
CL8447  := $(TC)/msvc-6.00-8447/BIN/cl.exe
ICL     := $(TC)/icl-4.0/BIN/icl.exe
LINK    := $(VC9782)/VC98/Bin/LINK.EXE
LIBEXE  := $(VC9782)/VC98/Bin/LIB.EXE
RC      := $(VC9782)/Common/MSDev98/Bin/RC.EXE

# Windows paths: everything is under Z: (/) ...
wz = z:$(subst /,\,$(1))
# ... except, in the drives prefix, src/ (S:) and toolchains/ (T:)
ws = $(if $(filter $(ROOT)/src $(ROOT)/src/%,$(1)),s:$(subst /,\,$(patsubst $(ROOT)/src%,%,$(1))),$(if $(filter $(TC) $(TC)/%,$(1)),t:$(subst /,\,$(patsubst $(TC)%,%,$(1))),$(call wz,$(1))))

# The environment of a tool run, by prefix. src/nolfsrc/ (nolfsrc_debug.h)
# is on every compiler's include path, last.
# $(call wine_env,<path function: wz|ws>,<prefix>,<INCLUDE>,<LIB>)
wine_env = WINEPREFIX=$(2) WINEPATH='$(call $(1),$(VC9782))\VC98\Bin;$(call $(1),$(VC9782))\Common\MSDev98\Bin' LIB='$(4)' INCLUDE='$(3)'
inc_vc  = $(call $(1),$(VC9782))\VC98\INCLUDE;$(call $(1),$(VC9782))\VC98\MFC\INCLUDE;$(call $(1),$(VC9782))\VC98\ATL\INCLUDE;$(call $(1),$(ROOT)/src/nolfsrc)
lib_vc  = $(call $(1),$(VC9782))\VC98\LIB;$(call $(1),$(VC9782))\VC98\MFC\LIB
# Intel C/C++ 4.0 uses msvc-6.00-8168's headers and libraries beside its own
ICL_W   := $(call wz,$(TC)/icl-4.0)
V8168_W := $(call wz,$(TC)/msvc-6.00-8168)

# cl 12.00.8447 (CL8447) runs with these too -- msvc-6.00-9782's headers, not
# its own: the build scripts the libraries and the engine were matched with
# meant to give it msvc-6.00-8447's, but the INCLUDE they set never reached
# the compiler (a variable assigned on a shell function call). Its headers
# give different objects, so it stays this way.
ENV      := $(call wine_env,wz,$(PFX_PLAIN),$(call inc_vc,wz),$(call lib_vc,wz))
ENV_ICL  := $(call wine_env,wz,$(PFX_PLAIN),$(ICL_W)\INCLUDE;$(V8168_W)\INCLUDE;$(call wz,$(ROOT)/src/nolfsrc),$(ICL_W)\LIB;$(V8168_W)\LIB)
ENV_GAME := $(call wine_env,ws,$(PFX_DRIVES),$(call inc_vc,ws),$(call lib_vc,ws))
# The engine images link the C runtime of Visual C++ 6.0 SP3 (msvc-6.00-8447:
# libcmt.lib's objects are cl 8168's, as the retail images' Rich headers
# count them), not SP6's, which adds the heap selection (__MSVCRT_HEAP_SELECT)
# the retail images lack; d3d.ren links against Intel's libraries first.
ENG_CRT ?= 8447
ENG_LIB_W := $(call wz,$(TC)/msvc-6.00-$(ENG_CRT))\LIB
ENV_ENGLINK := $(call wine_env,wz,$(PFX_PLAIN),$(call inc_vc,wz),$(ENG_LIB_W);$(call lib_vc,wz))
ENV_D3DLINK := $(call wine_env,wz,$(PFX_PLAIN),$(call inc_vc,wz),$(ICL_W)\LIB;$(ENG_LIB_W);$(call lib_vc,wz))

# The prefixes: made the first time, then only checked. Every tool run waits
# for its prefix (an order-only prerequisite, so it causes no rebuild). Intel
# C/C++ needs a TEMP, which a new prefix does not always have, and Wine never
# takes TEMP or TMP from the Unix environment: it is set in the registry.
$(PFX_PLAIN)/.ready $(PFX_DRIVES)/.ready:
	@echo "   WINE prefix $(@D)"
	@mkdir -p $(@D) && export WINEPREFIX=$(@D) && { $(WINE) wineboot -i >/dev/null 2>&1; \
	  for v in TEMP TMP; do $(WINE) reg add 'HKCU\Environment' /v $$v /t REG_SZ /d 'C:\windows\temp' /f >/dev/null 2>&1; done; \
	  wineserver -w; } && test -f $(@D)/system.reg && touch $@
.PHONY: wine-plain wine-drives FORCE
wine-plain: $(PFX_PLAIN)/.ready
	@rm -f $(PFX_PLAIN)/dosdevices/s: $(PFX_PLAIN)/dosdevices/t:
wine-drives: $(PFX_DRIVES)/.ready
	@ln -sfn $(ROOT)/src $(PFX_DRIVES)/dosdevices/s:
	@ln -sfn $(TC) $(PFX_DRIVES)/dosdevices/t:

# ---------------------------------------------------------------------------
# The machinery: compile and tool rules
#
# Every output's command is a variable, cmd_<output>, that the rule runs and
# keeps in <output>.cmd. An output whose command differs from the kept one
# is rebuilt (FORCE): a changed flag, a file added to a link.
# (compared stripped: what $(file <) leaves of the line's end is not reliable)
cmd_changed = $(call str_ne,$(strip $(cmd_$(1))),$(strip $(file <$(1).cmd)),FORCE)
str_ne = $(if $(subst x$(1),,x$(2))$(subst x$(2),,x$(1)),$(3))

OBJECTS :=
DEPSPEC :=

# $(call compile,<object>,<working dir>,<source as spelled>,<env>,<exe>,<flags>,<prefix>[,<first>])
# One compile: `<exe> <flags> <source> $(NOLFSRC_CFLAGS)` in <working dir>,
# after the shell command <first> if there is one
# (the variant flags last, after the command's own: /Oy- must follow the /O2
# that implies /Oy). The source and its headers are prerequisites through
# tools/mkdeps.py's rules.
define compile
OBJECTS += $(1)
cmd_$(1) := cd $(2) && $(if $(8),$(8) && )$(4) $(WINE) $(5) $(call q,$(6) $(3) $(NOLFSRC_CFLAGS))
DEPSPEC += $(subst $(space),|,$(1)$(tab)$(2)$(tab)$(3)$(tab)$(strip $(6)))
$(1): $$(call cmd_changed,$(1)) | $(7)
endef
c_obj = $(2)/$(basename $(notdir $(subst \,/,$(1)))).obj

# $(call tool,<output>,<prerequisites>,<command>): any other build step
define tool
cmd_$(1) := $(3)
$(1): $$(call cmd_changed,$(1)) $(2)
TOOLS += $(1)
endef
TOOLS :=

# ---------------------------------------------------------------------------
# 1. The lithshared static libraries -> src/lithshared/out/
#
# cl 12.00.8447, /MT /O2 like the game projects. The LT2-era headers in
# src/LT2/lithshared take precedence over the newer copies beside the
# sources. Each library is lib.exe over its directory's objects (Release/
# *.obj, a wildcard lib.exe expands, so member order is what it always was);
# objects that are not the library's are removed first.
LIB_CFLAGS ?= /nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /c
LT2LS := ..\..\LT2\lithshared
LOUT  := src/lithshared/out

# $(call lsobjs,<dir>,<incs>,<sources>) -> compile each into <dir>/Release
define lsobjs
$(foreach s,$(3),$(eval $(call compile,$(call c_obj,$(s),src/lithshared/$(1)/Release),src/lithshared/$(1),$(s),$(ENV),$(CL8447),$(LIB_CFLAGS) $(2) /FoRelease/,wine-plain)))
LSOBJ_$(1) := $(foreach s,$(3),$(call c_obj,$(s),src/lithshared/$(1)/Release))
endef
# $(call lslib,<dir>,<library>,<incs>,<sources>)
define lslib
$(call lsobjs,$(1),$(3),$(4))
$(call tool,$(LOUT)/$(2).lib,$$(LSOBJ_$(1)) | wine-plain,cd src/lithshared/$(1) && $(ENV) $(WINE) $(LIBEXE) /nologo '/out:..\out\$(2).lib' 'Release/*.obj')
pre_$(LOUT)/$(2).lib = $$(call prune,src/lithshared/$(1)/Release,$$(LSOBJ_$(1)))
LIBS += $(LOUT)/$(2).lib
endef
# the command that removes the objects in <dir> that are not in <list> (a
# tool's pre_<output>, run before its command), or nothing
prune = $(if $(filter-out $(2),$(wildcard $(1)/*.obj)),rm -f $(filter-out $(2),$(wildcard $(1)/*.obj)))
LIBS :=

$(eval $(call lslib,stdlith,StdLith,/I $(LT2LS)\stdlith /I $(LT2LS)\lith /I . /I sys\win /I ..\zlib,\
    abstractio.cpp dynarray.cpp fileio.cpp goodlinklist.cpp helpers.cpp l_allocator.cpp \
    linklist.cpp lithexception.cpp memory.cpp memoryio.cpp stringholder.cpp struct_bank.cpp \
    sys\win\compressedio.cpp))
# (These four have their /I paths with doubled backslashes: the original
# build's quoting gave them so, and Windows reads them the same.)
LT2LS2 := ..\\..\\LT2\\lithshared
$(eval $(call lslib,CryptMgr,CryptMgr,/I $(LT2LS2)\\cryptmgr /I $(LT2LS2)\\incs /I .,cryptmgr.cpp blowfish.cpp))
$(eval $(call lslib,lith,Lith,/I .,basehash.cpp baselist.cpp virtlist.cpp))
$(eval $(call lslib,rezmgr,RezMgr,/I $(LT2LS2)\\incs /I .,rezmgr.cpp rezmgr2.cpp rezfile.cpp rezemu.cpp rezdir.cpp rezhash.cpp))
$(eval $(call lslib,RegMgr,RegMgr,/I $(LT2LS2)\\incs /I .,regmgr.cpp))
$(eval $(call lslib,MFCStub,MFCStub,/I $(LT2LS2)\\mfcstub /I $(LT2LS2)\\incs /I $(LT2LS2)\\lith /I .,mfcs_string.cpp mfcs_rect.cpp mfcs_point.cpp))
$(eval $(call lslib,ltguimgr,ltguimgr_LT2,/D _NOMFC /D _LITHTECH2 /I $(LT2LS)\ltguimgr /I $(LT2LS)\incs /I $(LT2LS)\mfcstub /I $(LT2LS)\lith /I ..\..\LT2\sdk\inc /I .,\
    $(notdir $(wildcard src/lithshared/ltguimgr/*.cpp))))
$(eval $(call lslib,lithfontmgr,LithFontMgr_LT2,/D _LITHTECH2 /I $(LT2LS)\lithfontmgr /I $(LT2LS)\incs /I ..\..\LT2\sdk\inc /I .,\
    $(notdir $(wildcard src/lithshared/lithfontmgr/*.cpp))))

# ButeMgrNoMFC: the attribute parser plus the zTools containers
$(eval $(call lsobjs,ztools,/D _NOMFC /I $(LT2LS)\incs /I .,$(notdir $(wildcard src/lithshared/ztools/*.cpp))))
$(eval $(call lsobjs,ButeMgr,/D _NOMFC /I $(LT2LS)\butemgr /I $(LT2LS)\mfcstub /I $(LT2LS)\incs /I $(LT2LS)\lith /I $(LT2LS)\cryptmgr /I .,butemgr.cpp))
$(eval $(call tool,$(LOUT)/ButeMgrNoMFC.lib,$$(LSOBJ_ztools) $$(LSOBJ_ButeMgr) | wine-plain,\
    cd src/lithshared && $(ENV) $(WINE) $(LIBEXE) /nologo '/out:out\ButeMgrNoMFC.lib' 'ButeMgr\Release\butemgr.obj' 'ztools\Release\*.obj'))
pre_$(LOUT)/ButeMgrNoMFC.lib = $(call prune,src/lithshared/ztools/Release,$(LSOBJ_ztools))
LIBS += $(LOUT)/ButeMgrNoMFC.lib

# ButeMgr: the same parser built against MFC (statically linked, as the
# MFC applications that link it are): butemgr.cpp without _NOMFC, with the
# same zTools objects. NolfServ.exe links it. (The source release ships no
# prebuilt copy of this flavour, so it is scored inside NolfServ.exe only.)
$(eval $(call compile,src/lithshared/ButeMgr/Release/mfc/butemgr.obj,src/lithshared/ButeMgr,butemgr.cpp,$(ENV),$(CL8447),\
    $(LIB_CFLAGS) /I $(LT2LS)\butemgr /I $(LT2LS)\incs /I $(LT2LS)\lith /I $(LT2LS)\cryptmgr /I . /FoRelease/mfc/,wine-plain))
$(eval $(call tool,$(LOUT)/ButeMgr.lib,src/lithshared/ButeMgr/Release/mfc/butemgr.obj $$(LSOBJ_ztools) | wine-plain,\
    cd src/lithshared && $(ENV) $(WINE) $(LIBEXE) /nologo '/out:out\ButeMgr.lib' 'ButeMgr\Release\mfc\butemgr.obj' 'ztools\Release\*.obj'))
LIBS += $(LOUT)/ButeMgr.lib

# GameSpyMgr / GameSpyClientMgr: our replacements for the GameSpy middleware
$(eval $(call lsobjs,gamespy,/I $(LT2LS)\incs /I $(LT2LS)\mfcstub /I .,GameSpyMgr.cpp GameSpyClientMgr.cpp))
$(foreach l,GameSpyMgr GameSpyClientMgr,$(eval $(call tool,$(LOUT)/$(l).lib,src/lithshared/gamespy/Release/$(l).obj | wine-plain,\
    cd src/lithshared/gamespy && $(ENV) $(WINE) $(LIBEXE) /nologo '/out:..\out\$(l).lib' 'Release\$(l).obj')))
LIBS += $(LOUT)/GameSpyMgr.lib $(LOUT)/GameSpyClientMgr.lib

# RegMgr_Debug: regmgr.cpp with regmgr60.dsp's Debug settings (/ZI /Od
# /D _DEBUG; /MTd, which the source release's libs/Debug/RegMgr.lib says it
# was built with). The Game of the Year CShell.dll linked this build, not
# the Release one (its five CRegMgr functions are the unoptimised code,
# byte for byte, ahead of StdLith). Only CShell links it. /ZI keeps the
# types in Debug/vc60.pdb and adds to whatever is there, which changes the
# object: the program database starts empty every time.
$(eval $(call compile,src/lithshared/RegMgr/Debug/regmgr.obj,src/lithshared/RegMgr,regmgr.cpp,$(ENV),$(CL8447),\
    /nologo /MTd /W3 /GX /ZI /Od /D WIN32 /D _DEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /I $(LT2LS)\incs /I . /FoDebug/ /FdDebug/ /c,wine-plain,\
    rm -f Debug/vc60.pdb Debug/vc60.idb))
$(eval $(call tool,$(LOUT)/RegMgr_Debug.lib,src/lithshared/RegMgr/Debug/regmgr.obj | wine-plain,\
    cd src/lithshared/RegMgr && $(ENV) $(WINE) $(LIBEXE) /nologo '/out:..\out\RegMgr_Debug.lib' 'Debug\regmgr.obj'))
LIBS += $(LOUT)/RegMgr_Debug.lib

.PHONY: libs
libs: $(LIBS)
lslib = $(LOUT)/$(1).lib

# ---------------------------------------------------------------------------
# 2. The game DLLs, from Monolith's v1.003 source release (src/game/)
#
# Source lists come from the .dsp project files (tools/dsp2files.py). In the
# modern build (NOLFSRC_MODERN) CShell.dll and Object.lto also get the
# project's own game code (src/modern/cshell/, src/modern/object/), find
# headers under src/modern/, and link winmm.lib; CRes.dll gets
# src/modern/cres/*.rc.
ifeq ($(GAME_CONFIG),final)
GOUT := Final_Release
FINAL_DEFS := /D _REZFILE /D _FINAL
else
GOUT := Release
FINAL_DEFS :=
endif
GAME_CL := /nologo /MT /W3 /GX /O2 /c
MODERN_INCS := $(if $(MODERN),/I ..\..\modern)
MODERN_LIBS := $(if $(MODERN),winmm.lib)
# The old iostream library (libcimt.lib) the game DLLs and NolfServ.exe link
# is Visual C++ 6.0's first release's, as in the retail images (cl 8168
# objects in their Rich headers): the one SP6 ships was rebuilt, and adds
# an RTTI-carrying __InitLock class the retail images do not have.
LIBCIMT_RTM := msvc-6.00-8168\LIB\libcimt.lib
WINLIBS := $(strip kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib $(MODERN_LIBS))
dsp = $(shell python3 tools/dsp2files.py $(1) Release cpp)

# $(call gamelib,<project dir>) -> the lithshared library, as the link names it
glib = ..\..\lithshared\out\$(1).lib

# CRes.dll: main.cpp and ClientRes.rc, plus what the retail Game of the Year
# CRES.dll has and the v1.003 source lacks -- the GOTY strings and credits,
# which build/all.sh extracts from the retail game into goty/. Compiled from
# goty/, where rc finds the GOTY credits.txt before ClientRes.rc's own.
CRES := src/game/ClientRes
$(eval $(call compile,$(CRES)/Release/main.obj,$(CRES),main.cpp,$(ENV_GAME),$(CL),\
    /nologo /MT /W3 /GX /O2 /I ..\shared /D WIN32 /D NDEBUG /D _WINDOWS /FoRelease/ /c,wine-drives))
CRES_RC := $(CRES)/goty/cres_goty.rc
# (a printf format: \\ is one backslash)
hash := \#
# The modern build adds the PS2 levels' text (goty/ps2.rc) when build/all.sh
# made it from the PS2 disc (MODERN.md, P8).
CRES_RC_FMT := $(hash)include "..\\ClientRes.rc"\r\n$(hash)include "goty.rc"\r\n$(foreach r,$(if $(MODERN),$(notdir $(wildcard src/modern/cres/*.rc))),$(hash)include "..\\..\\..\\modern\\cres\\$(r)"\r\n)$(if $(MODERN),$(if $(wildcard $(CRES)/goty/ps2.rc),$(hash)include "ps2.rc"\r\n))
$(CRES_RC): FORCE
	@test -f $(CRES)/goty/goty.rc -a -f $(CRES)/goty/credits.txt || \
	  { echo "error: $(CRES)/goty/ is missing -- build/all.sh writes it" >&2; exit 1; }
	@printf '$(subst \r\n $(hash),\r\n$(hash),$(CRES_RC_FMT))' > $@.tmp; \
	  if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv -f $@.tmp $@; fi
$(eval $(call tool,$(CRES)/Release/ClientRes.res,$(CRES_RC) $(filter-out %/Release %/goty %.cpp,$(wildcard $(CRES)/*)) $(wildcard $(CRES)/goty/*.rc $(CRES)/goty/*.txt) $(if $(MODERN),$(wildcard src/modern/cres/*)) | wine-drives,\
    cd $(CRES)/goty && $(ENV_GAME) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /i .. '/fo..\Release\ClientRes.res' cres_goty.rc))
$(eval $(call tool,$(CRES)/Release/CRes.dll,$(CRES)/Release/main.obj $(CRES)/Release/ClientRes.res | wine-drives,\
    cd $(CRES) && $(ENV_GAME) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /subsystem:windows /dll /machine:I386 /out:Release/CRes.dll \
    Release/main.obj Release/ClientRes.res kernel32.lib user32.lib gdi32.lib advapi32.lib))
.PHONY: cres
cres: $(CRES)/Release/CRes.dll

# $(call game_objs,<dir>,<defs+incs>,<pch sources>,<no-pch sources>,<modern subdir>)
# Stdafx.cpp makes the precompiled header (/Yc), which the rest use (/Yu);
# a few sources are built without it.
define game_objs
$(call compile,$(1)/$(GOUT)/Stdafx.obj,$(1),..\Shared\Stdafx.cpp,$(ENV_GAME),$(CL),$(GAME_CL) $(2) /Ycstdafx.h /Fp$(GOUT)/proj.pch /Fo$(GOUT)/,wine-drives)
$(foreach s,$(3),$(eval $(call compile,$(call c_obj,$(s),$(1)/$(GOUT)),$(1),$(s),$(ENV_GAME),$(CL),$(GAME_CL) $(2) /Yustdafx.h /Fo$(GOUT)/ /Fp$(GOUT)/proj.pch,wine-drives))$(nl)$(call c_obj,$(s),$(1)/$(GOUT)): $(1)/$(GOUT)/Stdafx.obj$(nl))
$(foreach s,$(4),$(eval $(call compile,$(call c_obj,$(s),$(1)/$(GOUT)),$(1),$(s),$(ENV_GAME),$(CL),$(GAME_CL) $(2) /Fo$(GOUT)/,wine-drives)))
$(foreach s,$(if $(MODERN),$(wildcard src/modern/$(5)/*.cpp)),$(eval $(call compile,$(1)/$(GOUT)/modern/$(basename $(notdir $(s))).obj,$(1),..\..\modern\$(5)\$(notdir $(s)),$(ENV_GAME),$(CL),$(GAME_CL) $(2) /Fo$(GOUT)/modern/,wine-drives)))
GOBJ_$(1) := $(1)/$(GOUT)/Stdafx.obj $(foreach s,$(3) $(4),$(call c_obj,$(s),$(1)/$(GOUT))) \
             $(foreach s,$(if $(MODERN),$(sort $(wildcard src/modern/$(5)/*.cpp))),$(1)/$(GOUT)/modern/$(basename $(notdir $(s))).obj)
endef
# $(call game_link,<dir>,<binary>,<extra link flags>,<libraries>): the link,
# through a response file as the original build had it
define game_link
$(call tool,$(1)/$(GOUT)/$(2),$$(GOBJ_$(1)) $(foreach l,$(4),$(call lslib,$(l))) | wine-drives,\
    cd $(1) && { echo '/nologo /subsystem:windows /dll /machine:I386$(if $(3), $(3))'; \
      echo '/out:"$(GOUT)/$(2)" /map:"$(GOUT)/$(basename $(2)).map"'; \
      $$(foreach o,$$(GOBJ_$(1)),echo '"$$(subst /,\,$$(patsubst $(1)/%,%,$$(o)))"';) \
      $(foreach l,$(4),echo '"$(call glib,$(l))"';) \
      echo '/nodefaultlib:libcimt.lib "t:\$(LIBCIMT_RTM)"'; \
      echo '$(WINLIBS)'; } > $(GOUT)/link.rsp && \
    $(ENV_GAME) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) '@$(GOUT)/link.rsp')
endef

CSHELL := src/game/ClientShellDLL
CSHELL_SRC := $(subst /,\,$(filter-out %/Stdafx.cpp %/stdafx.cpp,$(call dsp,$(CSHELL)/ClientShellDLL.dsp)))
$(eval $(call game_objs,$(CSHELL),/D NDEBUG /D _CLIENTBUILD /D WIN32 /D _WINDOWS /D _NOLFBUILD /D NO_PRAGMA_LIBS $(FINAL_DEFS) \
    /I ..\shared /I ..\..\LT2\sdk\inc /I ..\..\LT2\lithshared\incs /I . $(MODERN_INCS),$(CSHELL_SRC),,cshell))
# RegMgr_Debug, not RegMgr, first of the libraries: see above (and the .dsp's
# /nodefaultlib:"LIBCMTD", which is for it)
$(eval $(call game_link,$(CSHELL),CShell.dll,/nodefaultlib:LIBCMTD,\
    RegMgr_Debug StdLith CryptMgr LithFontMgr_LT2 ltguimgr_LT2 MFCStub ButeMgrNoMFC GameSpyClientMgr))
.PHONY: cshell
cshell: $(CSHELL)/$(GOUT)/CShell.dll

# Object.lto. The animation lexer and parser are generated with the bundled
# Win32 flex and bison (the .dsp's custom build steps; bison's defines file
# becomes AnimationLexSymbols.h; it names its output in lower case, which
# Windows did not mind and make does) and built, with ltquatbase.cpp, without
# the precompiled header.
OBJDLL := src/game/ObjectDLL
OBJDLL_ALL := $(subst /,\,$(filter-out %/Stdafx.cpp %/stdafx.cpp,$(call dsp,$(OBJDLL)/Object.dsp)))
OBJDLL_NOPCH := $(filter %\AnimationLex.cpp %\AnimationParse.cpp %\ltquatbase.cpp,$(OBJDLL_ALL))
OBJDLL_NOPCH += $(if $(filter %\AnimationLex.cpp,$(OBJDLL_NOPCH)),,AnimationLex.cpp) \
                $(if $(filter %\AnimationParse.cpp,$(OBJDLL_NOPCH)),,AnimationParse.cpp)
$(eval $(call game_objs,$(OBJDLL),/D NDEBUG /D WIN32 /D _WINDOWS /D _NOLFBUILD /D NO_PRAGMA_LIBS $(FINAL_DEFS) \
    /I ..\..\LT2\sdk\inc /I ..\..\LT2\lithshared\incs /I ..\shared /I . $(MODERN_INCS),\
    $(filter-out $(OBJDLL_NOPCH),$(OBJDLL_ALL)),$(OBJDLL_NOPCH),object))
$(eval $(call game_link,$(OBJDLL),Object.lto,,StdLith CryptMgr MFCStub ButeMgrNoMFC GameSpyMgr RegMgr))
$(OBJDLL)/AnimationLex.cpp: $(OBJDLL)/AnimationLex.l | wine-drives
	@echo "   FLEX $@"
	@cd $(OBJDLL) && $(ENV_GAME) $(WINE) ../flex.exe -Pyyanimation -oAnimationLex.cpp AnimationLex.l
$(OBJDLL)/AnimationParse.cpp $(OBJDLL)/AnimationLexSymbols.h &: $(OBJDLL)/AnimationParse.y | wine-drives
	@echo "   BISON $@"
	@cd $(OBJDLL) && $(ENV_GAME) $(WINE) ../bison.exe --defines -v --verbose -pyyanimation -oAnimationParse.cpp AnimationParse.y && \
	  rm -f AnimationLexSymbols.h && \
	  { mv -f animationparse.cpp.h AnimationLexSymbols.h 2>/dev/null || mv -f AnimationParse.cpp.h AnimationLexSymbols.h; } && \
	  rm -f animationparse.cpp.output AnimationParse.cpp.output && \
	  { [ ! -f animationparse.cpp ] || mv -f animationparse.cpp AnimationParse.cpp; }
$(OBJDLL)/$(GOUT)/AnimationLex.obj: $(OBJDLL)/AnimationLex.cpp $(OBJDLL)/AnimationLexSymbols.h
$(OBJDLL)/$(GOUT)/AnimationParse.obj: $(OBJDLL)/AnimationParse.cpp
.PHONY: object game
object: $(OBJDLL)/$(GOUT)/Object.lto
game: cres cshell object

# ---------------------------------------------------------------------------
# 3. The engine (src/engine/)
#
# Each link reads committed link inputs in src/engine/<target>/link/: an
# alias list (aliases.rsp, turned into an object of weak externals by
# tools/mkaliasobj.py), for lithtech.exe and server.dll the stand-in vtables
# (vtables.txt, tools/mkvtabobj.py), for d3d.ren the data views (d3dviews.txt,
# tools/mkviewobj.py). Import libraries for Miles and DirectDraw are
# generated from their export lists. Each link's report is
# src/engine/<target>/Release/<target>-link.txt; a link that leaves no
# binary fails the build with the report's errors.
ENG_CL := /nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /c
E := src/engine

# A link's output goes to its report (report_<binary>, the original
# build's <target>-link.txt): the shared tool rule prints its count of
# unresolved externals, and its errors if no binary came out.

# ltmsg.dll: resources and the C runtime's DLL startup, around the C
# runtime's own default DllMain. Built, as the retail one was (its Rich
# header), entirely with Visual C++ 6.0 as first released (msvc-6.00-8168):
# its compiler, its libcmt.lib, and its linker, whose cvtres.exe (5.00.1720)
# turns the resources into an object.
V8168 := $(TC)/msvc-6.00-8168
ENV_LTMSG := WINEPREFIX=$(PFX_PLAIN) WINEPATH='$(V8168_W)\BIN' LIB='$(V8168_W)\LIB' INCLUDE='$(V8168_W)\INCLUDE'
$(eval $(call compile,$(E)/ltmsg/Release/ltmsg.obj,$(E)/ltmsg,ltmsg.cpp,$(ENV_LTMSG),$(V8168)/BIN/cl.exe,$(ENG_CL) /FoRelease/,wine-plain))
$(eval $(call tool,$(E)/ltmsg/Release/ltmsg.res,$(E)/ltmsg/ltmsg.rc | wine-plain,\
    cd $(E)/ltmsg && $(ENV) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /foRelease/ltmsg.res ltmsg.rc))
$(eval $(call tool,$(E)/ltmsg/Release/ltmsg.dll,$(E)/ltmsg/Release/ltmsg.obj $(E)/ltmsg/Release/ltmsg.res | wine-plain,\
    cd $(E)/ltmsg && $(ENV_LTMSG) $(WINE) $(V8168)/BIN/link.exe $(NOLFSRC_LDFLAGS) /nologo /subsystem:windows /dll /machine:I386 \
    /out:Release/ltmsg.dll /map:Release/ltmsg.map Release/ltmsg.obj Release/ltmsg.res kernel32.lib))

# sres.dll
$(eval $(call compile,$(E)/sres/Release/sres.obj,$(E)/sres,sres.cpp,$(ENV),$(CL),$(ENG_CL) /FoRelease/,wine-plain))
$(eval $(call tool,$(E)/sres/Release/sres.dll,$(E)/sres/Release/sres.obj | wine-plain,\
    cd $(E)/sres && $(ENV) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /subsystem:windows /dll /machine:I386 \
    /out:Release/sres.dll Release/sres.obj kernel32.lib))

# NOLF.exe, the launcher; its art (res/) is read out of the retail NOLF.exe
# by build/all.sh. Miles' import libraries come from libs/mss32.def: lib.exe
# makes the undecorated one, tools/mkimplib.py the decorated one.
LAUNCHER := $(E)/launcher
LAUNCHER_SRC := stdafx.cpp launcher.cpp gamedlg.cpp gamehelp.cpp dlgconfig.cpp dlgcustom.cpp dlghelp.cpp dlgsound.cpp \
    dlgdisplay.cpp preview.cpp cdcheck.cpp dib.cpp dibmgr.cpp dibpal.cpp RezFind.cpp Utils.cpp globals.cpp
$(foreach s,$(LAUNCHER_SRC),$(eval $(call compile,$(LAUNCHER)/Release/$(s:.cpp=.obj),$(LAUNCHER),$(s),$(ENV),$(CL),\
    /nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /I ..\..\LT2\lithshared\incs /c /FoRelease/,wine-plain)))
$(eval $(call tool,$(LAUNCHER)/Release/launcher.res,$(LAUNCHER)/launcher.rc $(LAUNCHER)/launcher.h $(wildcard $(LAUNCHER)/res/*) | wine-plain,\
    cd $(LAUNCHER) && $(ENV) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /foRelease/launcher.res launcher.rc))
MSS32_DEF := $(LAUNCHER)/libs/mss32.def
MSS32_DECORATED = $(shell sed -n "s/^ *\(AIL_[A-Za-z0-9_]*@[0-9]*\) *$$/_\1/p" $(MSS32_DEF))
$(eval $(call tool,$(LAUNCHER)/Release/mss32.lib,$(MSS32_DEF) | wine-plain,\
    cd $(LAUNCHER) && $(ENV) $(WINE) $(LIBEXE) /nologo /machine:i386 '/def:libs\mss32.def' '/out:Release\mss32.lib' >/dev/null))
$(foreach d,$(LAUNCHER)/Release $(E)/lithtech/link,$(eval $(call tool,$(d)/mss32dec.lib,$(MSS32_DEF),\
    python3 tools/mkimplib.py --keep-decoration $(d)/mss32dec.lib mss32.dll $$(MSS32_DECORATED) >/dev/null)))
LAUNCHER_LINKOBJ := launcher gamedlg gamehelp dlgconfig dlgcustom dlghelp dlgsound dlgdisplay preview cdcheck dib dibmgr \
    dibpal RezFind Utils globals stdafx
# Modern executables share every launcher object; only their entry points
# differ. The retail build keeps its original output and link inputs.
LAUNCHER_BINS := $(if $(MODERN),NOLFLauncher NOLF,NOLF)
ifneq ($(MODERN),)
$(foreach s,launcherentry gameentry,$(eval $(call compile,$(LAUNCHER)/Release/$(s).obj,$(LAUNCHER),..\..\modern\launcher\$(s).cpp,$(ENV),$(CL),\
    /nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /c /FoRelease/,wine-plain)))
endif
launcher_entry = $(if $(MODERN),$(if $(filter NOLF,$(1)),gameentry,launcherentry))
define link_launcher
$(eval $(call tool,$(LAUNCHER)/Release/$(1).exe,$(LAUNCHER_SRC:%.cpp=$(LAUNCHER)/Release/%.obj) $(LAUNCHER)/Release/launcher.res \
    $(if $(MODERN),$(LAUNCHER)/Release/$(call launcher_entry,$(1)).obj) \
    $(LAUNCHER)/Release/mss32.lib $(LAUNCHER)/Release/mss32dec.lib $(call lslib,RegMgr) | wine-plain,\
    cd $(LAUNCHER) && $(ENV) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /subsystem:windows /machine:I386 \
    /out:Release/$(1).exe /map:Release/$(1).map $(LAUNCHER_LINKOBJ:%=Release/%.obj) $(if $(MODERN),Release/$(call launcher_entry,$(1)).obj) Release/launcher.res \
    $(call q,..\..\lithshared\out\RegMgr.lib Release\mss32dec.lib Release\mss32.lib) nafxcw.lib libcmt.lib \
    kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib comdlg32.lib winmm.lib ole32.lib oleaut32.lib uuid.lib \
    odbc32.lib odbccp32.lib))
endef
$(foreach b,$(LAUNCHER_BINS),$(call link_launcher,$(b)))
.PHONY: launcher launcher-objs
launcher-objs: $(LAUNCHER_SRC:%.cpp=$(LAUNCHER)/Release/%.obj) $(LAUNCHER)/Release/launcher.res
launcher: $(LAUNCHER_BINS:%=$(LAUNCHER)/Release/%.exe)

# NolfServ.exe, the dedicated server: an MFC dialog application (MFC linked
# statically, RTTI on, a precompiled header, as AppWizard set it up), its
# own sources in link order, the launcher's dlgcustom.cpp (through
# CustomDlg.cpp) and the game's NetDefs.cpp. It links server.dll by ordinal
# (link/server.def), and the RegMgr, CryptMgr and ButeMgr (MFC) libraries.
# Its icons and wizard bitmap (res/) are read out of the retail NolfServ.exe
# by build/all.sh.
NOLFSERV := $(E)/nolfserv
NOLFSERV_CL := /nologo /MT /W3 /GR /GX /O2 /I ..\..\LT2\sdk\inc /I ..\..\LT2\lithshared\incs /I ..\..\game\Shared \
    /D NDEBUG /D WIN32 /D _WINDOWS /D NO_PRAGMA_LIBS /D _NOLFSERV /c
NOLFSERV_SRC := CustomDlg.cpp ..\launcher\dlgcustom.cpp ..\..\game\Shared\NetDefs.cpp OptionsAssaultDlg.cpp OptionsDeathmatchDlg.cpp OptionsDlg.cpp \
    ServerApp.cpp ServerDlg.cpp SessionDlg.cpp StdAfx.cpp WizardDlg.cpp WorldsDlg.cpp
# The modern build's own server code (MODERN.md, P6), and src/modern/ on the
# include path
NOLFSERV_SRC += $(if $(MODERN),..\..\modern\nolfserv\p6_server.cpp)
NOLFSERV_CL += $(MODERN_INCS)
NOLFSERV_OBJ := $(foreach s,$(NOLFSERV_SRC),$(call c_obj,$(s),$(NOLFSERV)/Release))
$(eval $(call compile,$(NOLFSERV)/Release/StdAfx.obj,$(NOLFSERV),StdAfx.cpp,$(ENV),$(CL),\
    $(NOLFSERV_CL) /Ycstdafx.h /FpRelease/NolfServ.pch /FoRelease/,wine-plain))
$(foreach s,$(filter-out StdAfx.cpp,$(NOLFSERV_SRC)),$(eval $(call compile,$(call c_obj,$(s),$(NOLFSERV)/Release),$(NOLFSERV),$(s),$(ENV),$(CL),\
    $(NOLFSERV_CL) /Yustdafx.h /FpRelease/NolfServ.pch /FoRelease/,wine-plain))$(eval $(call c_obj,$(s),$(NOLFSERV)/Release): $(NOLFSERV)/Release/StdAfx.obj))
$(eval $(call tool,$(NOLFSERV)/Release/nolfserv.res,$(NOLFSERV)/nolfserv.rc $(NOLFSERV)/resource.h $(wildcard $(NOLFSERV)/res/*) | wine-plain,\
    cd $(NOLFSERV) && $(ENV) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /foRelease/nolfserv.res nolfserv.rc))
$(eval $(call tool,$(NOLFSERV)/link/server.lib,$(NOLFSERV)/link/server.def | wine-plain,\
    cd $(NOLFSERV) && $(ENV) $(WINE) $(LIBEXE) /nologo /machine:i386 '/def:link\server.def' '/out:link\server.lib' >/dev/null))
$(eval $(call tool,$(NOLFSERV)/Release/NolfServ.exe,$(NOLFSERV_OBJ) $(NOLFSERV)/Release/nolfserv.res $(NOLFSERV)/link/server.lib \
    $(call lslib,CryptMgr) $(call lslib,RegMgr) $(call lslib,ButeMgr) | wine-plain,\
    cd $(NOLFSERV) && $(ENV) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /subsystem:windows /machine:I386 \
    /out:Release/NolfServ.exe /map:Release/NolfServ.map $(NOLFSERV_OBJ:$(NOLFSERV)/%=%) Release/nolfserv.res \
    winmm.lib $(call q,link\server.lib ..\..\lithshared\out\CryptMgr.lib ..\..\lithshared\out\RegMgr.lib ..\..\lithshared\out\ButeMgr.lib) \
    /nodefaultlib:libcimt.lib $(call q,$(call wz,$(TC))\$(LIBCIMT_RTM)) winmm.lib))
.PHONY: nolfserv nolfserv-objs
nolfserv-objs: $(NOLFSERV_OBJ) $(NOLFSERV)/Release/nolfserv.res
nolfserv: $(NOLFSERV)/Release/NolfServ.exe

# RezMgr's objects, copied beside server.dll's and lithtech.exe's: the
# links take the library, the per-object checks (engine.sh verify) these.
REZ_OBJ := rezmgr rezmgr2 rezfile rezemu rezdir rezhash
define rezcopy
$(1)/Release/$(2).obj: src/lithshared/rezmgr/Release/$(2).obj
	@cp -p $$< $$@
endef
$(foreach d,$(E)/server $(E)/lithtech,$(foreach o,$(REZ_OBJ),$(eval $(call rezcopy,$(d),$(o)))))

# server.dll: cl 8447 /O1, no /GX; localdriver.cpp /Os without /Og (and
# /Gf, string pooling, which /O1 implies)
SERVER := $(E)/server
SERVER_CL := /MT /W3 /O1 /D WIN32 /D NDEBUG /D _WINDOWS /D LT_SERVER_TIMERS /D LT_SERVER_BUILD /D NO_PRAGMA_LIBS /I ..\..\LT2\sdk\inc /I . /c /FoRelease/
SERVER_SRC := server.cpp servermgr.cpp servermsg.cpp netmgr.cpp servermath.cpp commonphysics.cpp commonmodel.cpp \
    sharedcommon.cpp servermodel.cpp serverlightanim.cpp servercommon.cpp servermessage.cpp packet.cpp \
    lthread.cpp smgr.cpp counter.cpp dalloc.cpp modelanim.cpp streamsim.cpp fsstream.cpp basestream.cpp \
    objbank.cpp objtypes.cpp collection.cpp worldbsp.cpp worldmodels.cpp saveio.cpp worldsim.cpp \
    serverengine.cpp ltobject.cpp lmessage.cpp spritecontrol.cpp tcpdriver.cpp stringbank.cpp dsi.cpp \
    genericprop.cpp dhash.cpp filemgr.cpp console.cpp structbank.cpp csbase.cpp sdkforce.cpp netobj.cpp \
    savrest.cpp shade.cpp smgrnew.cpp modelnew.cpp objmover.cpp smgrmisc.cpp svnew.cpp svlast.cpp \
    svtiny.cpp worldtree.cpp
SERVER_OS_SRC := localdriver.cpp
server_flags = $(if $(filter $(1),$(SERVER_OS_SRC)),$(subst /O1,/Os /Ob1 /Gf,$(SERVER_CL)),$(SERVER_CL))
$(foreach s,$(SERVER_SRC) $(SERVER_OS_SRC),$(eval $(call compile,$(SERVER)/Release/$(s:.cpp=.obj),$(SERVER),$(s),$(ENV),$(CL8447),\
    /nologo $(call server_flags,$(s)),wine-plain)))
SERVER_OBJ := $(sort $(patsubst %.cpp,$(SERVER)/Release/%.obj,$(SERVER_SRC) $(SERVER_OS_SRC)))
$(eval $(call tool,$(SERVER)/link/aliases.obj,$(SERVER)/link/aliases.rsp,python3 tools/mkaliasobj.py $(SERVER)/link/aliases.rsp $(SERVER)/link/aliases.obj))
$(eval $(call tool,$(SERVER)/link/vtables.obj,$(SERVER)/link/vtables.txt,python3 tools/mkvtabobj.py $(SERVER)/link/vtables.txt $(SERVER)/link/vtables.obj))
# its resources: the version block and one string (server.rc). link/server.def
# only names the DLL "server.DLL" in its export table, as the retail one does.
$(eval $(call tool,$(SERVER)/Release/server.res,$(SERVER)/server.rc | wine-plain,\
    cd $(SERVER) && $(ENV) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /foRelease/server.res server.rc))
$(eval $(call tool,$(SERVER)/Release/server.dll,$(SERVER)/link/vtables.obj $(SERVER)/link/aliases.obj $(SERVER_OBJ) $(SERVER)/Release/server.res $(SERVER)/link/server.def $(call lslib,RezMgr) $(call lslib,StdLith) $(call lslib,Lith) | wine-plain,\
    cd $(SERVER) && { $(ENV_ENGLINK) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /dll /machine:I386 /out:Release/server.dll /map:Release/server.map \
    '/def:link\server.def' /export:CreateServer /export:DeleteServer link/vtables.obj link/aliases.obj $(SERVER_OBJ:$(SERVER)/%=%) Release/server.res \
    $(call q,..\..\lithshared\out\RezMgr.lib ..\..\lithshared\out\StdLith.lib ..\..\lithshared\out\Lith.lib) \
    winmm.lib libcmt.lib kernel32.lib user32.lib ole32.lib wsock32.lib > Release/server-link.txt 2>&1 || true; }))
report_$(SERVER)/Release/server.dll := $(SERVER)/Release/server-link.txt
.PHONY: server server-objs
server-objs: $(SERVER_OBJ) $(REZ_OBJ:%=$(SERVER)/Release/%.obj) $(SERVER)/Release/server.res
server: server-objs $(SERVER)/Release/server.dll

# lithtech.exe: cl 8447 /O2. The copy of server's localdriver.cpp is
# unoptimised (/Od, with intrinsics and inline expansion) but pools its
# strings (/Gf, which /O1 and /O2 imply in VC6: each literal once, in .data,
# shared with every other object's), and controlfile.cpp is built with C++
# exceptions (/GX).
LITHTECH := $(E)/lithtech
LITHTECH_CL := /MT /W3 /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /I ..\..\LT2\sdk\inc /I ..\server /I . /c /FoRelease/
LITHTECH_SRC := lithtech.cpp ..\server\msgterm.cpp ..\server\sdkforce.cpp ..\server\netobj.cpp \
    ..\server\smgrnew.cpp ..\server\modelnew.cpp ..\server\objmover.cpp ..\server\smgrmisc.cpp \
    ..\server\shade.cpp wmrotate.cpp collelem.cpp cchandler.cpp cmodelload.cpp dmextra.cpp csvis.cpp \
    diblit.cpp savobj.cpp conline.cpp nettrace.cpp modchunk.cpp partfm.cpp ccobj.cpp demodtx.cpp \
    cissurf.cpp cltapi.cpp sndinst2.cpp sndmgr2.cpp predtick.cpp cmgrinit.cpp wgrid.cpp dmgrow.cpp \
    cmgsurf.cpp warpblit.cpp startload.cpp fntdraw.cpp predobj.cpp fmcvt32.cpp fmconvert.cpp \
    ccclient.cpp clientshell.cpp ccline.cpp binkvideo.cpp smackvideo.cpp lvideomgr.cpp soundinst2.cpp \
    tcpqarray.cpp clientmgr3.cpp clientwin.cpp worldtree2.cpp transform.cpp clientinterface.cpp \
    clientphysics.cpp clientsound.cpp dmloader.cpp clientmgr2.cpp stdlithalloc.cpp clientsoundinst.cpp \
    clientworld.cpp cltcursor.cpp clientconsole.cpp clientdsi.cpp lttexmod.cpp clienttexmgr.cpp \
    clientstreams.cpp clientinput.cpp clientsoundmgr.cpp clientworldinfo.cpp clientblit.cpp \
    clientrender.cpp ltnew1.cpp cltnew.cpp cltrest.cpp lt2new1.cpp lt2new2.cpp lt2new3.cpp lt2new4.cpp \
    ltnew2.cpp ..\server\csbase.cpp ..\server\servermgr.cpp ..\server\servermsg.cpp \
    ..\server\netmgr.cpp ..\server\servermath.cpp ..\server\commonphysics.cpp ..\server\commonmodel.cpp \
    ..\server\sharedcommon.cpp ..\server\servermodel.cpp ..\server\serverlightanim.cpp \
    ..\server\servercommon.cpp ..\server\servermessage.cpp ..\server\packet.cpp ..\server\lthread.cpp \
    ..\server\smgr.cpp ..\server\ltobject.cpp ..\server\lmessage.cpp ..\server\spritecontrol.cpp \
    ..\server\server.cpp ..\server\stringbank.cpp ..\server\dsi.cpp ..\server\genericprop.cpp \
    ..\server\dhash.cpp ..\server\filemgr.cpp ..\server\console.cpp ..\server\structbank.cpp \
    ..\server\counter.cpp ..\server\dalloc.cpp ..\server\modelanim.cpp ..\server\basestream.cpp \
    ..\server\collection.cpp ..\server\fsstream.cpp ..\server\objbank.cpp ..\server\objtypes.cpp \
    ..\server\saveio.cpp ..\server\serverengine.cpp ..\server\streamsim.cpp ..\server\tcpdriver.cpp \
    ..\server\worldbsp.cpp ..\server\worldmodels.cpp ..\server\worldsim.cpp worldtree.cpp clientmusic.cpp
# The modern build's own engine code (MODERN.md): the ##gamepad device.
LITHTECH_SRC += $(if $(MODERN),..\..\modern\engine\p7_gamepad.cpp)
LITHTECH_OD_SRC := ..\server\localdriver.cpp
LITHTECH_GX_SRC := controlfile.cpp
lithtech_flags = $(subst /O2,$(strip \
    $(if $(filter $(1),$(LITHTECH_OD_SRC)),/Od /Oi /Ob1 /Gf,\
    $(if $(filter $(1),$(LITHTECH_GX_SRC)),/GX /O2,/O2))),$(LITHTECH_CL))
LITHTECH_ALL := $(LITHTECH_SRC) $(LITHTECH_OD_SRC) $(LITHTECH_GX_SRC)
$(foreach s,$(LITHTECH_ALL),$(eval $(call compile,$(call c_obj,$(s),$(LITHTECH)/Release),$(LITHTECH),$(s),$(ENV),$(CL8447),\
    /nologo $(call lithtech_flags,$(s)),wine-plain)))
LITHTECH_OBJ := $(sort $(foreach s,$(LITHTECH_ALL),$(call c_obj,$(s),$(LITHTECH)/Release)))
# the modern build's ILTClient has slots the retail one lacks (vtables.txt's
# %ifdef NOLFSRC_MODERN block)
$(eval $(call tool,$(LITHTECH)/link/aliases.obj,$(LITHTECH)/link/aliases.rsp,python3 tools/mkaliasobj.py $(LITHTECH)/link/aliases.rsp $(LITHTECH)/link/aliases.obj))
$(eval $(call tool,$(LITHTECH)/link/vtables.obj,$(LITHTECH)/link/vtables.txt,\
    python3 tools/mkvtabobj.py $(if $(MODERN),--define NOLFSRC_MODERN) $(LITHTECH)/link/vtables.txt $(LITHTECH)/link/vtables.obj))
# its resources (lithtech.rc): the icon and the console font, read out of the
# retail lithtech.exe into res/ (build/all.sh, step 2b), MFC's from afxres.rc
$(eval $(call tool,$(LITHTECH)/Release/lithtech.res,$(LITHTECH)/lithtech.rc $(wildcard $(LITHTECH)/res/*) | wine-plain,\
    cd $(LITHTECH) && $(ENV) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /foRelease/lithtech.res lithtech.rc))
$(eval $(call tool,$(LITHTECH)/Release/lithtech.exe,$(LITHTECH)/link/vtables.obj $(LITHTECH)/link/aliases.obj $(LITHTECH_OBJ) $(LITHTECH)/Release/lithtech.res \
    $(call lslib,RezMgr) $(call lslib,StdLith) $(call lslib,Lith) $(LITHTECH)/link/mss32dec.lib $(LAUNCHER)/Release/mss32.lib | wine-plain,\
    cd $(LITHTECH) && { $(ENV_ENGLINK) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /subsystem:windows /machine:I386 \
    /out:Release/lithtech.exe /map:Release/lithtech.map link/vtables.obj link/aliases.obj $(LITHTECH_OBJ:$(LITHTECH)/%=%) Release/lithtech.res \
    $(call q,..\..\lithshared\out\RezMgr.lib ..\..\lithshared\out\StdLith.lib ..\..\lithshared\out\Lith.lib) \
    winmm.lib dinput.lib dxguid.lib libcmt.lib kernel32.lib user32.lib gdi32.lib ole32.lib uuid.lib \
    $(call q,link\mss32dec.lib ..\launcher\Release\mss32.lib) wsock32.lib \
    > Release/lithtech-link.txt 2>&1 || true; }))
report_$(LITHTECH)/Release/lithtech.exe := $(LITHTECH)/Release/lithtech-link.txt
.PHONY: lithtech lithtech-objs
lithtech-objs: $(LITHTECH_OBJ) $(REZ_OBJ:%=$(LITHTECH)/Release/%.obj) $(LITHTECH)/Release/lithtech.res
lithtech: lithtech-objs $(LITHTECH)/Release/lithtech.exe

# d3d.ren: Intel C/C++ 4.0, with per-file flag groups. Files not in any
# group build /O2. /Qip: interprocedural register calling between functions
# of one file; /Qip_no_inlining: the same without inlining; /Os: favour
# size; /Oi-: CRT intrinsics called, not inlined; /Qrcd: fast float-to-int
# conversion; /Qsfalign16: realign the stack only for 16-byte locals; /G5:
# Pentium scheduling. Files named w<N>_*_<flags>.cpp join a group by their
# suffix. The first group that names a file gives its flags.
D3D := $(E)/d3d
D3D_SRC := $(notdir $(wildcard $(D3D)/*.cpp))
d3dw = $(filter w%_$(1).cpp,$(D3D_SRC))
D3D_GROUPS := OS_G5 OS_SF QIP_OS_OI_SF QIP_SF QIP_NI_SF QIP_OS_OI_NI QIP_OS_OI QIP_OS_NI RCD OI OS_OI QIP_NI OS QIP QIP_OS
D3D_OS_G5        := /Os /G5                              $(call d3dw,osg5)
D3D_OS_SF        := /Os /Qsfalign16                      $(call d3dw,ossf)
D3D_QIP_OS_OI_SF := /Os /Oi- /Qip /Qsfalign16            $(call d3dw,osoiqipsf)
D3D_QIP_SF       := /O2 /Qip /Qsfalign16                 $(call d3dw,qipsf)
D3D_QIP_NI_SF    := /O2 /Qip /Qip_no_inlining /Qsfalign16 big24090.cpp $(call d3dw,qipnisf)
D3D_QIP_OS_OI_NI := /Os /Oi- /Qip /Qip_no_inlining       $(call d3dw,osoiqipni)
D3D_QIP_OS_OI    := /Os /Oi- /Qip                        d3dblit.cpp big1ef30.cpp $(call d3dw,osoiqip)
D3D_QIP_OS_NI    := /Os /Qip /Qip_no_inlining            $(call d3dw,osqipni) big6d910.cpp
D3D_RCD          := /O2 /Qrcd                            big8fd0.cpp big2b1b0.cpp big051a0.cpp big5dca0.cpp big40e10.cpp \
                                                         w4_100234d0.cpp w16_10042e80.cpp
D3D_OI           := /O2 /Oi-                             big4b720.cpp
D3D_OS_OI        := /Os /Oi-                             d3dr5c.cpp d3dr2d_os.cpp d3dbatchd.cpp big4e3e4.cpp $(call d3dw,osoi)
D3D_QIP_NI       := /O2 /Qip /Qip_no_inlining            d3dqip4.cpp $(call d3dw,qipni)
D3D_OS           := /Os                                  big729c0.cpp d3d.cpp d3dr5a.cpp d3dr5d_os.cpp d3dbatchb_os.cpp \
    d3dmid.cpp d3dr2a_os.cpp d3dr2c_os.cpp d3dr3a_os.cpp d3ddraw_os.cpp d3dmid_os.cpp d3dneutral.cpp d3dopt2d.cpp d3dlow.cpp \
    d3dhigh.cpp d3dbatchc.cpp d3dbatcha.cpp d3dr2b.cpp d3dr3b.cpp d3dr3d.cpp d3dr4a.cpp d3dr4c.cpp d3dr4b.cpp d3dr3c_os.cpp \
    $(call d3dw,os)
D3D_QIP          := /O2 /Qip                             d3dqip.cpp d3dqip2.cpp $(call d3dw,qip)
D3D_QIP_OS       := /Os /Qip                             d3dqip3.cpp big7cc90.cpp d3dr5b.cpp
# Not icl at all: StdLith's l_allocator.cpp (LAlloc and g_DefAlloc), VC6
# library code linked into d3d.ren, built as StdLith is (see w12_7fae0.cpp)
D3D_CL_SRC := w12_7fae0.cpp
d3d_group = $(firstword $(foreach g,$(D3D_GROUPS),$(if $(filter $(1),$(D3D_$(g))),$(g))))
d3d_opt = $(if $(call d3d_group,$(1)),$(filter /%,$(D3D_$(call d3d_group,$(1)))),/O2)
$(foreach s,$(filter-out $(D3D_CL_SRC),$(D3D_SRC)),$(eval $(call compile,$(D3D)/Release/$(s:.cpp=.obj),$(D3D),$(s),$(ENV_ICL),$(ICL),\
    /nologo /MT /W3 $(call d3d_opt,$(s)) /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /I ..\..\LT2\sdk\inc /I . /c /FoRelease/,wine-plain)))
$(foreach s,$(D3D_CL_SRC),$(eval $(call compile,$(D3D)/Release/$(s:.cpp=.obj),$(D3D),$(s),$(ENV),$(CL8447),\
    /nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /c /I ..\..\LT2\lithshared\stdlith /FoRelease/,wine-plain)))
D3D_OBJ := $(sort $(D3D_SRC:%.cpp=$(D3D)/Release/%.obj))
$(eval $(call tool,$(D3D)/link/aliases.obj,$(D3D)/link/aliases.rsp,python3 tools/mkaliasobj.py $(D3D)/link/aliases.rsp $(D3D)/link/aliases.obj))
$(eval $(call tool,$(D3D)/link/d3dviews.obj,$(D3D)/link/d3dviews.txt,python3 tools/mkviewobj.py $(D3D)/link/d3dviews.txt $(D3D)/link/d3dviews.obj))
$(eval $(call tool,$(D3D)/link/ddraw7.lib,,python3 tools/mkimplib.py $(D3D)/link/ddraw7.lib DDRAW.dll _DirectDrawCreateEx@16))
# its one resource, the renderer's name (d3d.rc)
$(eval $(call tool,$(D3D)/Release/d3d.res,$(D3D)/d3d.rc | wine-plain,\
    cd $(D3D) && $(ENV) $(WINE) $(RC) $(NOLFSRC_RCFLAGS) /l 0x409 /d NDEBUG /foRelease/d3d.res d3d.rc))
$(eval $(call tool,$(D3D)/Release/d3d.ren,$(D3D)/link/aliases.obj $(D3D)/link/d3dviews.obj $(D3D)/link/ddraw7.lib $(D3D_OBJ) $(D3D)/Release/d3d.res $(call lslib,StdLith) | wine-plain,\
    cd $(D3D) && { $(ENV_D3DLINK) $(WINE) $(LINK) $(NOLFSRC_LDFLAGS) /nologo /dll /machine:I386 \
    /out:Release/d3d.ren /map:Release/d3d.map /export:RenderDLLSetup /export:GetSupportedModes /export:FreeModeList \
    link/aliases.obj link/d3dviews.obj $(D3D_OBJ:$(D3D)/%=%) Release/d3d.res $(call q,..\..\lithshared\out\StdLith.lib) \
    ddraw.lib link/ddraw7.lib winmm.lib libmmt.lib libcmt.lib kernel32.lib user32.lib dxguid.lib > Release/d3d-link.txt 2>&1 || true; }))
report_$(D3D)/Release/d3d.ren := $(D3D)/Release/d3d-link.txt
.PHONY: d3d d3d-objs
d3d-objs: $(D3D_OBJ) $(D3D)/Release/d3d.res
d3d: $(D3D)/Release/d3d.ren

.PHONY: engine all
engine: ltmsg sres launcher nolfserv server lithtech d3d
.PHONY: ltmsg sres
ltmsg: $(E)/ltmsg/Release/ltmsg.dll
sres: $(E)/sres/Release/sres.dll
all: libs game engine

# ---------------------------------------------------------------------------
# The rules all compiles and tools share

dups := $(strip $(foreach o,$(sort $(OBJECTS)),$(if $(word 2,$(filter $(o),$(OBJECTS))),$(o))))
ifneq ($(dups),)
$(error two sources compile to one object: $(dups))
endif

$(OBJECTS):
	@echo "   CC   $@"
	@mkdir -p $(@D) && rm -f $@
	@$(cmd_$@)
	@test -f $@ || { echo "error: $@ was not built" >&2; exit 1; }
	@printf '%s\n' '$(subst ','\'',$(cmd_$@))' > $@.cmd

$(TOOLS):
	@echo "   GEN  $@"
	@mkdir -p $(@D) && rm -f $@ $(if $(filter %.exe %.dll %.ren %.lto,$@),$(basename $@).pdb $(basename $@).ilk)
	@$(pre_$@)
	@$(cmd_$@)
	@$(if $(report_$@),echo "   $@: unresolved externals: `grep -o 'unresolved external symbol .*' $(report_$@) | sort -u | wc -l`")
	@test -f $@ || { $(if $(report_$@),grep -E 'error|unresolved' $(report_$@) | head -20 >&2;) echo "error: $@ was not built" >&2; exit 1; }
	@printf '%s\n' '$(subst ','\'',$(cmd_$@))' > $@.cmd

FORCE:

# Header dependencies (tools/mkdeps.py), brought up to date on every run
DEPDIR := $(ROOT)/.build
$(shell mkdir -p $(DEPDIR))
$(file >$(DEPDIR)/deps.spec,$(subst |, ,$(subst $(space),$(nl),$(DEPSPEC))))
DEPS_OK := $(shell python3 tools/mkdeps.py $(DEPDIR)/deps.spec $(DEPDIR)/deps.mk && echo ok)
ifneq ($(DEPS_OK),ok)
$(error tools/mkdeps.py failed)
endif
include $(DEPDIR)/deps.mk
