# progress/ -- how much of the build is byte-identical to the retail game

The goal of this source is to compile to exactly the machine code of the
retail game (*No One Lives Forever*, Game of the Year Edition, version
1.003). This directory holds the record of how far that has got, and the
evidence the scoring needs. The table in the top-level `README.md` is
generated from it.

```sh
make                                     # build (the scoring reads its output)
make progress                            # score it; fails on any regression
make progress-update                     # record the score here and in README.md
```

This is the reference. For a walk-through -- what to run when, what the
output means, what to do when it fails, and how to score another project
with the same tools -- see
[`tools/guides/14-progress.md`](../tools/guides/14-progress.md).

## What is scored

Every function of each retail binary is compared with what our build
produced for it. The binaries, where each one's own code is and what of
ours it is compared with are `targets.py` in this directory, with the
evidence for every address (the shared `tools/progress.py` knows no binary
by name):

| Target | Compared against | Our side |
|---|---|---|
| `lithtech.exe`, `server.dll`, `d3d.ren`, `NOLF.exe`, `NolfServ.exe` | the retail images, extracted from disc 2 into `out/reference/` | `src/engine/*/Release/*.obj` (plus the lithshared libraries each links; `NolfServ.exe`'s ButeMgr is the MFC flavour, which the source release has no prebuilt copy of and is scored only here) |
| `StdLith`, `CryptMgr`, `RegMgr`, `MFCStub`, `ButeMgrNoMFC`, `ltguimgr_LT2`, `LithFontMgr_LT2` | Monolith's prebuilt `.lib`s from the NOLF Source Code v1.003 release (`reference/`, or `NOLF_SOURCE`) | `src/lithshared/out/*.lib` |
| `CShell.dll`, `Object.lto` | the Game of the Year copies in disc 1's `NOLFGOTY.REZ` (the ones the GOTY install runs), extracted into `out/reference/` | `src/game/*/Final_Release/*.obj` (plus the lithshared libraries each links) |
| `CRes.dll` | the same archive's `CRES.dll` | `src/game/ClientRes/Release/CRes.dll`, resource by resource |
| `<image>:rsrc` (`lithtech.exe`, `server.dll`, `d3d.ren`, `NOLF.exe`, `NolfServ.exe`, `ltmsg.dll`, `sres.dll`) | the retail images' resources (`sres.dll`'s out of `NOLF2.REZ`, the only copy) | the resources of our images, resource by resource |

The discs hold three generations of the game DLLs (`NOLF2.REZ`: the
original release, `nolfu003.rez`: the 1.003 update, `NOLFGOTY.REZ`: Game of
the Year). Monolith's source release builds the Game of the Year ones:
`Object.lto` scores 89% of its bytes identical against them and 55% against
1.003's (`CShell.dll` is close either way: 66% and 68%).

`CRes.dll` has no code of Monolith's -- its only code is the C runtime's DLL
entry -- so each of its resources (string tables, dialogs, bitmaps, text) is
a row, sized by its data, `identical` when our bytes are the retail bytes.

The engine images' resources are scored the same way, each image's as a
target of its own beside its code (`lithtech.exe:rsrc`, ...; `ltmsg.dll`,
resources and the C runtime, only this way, and `sres.dll`, which has
no resources, as a row that says so): version blocks, dialogs,
string tables, icons, the console font, the MFC resources an AppWizard
application carried. Their text is `.rc` source (`lithtech.rc`,
`server/server.rc`, `d3d/d3d.rc`, ...); their art is read out of the retail
images at build time (`docs/BUILDING.md`, step 2b).

The README lists `CRes.dll` after `Object.lto`, and each image's resources
as a row of its own after it, marked as not in the total, which counts code
only.
Their rows are held like functions: `identical.txt` lists every identical
resource (`lithtech.exe:rsrc<TAB>rsrc:<type>/<name>/<language>`).

The lithshared libraries a game DLL links are scored twice: on their own
(above), and again inside the DLL, where they are placed like the game's own
functions -- except that a library function too short to anchor (claimed by
shape) or one that does not match (paired as `near`) is only placed inside
its library's block of the image: its anchored span, out to the nearest
function matched from anything else. The latter are the nears of the
library's own score, such as ButeMgrNoMFC's `zErrHandler::set_ef`.

