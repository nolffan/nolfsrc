"""What the scoreboard scores: No One Lives Forever's binaries, and where
each one's own code, and ours, is.

ext/tools/progress.py and ext/tools/ratchet.py read this file and nothing else
about the project (their header documents every key). It is NOLF1's
configuration, which ext/tools/progress.py held itself until the tools became
shared with nolf2src and cjacksrc; the evidence for every address is the
comment beside it (the CSVs in this directory are the rest of it).

Our side of each image: nolfsrc's links write no response file
(<image>.rsp), so each target lists its objects (`objs`, globs under the
repository) -- the object directory of its build, and the lithshared
libraries of ours it links -- and its .map beside the image places our
functions.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "ext", "tools"))
from d3dcheck import NAMED as D3D_NAMED, SYMBOLS as D3D_SYMBOLS  # noqa: E402

REFERENCE = "out/reference"     # build/reference.sh writes it
OUT = "out/progress"            # each run's functions.csv and summary.json
DATA = "progress"               # the recorded baseline and the evidence

# Every hex address cited in these source trees marks the address
# `attempted` in a target with `cites`. Monolith's game source cites no
# retail address, and the game DLLs share server.dll's and d3d.ren's
# 0x10000000 range, so they do not read them.
CITED_SRC = ["src/engine", "src/lithshared"]

# The lithshared libraries, scored on their own, symbol by symbol, against
# the prebuilt .libs of the NOLF Source Code v1.003 release ($NOLF_SOURCE):
# (label, ours.lib); theirs is SHIPPED_LIB there, or SHIPPED_AS's path
# (relative to SHIPPED_LIB's directory).
LIBS = [
    ("StdLith",         "src/lithshared/out/StdLith.lib"),
    ("CryptMgr",        "src/lithshared/out/CryptMgr.lib"),
    ("RegMgr",          "src/lithshared/out/RegMgr.lib"),
    ("RegMgr_Debug",    "src/lithshared/out/RegMgr_Debug.lib"),
    ("MFCStub",         "src/lithshared/out/MFCStub.lib"),
    ("ButeMgrNoMFC",    "src/lithshared/out/ButeMgrNoMFC.lib"),
    ("ltguimgr_LT2",    "src/lithshared/out/ltguimgr_LT2.lib"),
    ("LithFontMgr_LT2", "src/lithshared/out/LithFontMgr_LT2.lib"),
]
SHIPPED_LIB = "LT2/lithshared/libs/Release/{}.lib"
# The GOTY CShell.dll linked the Debug build of RegMgr (build/libs.sh), so
# that one is scored against the release's Debug library.
SHIPPED_AS = {"RegMgr_Debug": "../Debug/RegMgr.lib"}
SHIPPED_ENV = "NOLF_SOURCE"

# Translation units of ours that exist only to make the compiler emit
# functions for the oracle to compare (sdkforce.cpp takes the address of
# every SDK inline, so ALL of them are out of line in our link, whether or
# not the shipped image has them so): reported as `driver`.
DRIVER_OBJS = {"sdkforce.obj"}

# RezMgr's objects, as build/build.mk copies them beside the engine's own
# (for the per-object checks): the links read RezMgr.lib, and the .map names
# its functions by library member (`RezMgr:rezmgr2.obj`), so the library is
# what lithtech.exe and server.dll score and these copies are left out. Read
# as bare objects they could never be found in the map: a short function of
# theirs (the hash tables' five-byte destructor, server.dll 0x1003a9f0,
# lithtech.exe 0x0048e1a0) was placed by its shape alone and never checked by
# what it calls. (Not third-party code; `third_party` is the key that leaves
# an input of the link's directory out.)
REZMGR_COPIES = ["rezmgr.obj", "rezmgr2.obj", "rezfile.obj", "rezemu.obj",
                 "rezdir.obj", "rezhash.obj"]

LITHSHARED = "src/lithshared/out/"

TARGETS = [
    # ---- the engine -------------------------------------------------------
    dict(label="lithtech.exe", what="the engine", kind="image",
         shipped=REFERENCE + "/lithtech.exe", stem="lithtech",
         built="src/engine/lithtech/Release/lithtech.exe",
         # the server's sources compiled a second time, with lithtech.exe's
         # own compiler revision and flags (build/engine.sh), its own TUs,
         # the same prebuilt Lith.lib as server.dll (its island below, plus
         # baselist at 0x0048de30) and StdLith.lib, confined to its block
         # by lib_range
         objs=["src/engine/lithtech/Release/*.obj", LITHSHARED + "RezMgr.lib",
               LITHSHARED + "Lith.lib", LITHSHARED + "StdLith.lib"],
         third_party=REZMGR_COPIES,
         cites=True,
         # Placed by a rigorous oracle rather than by eye: the static CRT is
         # the same library in every image built with this toolchain, so it
         # is byte-identical across them, and server.dll's non-app region is
         # already established. Taking each server.dll function above
         # 0x10037b47 (the server cut as it then stood -- since corrected to
         # 0x1003e0f0, so part of that sample was really rezmgr.lib /
         # StdLith.lib, which are also prebuilt and identical across images;
         # the cut is still _ftol, as in server.dll) that is at least 32
         # bytes long and searching lithtech.exe's .text for those exact
         # bytes finds 71 of them, clustered into a dense tail. It begins at
         # _ftol (0x4928c0), with strncpy, _stricmp, _findfirst, _vsnprintf
         # and the CRT entry point following it. Nothing Monolith wrote is
         # identified above it: the highest error-tag literal names a
         # function at 0x48b820 and the highest function any of our objects
         # matches is a collection template at 0x4915c0. There are a few
         # CRT hits below the cut, interleaved with that template -- the
         # boundary is left under them deliberately, because counting a
         # little of the CRT against us errs in the honest direction.
         #   0x004a03d0..0x004a061f: Lith.lib's basehash.obj
         # (CBaseHashItem::Next 0x4a03d0 .. CBaseHash::GetFirstInBin
         # 0x4a0590) and virtlist.obj (CVirtBaseList::InsertFirst 0x4a05b0,
         # ::Delete 0x4a05e0), linked above the CRT, byte-identical to
         # server.dll 0x1004bb20..0x1004bdc3 and matched by our Lith.lib.
         # Bracketed by CRT: an IAT jump thunk at 0x4a03be below it,
         # __ehvec_ctor at 0x4a0620 above. (baselist.obj was pulled earlier
         # here, into the app region at 0x0048de30..0x0048df0f.)
         app=[(0x00401000, 0x004928bf), (0x004a03d0, 0x004a0620)],
         # StdLith.lib 0x490b50..0x4919df: every StdLith function with a
         # unique hit lands in one run, in link order struct_bank (sb_Init
         # 0x490b50 .. sb_AllocateNewStructPage 0x490c20, ends 0x490c7e),
         # helpers ($E2/$E1 initializer thunks 0x490c80/0x490c90 -- `jmp` /
         # `mov ecx, &g_Helpers; jmp ??0CHelpers` -- then CHelpers::CHelpers
         # 0x490ca0 .. ExtractNames 0x490e50), stringholder (CStringHolder
         # ctor 0x490f60 .. ClearStrings 0x491270, its CMoArray<SBank>
         # instances 0x4912d0..0x491739), l_allocator ($E thunks
         # 0x491740..0x49177b constructing the default LAlloc global
         # 0x4b4e88 with vtable 0x4a3c54, ~LAlloc 0x491780,
         # LAlloc::Alloc/Free 0x491790/0x4917a0 .. LAllocSimpleBlock::Clear
         # 0x491940), dynarray (MoArray_FindElementMemcmp 0x491950) and
         # goodlinklist (GLinkedList_FindElementMemcmp 0x491990, ends
         # 0x4919d1). The same objects in the same order as server.dll
         # 0x1003d170..0x1003e090, byte-identical bar relocations (sb_Term2
         # 0x490bd0 = 0x1003d1f0). Below it, 0x490b40 is a 4-byte engine
         # accessor after an engine function's `ret`; above it, 0x4919e0 is
         # a thiscall string copy matching no member of either our or the
         # shipped StdLith.lib (server.dll has the CRT's fpinit straight
         # after goodlinklist). Without the range, DefStdlithFree's 16 bytes
         # credit the engine's own free wrapper at 0x436510, StdLith's $E
         # thunks take the engine's startup/atexit thunks
         # 0x436880/0x4368b0, and CAbstractIO::Open's 8 bytes (`mov eax,1;
         # ret 8`) claim the tail of 0x4919e0 as a function at 0x491a10.
         lib_range={"StdLith.lib": (0x00490b50, 0x004919e0)},
         # the CRT's _purecall, every abstract vtable's filler
         # (ext/tools/vtabcheck.py: lithtech.exe links the CRT statically, so
         # there is no `jmp [IAT]` thunk to find it by)
         purecall=0x004928e7),
    dict(label="server.dll", what="the server", kind="image",
         shipped=REFERENCE + "/server.dll", stem="server",
         built="src/engine/server/Release/server.dll",
         # StdLith.lib is linked into server.dll whole (0x1003d170 ..
         # 0x1003e0ef, below); like RegMgr.lib in NOLF.exe, its functions
         # there are ours, from our reconstructed library. Lith.lib
         # (basehash/baselist/virtlist) is the island above the CRT.
         objs=["src/engine/server/Release/*.obj", LITHSHARED + "RezMgr.lib",
               LITHSHARED + "StdLith.lib", LITHSHARED + "Lith.lib"],
         third_party=REZMGR_COPIES,
         cites=True,
         # The static CRT (libcmt.lib) begins at _ftol, 0x1003e0f0.
         # Evidence: every libcmt.lib member was searched for in .text
         # (masked by its own relocations, asm labels included); the lowest
         # real hit is __ftol at 0x1003e0f0, followed densely by _free,
         # _malloc, _strcmp, _memset, _strlen, ... Nothing between the old
         # cut (0x10037b47, "last function referencing a Class::Method error
         # tag") and 0x1003e0f0 is CRT; it is, in link order:
         #   ..0x1003a54f  engine TUs (unaligned /O1 code): CMoArray
         #                 templates byte-identical from
         #                 worldbsp/collection.obj, the BaseNew/BaseDelete
         #                 pair at 0x10038329/0x10038366,
         #                 wt_VisibilityTraversal at 0x10038e77
         #   0x1003a550..  rezmgr.lib (16-aligned): rezmgr/rezmgr2/rezdir/
         #                 rezhash/rezfile/rezemu objects match 45+ functions
         #   0x1003d170..  StdLith.lib: struct_bank, l_allocator,
         #                 stringholder, dynarray, helpers, goodlinklist all
         #                 match exactly
         #   0x1004bb20..0x1004bdcf: Lith.lib (src/lithshared/lith, built by
         # build/libs.sh lith), linked above the CRT: basehash.obj
         # 0x1004bb20..0x1004bcfa, then baselist.obj
         # (CBaseList::InsertFirst/Delete 0x1004bd00/0x1004bd30) and
         # virtlist.obj (CVirtBaseList::InsertFirst/Delete
         # 0x1004bd60/0x1004bd90), all 13 byte-identical. The basehash
         # iterator/bin code is what rezmgr calls (CRezDir::GetNextSubDir
         # 0x1003aca0 calls 0x1004bb20). /GX code using __ehvec_ctor with
         # rezmgr's bin dtor 0x1003b460 and the engine's
         # 0x1000d25d/0x10028ace allocator pair; its twin sits after
         # lithtech.exe's CRT too (0x4a03d0..), byte-identical, so it is a
         # prebuilt Monolith lib. It is bracketed by CRT: an IAT jump thunk
         # at 0x1004bb10 below it and _strupr, __ehvec_ctor/dtor,
         # __ArrayUnwind from 0x1004bdd0 on. Deliberately not included:
         # 0x1004c070..end, the C++ EH unwind funclets and __ehhandler stubs
         # (.text$x) of Monolith functions -- compiler-generated, 2-5
         # unmasked bytes each, not separately reconstructable.
         app=[(0x10001000, 0x1003e0f0), (0x1004bb20, 0x1004bdd0)]),

    dict(label="d3d.ren", what="the Direct3D renderer", kind="image",
         shipped=REFERENCE + "/d3d.ren", stem="d3d",
         built="src/engine/d3d/Release/d3d.ren",
         # renderer-specific source only (icl 4.0; build/engine.sh)
         objs=["src/engine/d3d/Release/*.obj"],
         cites=True,
         # Intel C/C++: multi-byte lea/mov nop padding and /Qip register
         # entries (`X$`, `X$$1`); ext/tools/d3dcheck.py's tables name the
         # shipped address of the engine-named and template functions
         icl=True, own_names=D3D_NAMED, own_symbols=D3D_SYMBOLS,
         # The tail begins with the generic thunk run at 0x1007faa0, above
         # a stretch of int3 padding (found as lithtech.exe's was: 38 of
         # server.dll's CRT functions are in d3d.ren's .text, in a dense
         # tail). Those thunks are the twenty-three functions the engine
         # objects "match" in d3d.ren by coincidence, which the project has
         # always refused to count; the boundary says so in the
         # denominator too.
         #   0x1007faa0..0x1007fb1f: StdLith's l_allocator.cpp -- LAlloc's
         # three virtuals (0x1007fae0 Alloc, 0x1007faf0 Free, 0x1007fb00 the
         # scalar deleting destructor; vtable 0x100881b8) and g_DefAlloc's
         # VC6 initialiser thunks (0x1007faa0 `call 0x1007fab0; jmp
         # 0x1007fac0`, the vtable store, the atexit registration, the
         # atexit destructor). Monolith's code: its Alloc and Free call the
         # renderer's own DefStdlithAlloc/DefStdlithFree
         # (0x10021ce0/0x10021cb0), and src/engine/d3d/w12_7fae0.cpp is all
         # seven, byte-identical (cl 8447, as StdLith). It opens the tail
         # above the CRT; StdLith.lib's struct_bank.obj follows at
         # 0x1007fb20 (sb_Init), which d3d.ren does not score (StdLith's own
         # row does).
         app=[(0x10001000, 0x1007fa91), (0x1007faa0, 0x1007fb20)]),
    # ---- the launcher and the dedicated server ----------------------------
    dict(label="NOLF.exe", what="the launcher", kind="image",
         shipped=REFERENCE + "/NOLF.exe", stem="nolf",
         built="src/engine/launcher/Release/NOLF.exe",
         # every reconstructed launcher TU, and RegMgr, linked from our own
         # reconstructed lithshared library, so its functions here are ours
         # too
         objs=["src/engine/launcher/Release/*.obj", LITHSHARED + "RegMgr.lib"],
         # StdAfx.obj is empty: not reconstruction, so left out of our side
         third_party=["stdafx.obj"],
         linked_oracle=True, cites=True,
         # Monolith's own code runs 0x401000 .. 0x409ed2 (the last app
         # function is CDibPal::InitRes at 0x409e70). From 0x409ee0 on it
         # is the statically linked MFC (AfxGetMainWnd, the multi-monitor
         # stubs, CArchive operators) and then the CRT (_splitpath,
         # strncpy, ...).
         app=[(0x00401000, 0x00409ed2)]),
    dict(label="NolfServ.exe", what="the dedicated server", kind="image",
         shipped=REFERENCE + "/NolfServ.exe", stem="nolfserv",
         built="src/engine/nolfserv/Release/NolfServ.exe",
         # its own TUs and the three lithshared libraries it links, from
         # our own builds (the MFC ButeMgr has no prebuilt counterpart in
         # the source release: it is scored here only)
         objs=["src/engine/nolfserv/Release/*.obj", LITHSHARED + "CryptMgr.lib",
               LITHSHARED + "RegMgr.lib", LITHSHARED + "ButeMgr.lib"],
         third_party=["stdafx.obj"],    # empty, as the launcher's
         linked_oracle=True, cites=True,
         # Monolith's objects in link order (CustomDlg, the launcher's
         # dlgcustom, NetDefs, the option pages, ServerApp, ServerDlg,
         # SessionDlg, WizardDlg, WorldsDlg: 0x401000 .. 0x40aa0f), then the
         # lithshared libraries it links, which are ours as RegMgr.lib is in
         # NOLF.exe: CryptMgr (Encrypt 0x40aa10 .. Blowfish_decipher), RegMgr
         # (CRegMgr::Init 0x40b460 ..) and ButeMgr, MFC flavour
         # (CReservedWords 0x40b7f0 .. its zTools objects, the last being
         # zSLink::freecache at 0x410190, 34 bytes). Every library function
         # with a unique hit lies in 0x40aa10 .. 0x4101b2. From 0x4101c0 on
         # it is the static MFC (CPropertySheet::GetTabControl,
         # AfxGetMainWnd, the multi-monitor stubs, AfxEnableControlContainer:
         # nafxcw.lib members matched byte for byte), then the CRT.
         app=[(0x00401000, 0x004101b2)]),

    # ---- the game ---------------------------------------------------------
    # Built from Monolith's source release (src/game/, Final Release:
    # build/build.sh all final) and scored against the Game of the Year
    # copies in NOLFGOTY.REZ. They link like server.dll: Monolith's objects,
    # then the lithshared libraries (StdLith, CryptMgr, ltguimgr_LT2,
    # MFCStub, ButeMgrNoMFC, the GameSpy library, RegMgr), then the static
    # CRT, which begins with libcimt.lib (the iostream classes) and goes on
    # into libcmt.lib. Found by taking every function of 24+ bytes in our
    # own linked image from the libraries and the CRT (our .map says which),
    # masking our relocations, and searching the retail .text for it: each
    # group's unique hits form one run, in our link order. libcimt's first
    # function is the same 43-byte function in both images (ifstream's,
    # just below ifstream::close, the group's first unique hit). The
    # GameSpy library stays in the region as middleware: Monolith's thin
    # CGameSpy* wrapper over the GameSpy SDK (the "C Engine" server list in
    # CShell, the query responder in Object), shipped prebuilt;
    # src/lithshared/gamespy/ stands in for it from the headers alone and
    # is deliberately not byte-matched (the master server it talked to is
    # gone). Evidence for its ranges: the WSOCK32 imports, the
    # master.gamespy.com and \gamename\%s\... strings and every caller of
    # the block lie in it, and nothing else in the image calls into it
    # except through the wrapper. A library's block takes in the unmatched
    # addresses around its anchored span (lib_block): a library's first or
    # last function can be one too short to anchor (CryptMgr's 3-byte
    # ??0CCryptMgr ahead of its SetKey, LithBitmapFont::IsChromaKeyed after
    # its ClipFont).
    dict(label="CShell.dll", what="the game: client", kind="image",
         shipped=REFERENCE + "/CShell.dll", stem="cshell",
         built="src/game/ClientShellDLL/Final_Release/CShell.dll",
         # CShell links RegMgr_Debug, the Debug build of regmgr.lib, where
         # the others link the Release one: so did the Game of the Year
         # CShell.dll (build/libs.sh, build_regmgr_debug)
         objs=["src/game/ClientShellDLL/Final_Release/*.obj"]
              + [LITHSHARED + l + ".lib" for l in
                 ("StdLith", "CryptMgr", "ltguimgr_LT2", "MFCStub", "ButeMgrNoMFC",
                  "GameSpyClientMgr", "RegMgr_Debug", "LithFontMgr_LT2")],
         lib_block=True,
         # our stand-in for the GameSpy library builds `middleware`
         third_party=["GameSpyClientMgr.lib"],
         # libcimt 0x100edab0 (close at 0x100edbd3), libcmt from 0x100efd1b
         # (??_M, our first libcmt function) to _findenv 0x100fd769; above
         # the libraries, ButeMgrNoMFC ends at ??3zSLink 0x100eb600 and
         # GameSpyClientMgr is at 0x100eb6d0.
         #   0x100fd930..0x1010086f: LithFontMgr_LT2.lib, linked after the
         # CRT exactly as in our CShell.dll (our map: LithFontMgr_LT2
         # between LIBCMT and __strupr). Its 29 unique hits run 0x100fda40
         # (LithFont's constructor) .. 0x10100760 (LithBitmapFont::ClipFont,
         # 250 bytes); below it an import thunk (jmp [0x101080dc] at
         # 0x100fd920, then int3 padding), above it, after a 4-byte
         # nop-padded accessor at 0x10100860, libcmt's __strupr at
         # 0x10100870 (the same bytes as ours).
         app=[(0x10001000, 0x100edab0), (0x100fd930, 0x10100870)],
         # CGameSpyClientMgr::Init (strcpy of the game name and key into +8
         # / +0x88) right after ButeMgrNoMFC's zSLink::freecache, through
         # the SDK's hash table/array helpers, to libcimt's first function
         # at 0x100edab0
         middleware=[(0x100eb650, 0x100edab0)],
         # the CRT's _purecall (`push 0x19; call _amsg_exit`): libcmt is
         # linked statically, so ext/tools/vtabcheck.py has no `jmp [IAT]`
         # thunk to find it by, as with lithtech.exe
         purecall=0x100f0a13),
    dict(label="Object.lto", what="the game: server objects", kind="image",
         shipped=REFERENCE + "/Object.lto", stem="object",
         built="src/game/ObjectDLL/Final_Release/Object.lto",
         objs=["src/game/ObjectDLL/Final_Release/*.obj"]
              + [LITHSHARED + l + ".lib" for l in
                 ("StdLith", "CryptMgr", "MFCStub", "ButeMgrNoMFC", "GameSpyMgr", "RegMgr")],
         lib_block=True,
         third_party=["GameSpyMgr.lib"],    # as CShell.dll's
         # libcimt 0x10141db0 (close at 0x10141ed3), libcmt from
         # __cfltcvt_init 0x1014412f to __decomp 0x1015030e; RegMgr, the
         # last library, ends at 0x10141d70 + 50 bytes.
         #   0x101503e0..0x101503ff: MFCStub's CRect::CRect(l, t, r, b) (32
         # bytes, ret 0x10), linked after the CRT as in our Object.lto: an
         # import thunk at 0x101503d0 below it, CRT code from 0x10150400.
         app=[(0x10001000, 0x10141db0), (0x101503e0, 0x10150400)],
         # CGameSpyMgr::Init (7 arguments, this+0x107c) right after
         # StdLith's MoArray_FindElementMemcmp, to the end of the last
         # function's switch table; RegMgr's CRegMgr::Init starts at
         # 0x10141b30
         middleware=[(0x10140950, 0x10141b30)],
         # the CRT's _purecall, as CShell.dll's: without it the abstract
         # tables (CAIPoodleState's) are read on into the next one
         purecall=0x101449ed),

    # ---- resources: scored resource by resource, left out of the total -----
    # CRes.dll: string tables, dialogs, bitmaps and text, and nothing but
    # the CRT's DLL entry code -- so its resources are what is scored.
    dict(label="CRes.dll", what="the game's resources", kind="resources",
         shipped=REFERENCE + "/CRes.dll", stem="cres",
         built="src/game/ClientRes/Release/CRes.dll"),
    # The engine images' resources (version blocks, dialogs, string tables,
    # icons; ltmsg.dll's engine messages), each a target of its own beside
    # the image's code.
    dict(label="lithtech.exe:rsrc", what="the engine's resources", kind="resources",
         shipped=REFERENCE + "/lithtech.exe", stem="lithtech-rsrc",
         built="src/engine/lithtech/Release/lithtech.exe"),
    dict(label="server.dll:rsrc", what="the server's resources", kind="resources",
         shipped=REFERENCE + "/server.dll", stem="server-rsrc",
         built="src/engine/server/Release/server.dll"),
    dict(label="d3d.ren:rsrc", what="the renderer's resources", kind="resources",
         shipped=REFERENCE + "/d3d.ren", stem="d3d-rsrc",
         built="src/engine/d3d/Release/d3d.ren"),
    dict(label="NOLF.exe:rsrc", what="the launcher's resources", kind="resources",
         shipped=REFERENCE + "/NOLF.exe", stem="nolf-rsrc",
         built="src/engine/launcher/Release/NOLF.exe"),
    dict(label="NolfServ.exe:rsrc", what="the dedicated server's resources", kind="resources",
         shipped=REFERENCE + "/NolfServ.exe", stem="nolfserv-rsrc",
         built="src/engine/nolfserv/Release/NolfServ.exe"),
    dict(label="ltmsg.dll:rsrc", what="the engine's messages", kind="resources",
         shipped=REFERENCE + "/ltmsg.dll", stem="ltmsg",
         built="src/engine/ltmsg/Release/ltmsg.dll"),
    # the server's resource DLL (out of NOLF2.REZ): it has none, so its one
    # row says that ours has none either
    dict(label="sres.dll:rsrc", what="the server's resource DLL", kind="resources",
         shipped=REFERENCE + "/sres.dll", stem="sres",
         built="src/engine/sres/Release/sres.dll"),
]
