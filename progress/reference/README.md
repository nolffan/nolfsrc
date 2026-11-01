# Monolith's prebuilt libraries

`LT2/lithshared/libs/` here is the `libs/` directory of Monolith's *No One
Lives Forever Source Code v1.003* release (May 2001), as it came: the shared
"lithshared" libraries the game and its tools link -- StdLith, CryptMgr,
RegMgr, MFCStub, ButeMgr (`ButeMgrNoMFC`), the GUI and font managers
(`ltguimgr_LT2`, `LithFontMgr_LT2`) and the two GameSpy managers -- each in
its Release and its Debug build. Monolith shipped them as binaries only; the
source in `src/lithshared/` is ours, reconstructed or adapted
(`docs/LICENSE_NOTES.md`). Taken from a public copy of the release
(github.com/osgcc/no-one-lives-forever); `SHA256SUMS` lists every file.
They are under the release's licence, as `src/game/` is
(`src/MONOLITH-README.txt`, section IV).

Nothing is built from them: they are what `make progress` measures our
rebuilt libraries against. This directory is the default `NOLF_SOURCE`
(`progress/README.md`), so `make progress` and `make progress-update` need
nothing else.

## Why the game's binaries are not enough

Most of the scoring compares our build with the retail Game of the Year
binaries (`lithtech.exe`, `CShell.dll`, `Object.lto`, ...), and the
lithshared code the game uses is checked there: every library function
linked into a retail binary is compared with that binary's copy.

But a static library holds more than any one program takes from it. The
linker pulls in a library's object file only when the program uses
something defined in it, and within an object it drops the functions
nothing refers to (functions compiled one per section, `/Gy`, and linked
with `/OPT:REF`). So much of these libraries -- a GUI control no menu of the
game uses, a string helper nothing calls -- is in no retail binary, and the
only thing to compare it with is the library itself. Scoring each of our
libraries object by object against these (the library targets in
`progress/targets.py`, `LIBS`) covers that code too, and checks every
function before the linker has had a say.

Which are scored: the seven Release libraries the game links, and the Debug
`RegMgr.lib` (the Game of the Year `CShell.dll` linked RegMgr's Debug
build); `progress/shipped-libs.sha256` lists them, and the scoring refuses
a copy that does not match. The GameSpy libraries and the other Debug builds
are kept for completeness: our GameSpy code is a stand-in
(`src/lithshared/gamespy/`), and nothing builds the Debug libraries.