The retail images have no symbols, so their functions are enumerated from the
code itself (call targets, vtables, callbacks, exports, ...), and each of our
compiled functions is searched for in the image. A function is
**identical** when our bytes equal the retail bytes everywhere except where
the linker writes an address (relocations, which an unlinked object leaves
blank). The libraries are compared symbol by symbol, the same way.

Each retail function gets one status (`out/progress/functions.csv`):

| Status | Meaning |
|---|---|
| `identical` | our code is the retail code, relocations aside |
| `near` | we have the function, but the bytes differ |
| `small` | we have it, but it is almost all relocations (global initializers), so there is nothing to compare: reconstructed, not verified |
| `attempted` | no match, but the address is cited in `src/` or recorded in the CSVs here |
| `todo` | nothing of ours corresponds to it yet |
| `crt` | the statically linked C runtime / MFC: listed, not counted |
| `middleware` | third-party code linked among Monolith's (the GameSpy library in `CShell.dll` and `Object.lto`, `middleware` in `targets.py`): listed, not counted. `src/lithshared/gamespy/` stands in for it and is not meant to match |

Three more statuses are for functions of OURS, not shipped ones, so none of
them is counted in the denominator:

| Status | Meaning |
|---|---|
| `unplaced` | written, and our own link puts it in our image, but no shipped function is placed for it |
| `unlinked` | compiled, but our own link leaves it out too: not part of this binary (most are server code `lithtech.exe` compiles but never links) |
| `driver` | emitted only by a scoring driver that ships nowhere (`sdkforce.cpp`) |

The README's bars are identical bytes over all counted bytes.

## Coverage: unwritten and unplaced

Next to identity, the scoreboard measures coverage, and the README shows it
per binary:

* **Unwritten**: shipped functions with nothing of ours placed at them
  (`todo` + `attempted`).
* **Unplaced**: functions of ours in our own image that no shipped function
  is placed for.

Both at zero means the build is complete: every shipped function has a
written, placed counterpart, near or identical, and nothing of ours is left
over. Identity is then a matter of converging the near ones.

A function is placed first by its bytes (identical, or `small` by shape), then
by similarity (`near`: same length within 15% and most unmasked bytes
agreeing). What both miss -- a function written right but compiled with other
registers, another block order, a different member offset -- would otherwise
be counted twice, once as an unwritten shipped function and once as an
unplaced one of ours. So the scorer also joins the two sides, and a function
joined this way is `near`:

