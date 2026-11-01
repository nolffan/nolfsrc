## Download

| File | What it is |
|---|---|
| `nolfsrc-{{ARCHIVE}}.tar.gz` | **This is what you want**; the retail game with bug fixes and new features. |
| `nolfsrc-debug-{{ARCHIVE}}.tar.gz` | The same build with debug information. For bug reporting. |

**No game data is included.** A platform specific installer is included that copies the game's data from your own images of the two discs of the EU GOTY Edition.

Supported images:

- <http://redump.org/disc/42823/>
- <http://redump.org/disc/42561/>

## Install

**This is the whole game, not a patch.** Unpack it into a new, empty directory, not over an earlier release or another installation: the installer checks the directory against `SHA256SUMS` and stops if it holds anything else. Copy your old `Save` folder across to keep your saved games.

The full guide, is [here](https://github.com/{{REPO}}/blob/{{TAG}}/docs/INSTALL.md).

### Windows

Unpack the archive, drop both disc images onto `windows-install.bat`, then start `NOLF.exe`.

### Linux

There's no native Linux buid yet, but the game runs well under Wine or Proton; including on Steam Deck.

```sh
tar xzf nolfsrc-{{ARCHIVE}}.tar.gz
cd nolfsrc-{{ARCHIVE}}
./linux-install.sh /path/to/disc1.zip /path/to/disc2.zip
./linux-run.sh
```

#### Steam Deck

Add as a non-steam game, enable compatibility mode, and pick a Proton runtime.

### Mac

The game runs in CrossOver (or another Wine for macOS). Unpack the archive, put both disc images in its folder and double-click `mac-install.command`, then run `NOLF.exe` in a CrossOver bottle with Run Command. If macOS will not open the script, the [INSTALL.md guide](https://github.com/{{REPO}}/blob/{{TAG}}/docs/INSTALL.md#mac-crossover) says what to do.

## What the modern build adds

- Direct3D 9 (the default) and OpenGL renderers, with antialiasing; the original DirectX 7 renderer is included too.
- Wide screen support, window and borderless modes, and high frame rates at the right game speed.
- Xbox-style controllers, raw mouse input, and a crouch toggle.
- No proprietary libraries for movies and music.
- Fixes for dozens of the retail game's own bugs.

Every change since the retail game is in [`CHANGELOG.md`](https://github.com/{{REPO}}/blob/{{TAG}}/CHANGELOG.md).

## Testing

The game is a 32-bit Windows program. It is tested on Linux with Wine and Proton; Windows has had less testing. Please report problems in the [issues](https://github.com/{{REPO}}/issues), with the `modern.log` from the game directory attached.
