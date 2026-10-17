# src/modern/directmusic/ -- the music, without installing DirectMusic

The game's music is DirectMusic. Wine's own DirectMusic, which Proton uses
too, cannot play it: the game runs in silence apart from the sound effects.
The usual fix is to install Microsoft's DirectMusic into the Wine prefix with
winetricks, which means finding and changing the prefix of every machine the
game runs on (on a Steam Deck, Proton's prefix for a non-Steam game). The
modern build carries Microsoft's DirectMusic in its game directory instead,
so it plays with any prefix, Wine's or Proton's, as it comes (MODERN.md,
M-P4-12).

## How

* **The DLLs.** `build/all.sh` step 2 (`build/retailextract.py openruntime`)
  copies Microsoft's nine DirectMusic DLLs from
  `toolchains/directmusic-5.3.1.904/` (where they come from:
  `toolchains/README.md`; checked against `SHA256SUMS` there) into the game
  directory as `ms_<name>.dll`:
  `ms_dmband.dll`, `ms_dmcompos.dll`, `ms_dmime.dll`, `ms_dmloader.dll`,
  `ms_dmscript.dll`, `ms_dmstyle.dll`, `ms_dmsynth.dll`, `ms_dmusic.dll`,
  `ms_dswave.dll`. Microsoft's `dsound.dll` is not among them: the sound
  effects play through Wine's DirectSound (with Microsoft's they repeat).
* **The manifest.** `lithtech.exe.manifest`, installed beside
  `lithtech.exe`, is registration-free COM: it maps each DirectMusic class
  to its DLL in the game directory, so every `CoCreateInstance` of one --
  the engine's own, and the ones the DirectMusic DLLs make of each other --
  finds Microsoft's code without the registry. `lithtech.exe` has no
  manifest of its own, so Windows and Wine both read this one. It also
  declares the game DPI-aware (M-P3-23), which is not about DirectMusic but
  has to live in the same file.
* **The names.** A DLL called `dmime.dll` is loaded as Wine's builtin even
  from the game directory, unless the prefix overrides it. Wine has no
  builtin `ms_dmime.dll`, so it loads the file. None of the nine imports
  another by name, so the rename is safe.

Nothing in the engine changes: the release and modern `lithtech.exe` both
find DirectMusic through COM, and the manifest decides where. On Windows the
game directory's copies are used in place of the system's (the same DirectX
9.0c DirectMusic Windows ships).

`gm.dls` (the General MIDI instrument set) is not needed: the engine's own
loader (`src/engine/lithtech/dmloader.cpp`) cannot hand DirectMusic the
default GM collection, so the retail game never used it, and the music's
instruments are its own DLS banks (`Music/*/*.dls`). Checked under Wine:
with `gm.dls` in the prefix nothing opens it.

When `toolchains/directmusic-5.3.1.904/` is missing, the build says so and
installs neither the DLLs nor the manifest (a manifest naming missing files
would break DirectMusic on Windows); the music then needs winetricks as
before (`README.md`, "Play").

## Regenerating the manifest

The class list is what the DLLs register. To check it against a new set of
DLLs, register them in a scratch Wine prefix and read the registry back:

```sh
WINEPREFIX=/tmp/pfx wineboot -i
cp toolchains/directmusic-5.3.1.904/*.dll /tmp/pfx/drive_c/windows/system32/
for f in dmband dmcompos dmime dmloader dmscript dmstyle dmsynth dmusic dswave; do
    WINEPREFIX=/tmp/pfx WINEDLLOVERRIDES="$f=n" wine regsvr32 /s $f.dll
done
grep -A3 'CLSID\\\\{.*}\\\\InprocServer32\]' /tmp/pfx/system.reg
```

Every `InprocServer32` naming one of the nine is a `<comClass>` of that
DLL's `<file>` (threading model `Both` for all 48).