1. **By name**: a function named for the shipped address it reconstructs
   (`sub_<addr>`, `d3d.ren`'s convention) is placed there, however
   differently it compiled.
2. **By object**: the linker lays an object's functions out together, so a
   shipped function nothing is placed at, whose nearest placed neighbours are
   object X's, is one of X's; X's unplaced functions are matched to those by
   size, closest first (within a factor of two). Our order within an object
   need not follow the shipped one -- a reconstructed file is not laid out in
   Monolith's order.
3. **By link order**: what is left, between two neighbouring functions placed
   in both images (our `.map` gives our addresses), is aligned in order by
   size.

Two refinements keep the count honest. An inline function from a header is
compiled into every object that includes it; the linker keeps one copy, so
placing one places them all (compiler-generated per-object names such as
`_$E12` excepted). And functions too short to tell apart (`mov eax,8 / ret`,
a one-`jmp` initializer thunk) are counted by shape: of each exact shape,
ours are unplaced only by as many as our image has more of than the shipped
one.

`make progress` fails if either count goes up; `make progress COVERED=1`
fails unless both are zero everywhere. `tools/progress.py --unplaced=<target>`
and `--todo=<target>` (and `--attempted`) list them.

This is the per-function measure. A whole binary is byte-perfect only once
every function is identical *and* it links in the same order, with the same
data and headers; that final, whole-file comparison is not made yet. The
data check (below) and the resource rows cover part of it: the text and the
resources.

## The data check: text outside the code

The function comparison masks every address the linker writes, so it never
reads the data those addresses point at; a wrong string, a missing literal,
a duplicate one, another C runtime's tables or imports pass it unseen.
`tools/datacheck.py` reads them. For each image (the engine images,
`ltmsg.dll`, `sres.dll` and the game DLLs) it takes the text of every
section that is neither code nor `.reloc` nor `.rsrc` (the resources are
scored resource by resource, above), reads the import and export tables as
what they are (`import KERNEL32.dll GetTickCount`, `export name server.DLL`),
and compares ours with the retail image's as multisets of strings: a string
the retail image holds more often than ours is a `retail` difference, one
ours holds more often an `ours` one. A string is a NUL-terminated run of at
least five printable bytes, mostly letters, starting 4-byte aligned (where
the compiler puts a literal: what printable bytes precede it are the end of
other data), with every relocated pointer's bytes taken out first.

The differences that are known and meant are listed, each under its reason,
in `progress/data-known.txt`: the GameSpy SDK our stand-in replaces (its
query strings, master servers, Winsock and `GetTickCount` imports), the
path Monolith's IDE compiled `Attachments.cpp` by (its `__FILE__`), the
Debug-runtime `sres.dll` (all of its retail text, by a `*` entry), and text
of code not reconstructed yet. The check is a ratchet like `identical.txt`:

* `make progress` (`datacheck.py check`) **fails on a difference the list
  does not have** -- a new one, or one that came back -- and reports the
  listed ones as counts, per image;
* `make progress-update` (`datacheck.py update`) **drops the entries that
  no longer differ**, so a difference once fixed is held fixed;
* entries are only added by hand, under a `# ` line giving the reason
  (`datacheck.py show [image]` lists every difference, known or not).

It reports, and fails, on text only. Other data-level differences are not
checked: the order of each DLL's imports within its import table, the
layout of `.data`, the headers and time stamps.

### For later: building from Monolith's path

One known difference is worth an experiment of its own. Monolith built the
game DLLs in the Visual C++ IDE, which passes each file to the compiler by
its full path, so `__FILE__` in `Object.lto` reads
`G:\Nolf\NolfCode1.03\Source\Game\ObjectDLL\Attachments.cpp` where ours
reads `.\Attachments.cpp` (`progress/data-known.txt`). A `#line` would only
change the string; the build itself would differ, because VC6's output for
these DLLs depends on the path a file is compiled by.

The experiment: map a Wine drive `G:` to a directory holding the sources
at `Nolf\NolfCode1.03\Source\...`, compile the game DLLs' files there by
their full paths, as the IDE did, and compare. If it works, the
`Attachments.cpp` entries leave `data-known.txt`. It may also move some
of the game DLLs' near matches, so run `make progress` before and after
and compare the near-match counts as well as the identical ones. Nothing
has been tried yet.


## The test

Only the release build is scored. The debug and modern builds
(`NOLFSRC_DEBUG`, `NOLFSRC_MODERN`; `BUILDING.md`, "Variant builds") are built
in copies under `out/build-<variant>/` that the scoring never reads, and
`build/env.sh` refuses their flags anywhere else, so code behind either define
cannot reach what is scored -- and the modern build, which fixes retail bugs on
purpose, could never pass this test.


`progress/identical.txt` lists every function verified identical, and
`progress/summary.json` the figures they add up to. `make progress`
(`build/progress.sh check`) re-scores the current build and fails if:

* a function listed in `identical.txt` is no longer identical,
* a target's identical byte count went down,
* `README.md`'s table is not the one `summary.json` renders, or
* the data check finds a text difference `data-known.txt` does not list
  (below), or
* `tools/fpconst.py` finds a float or double that one of our placed
  functions loads (through an address the byte match masks) differing
  from the one retail loads there: a masked match is identical only up to
  the constants it reads, and two of `wm_SetObjectDims`' thresholds were
  once wrong behind an `identical`.

Before anything is scored, the retail images cached in `out/reference/`
are checked against `progress/reference.sha256` (and extracted again from
the installed game if they differ): what is scored is always the retail
release, not whatever the cache holds.

Newly identical functions pass, with a reminder to record them:
`make progress-update` rewrites `identical.txt`, `summary.json` and the
README table, so the improvement is locked in from then on. It refuses a run
that lost a recorded function unless `ACCEPT_LOSSES=1` is given (when a
change rightly gives one up -- say, a function was matched by coincidence),
and a run without the libraries, since the record must cover every target.

`make progress COMPLETE=1` also fails unless every function of every target
is identical, and nothing is unwritten or unplaced (`COVERED=1` alone): the
finish line for the code. It is not whole-image identity, which
`tools/buildcmp.py` against the retail image is.

The scoring compiles nothing; it reads what the last `make` left in the
tree. Run `make` after changing the source, then `make progress`. It needs
Python 3 only, so inside `make shell` (or on a host with Python)
`build/progress.sh check` works directly.

## The libraries: `NOLF_SOURCE`

The seven lithshared libraries are scored against the prebuilt `.lib`s that
came with Monolith's *NOLF Source Code v1.003* release
(`LT2/lithshared/libs/Release/`), and the Debug build of RegMgr, which the
Game of the Year `CShell.dll` links, against the release's
`libs/Debug/RegMgr.lib`. They are in this repository, in
`reference/LT2/lithshared/libs/` (`reference/README.md` says what they are
and why the retail binaries are not enough), and `NOLF_SOURCE` points there
by default; set it to another copy of the release (the directory holding
`LT2/`) to use that instead.
They are checked against `shipped-libs.sha256`, and a mismatch is an error.

With `NOLF_SOURCE` empty, the engine images are still scored and checked,
and the libraries are skipped with a note; `make progress-update` needs them.

## Files

| File | What |
|---|---|
| `targets.py` | the binaries scored: each one's retail image and our build of it, its objects and libraries, its own code's address ranges (`app`; the C runtime above them is listed, not counted), third-party code inside them (`middleware`), and the rest the shared tools need to know about NOLF1 (`tools/progress.py`'s header has every key) |
| `identical.txt` | every function verified identical (`target<TAB>function`): the baseline the test holds the build to. Written by `make progress-update` |
| `summary.json` | per-target counts and bytes of the recorded run; the README table is rendered from it |
| `data-known.txt` | the text in our images' data known to differ from the retail images', each entry under its reason: the data check's list (`tools/datacheck.py`). `make progress` fails on a difference not in it; `make progress-update` drops the entries that no longer differ |
| `shipped-libs.sha256` | the source release's `.lib`s that are scored (paths relative to `libs/Release/`) |
| `reference.sha256` | the retail images `out/reference/` must hold to be scored |
| `corrections.txt` | the functions taken out of `identical.txt` when the scoring was corrected (2026-11-02), each with why: matches inside another identical function (the same bytes counted twice), and matches outside the image's own code |
| `*-screen-exclude.csv` | per image (`lithtech`, `server`, `d3d`, `nolfserv`, and for the game DLLs `cshell`, `object`): enumerated addresses examined by hand. A reason that starts `NOT A FUNCTION` or `NOT GAME CODE` removes the row from the count (an operand byte, a jump target inside another function, padding); any other reason only marks the address as `attempted` (not in the game DLLs, where only the removal applies) |
| `extent-splits.csv` | function starts the enumeration misses (a function hidden behind a thunk), with the evidence |
| `lithtech-decoded-map.csv` | `lithtech.exe` addresses a function of ours has been mapped to, marking them `attempted` |
| `*-enum-suspect.csv` | per image: enumerated rows judged wrong (`addr,severity,reason`); `bogus` rows are not functions, and `tools/uncovered.py`, `decoded.py` and `d3dwork.py` leave them off their worklists. Not read by the scoring |
| `decoded-extents.csv`, `decoded-moved.csv` | for `tools/decoded.py`: the true extents of large functions whose enumerated row is wrong, and `d3d.ren` reconstructions moved out of their `big<addr>.cpp` file |

The CSVs change what is counted, so a change to them is a change to the
denominator: give the evidence in the reason, as the existing rows do.

The per-image function and vtable tables the reconstruction tools read
(`out/reference/<image>-functions.csv`, `-vtables.csv`) are not kept here:
`build/reference.sh` derives them from the retail images.

Per run, in `out/progress/`: `functions.csv` (every retail function and its
status) and `summary.json`. `tools/progress.py --nears` lists the near
matches closest first, and `--todo` / `--attempted` / `--small` the rows of
that status.
