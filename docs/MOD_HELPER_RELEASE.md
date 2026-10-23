# Building and distributing the optional mod component

The manager uses the existing NOLFSRC modern architecture. No changes to the
renderer, server gameplay or retail reconstruction are needed. New client code
is in `src/modern/cshell/mod_manager.*`; `build/build.sh` already discovers modern
client `.cpp` files. Small `NOLFSRC_MODERN` hooks register the folder and route
mouse-wheel input. The layout is a recipe in
`src/modern/data/ATTRIBUTES/LAYOUT.txt.patch`, applied to player-owned data by the
existing modern build. `src/modern/launcher/mod_startup.h` and
`src/modern/mod_helper_launch.h` implement the native process bridge.

## Build and installation

Build NOLFSRC's modern game normally as described in [BUILDING.md](BUILDING.md).
Prepare the helper on a Windows build machine with Python and PyInstaller already
installed. The qualified toolchain is CPython 3.10.11 x64 and PyInstaller 6.21.0.
Those are developer tools; players do not install them. Use fresh output paths:

```text
python build/package_mod_helper.py --output <new-helper-output>
python build/install_resource_manager.py --game <complete-owned-modern-game> --root <new-managed-installation> --game-version 1.004 --launcher <rebuilt-modern-NOLF.exe> --helper-bundle <new-helper-output>/ModManager
```

The first command produces `NOLF-ModManager-Windows.zip`, containing only the
complete `ModManager/` component. It invokes no pip/download command. PyInstaller
packages the tested standard-library backend in a windowless onedir executable.
`bundle.json` records runtime versions, source hashes, payload hashes and size.
The optional advanced Tk interface lives in the same bundle. Packaging an x86
runtime or updating runtime versions requires fresh qualification and notices.

The second command is a trusted developer/release preparation step. It reads the
selected complete modern game, copies its resources once into a separate private
base and prepares the first empty configuration. It does not modify that input or
migrate its saves. It recognizes `nolfcmds.txt` ordering, including `custom`, or
the standard archive set; ambiguous extra archives need an explicit `--resources`
order. It has no dependency on a private directory, American edition or personal
gameplay adapter. A new game edition must be tested before claiming support.

```
NOLF.exe                    modern launcher with bootstrap V2
ModManager/
  NolfModHelper.exe          windowless helper; optional --gui
  _internal/                all bundled runtime files
  licenses/                 all runtime notices
  bundle.json               component integrity and provenance receipt
manager.json                portable trusted-input metadata
base/, library/             private trusted game resources/runtime
Mods/                       empty package inbox at distribution time
profiles/                   immutable combinations and their separate saves
```

Keep the complete component together. There is no interpreter PATH lookup or
launch-time download. The launcher uses the adjacent executable through
CreateProcess, with explicit quoted arguments and no shell. When the optional
component is absent, it retains the original launcher path. The helper clears
PyInstaller's DLL search directory before starting the x86 game. Profile-relative
resource mounts avoid unnecessary expansion of the engine's command-line limit.

The existing public-game archive/disc installer is not replaced. The component
ZIP is independently buildable and contains no retail assets. A managed tree
prepared from an owned game is a **private installation**, not a public release:
do not upload its base resources or treat it as input to the stock public archive
packager. A distributor may build the component from this source and integrate
the trusted initialization step after the existing owned-disc installation.
Automatic attachment/installation in NOLFSRC's nightly archives is not enabled
by this contribution; that release-policy decision stays with the maintainer.
Shipped component files must be included in any distributor checksum inventory.
The installer preflight permits user data under Mods while retaining checks of
every file listed in its manifest and unexpected files elsewhere.

Never package a played review installation. Test ZIPs, imported-library caches,
profile selections, saves and build outputs are not release inputs. Optional
example generators are invoked only by developers/tests and are not part of a
normal component build. No generated example ZIP or game binary is committed.

## Shared backend and storage

| Component | Responsibility |
|---|---|
| `nolf_mod_core.py` | Bounded ZIP/REZ validation, paths, hashes, manifest/version rules, dependency ordering, conflict tokens, locks and storage checks |
| `nolf_mod_profiles.py` | Named snapshots, exact pins, ordering, approval retention and asset-free profile manifests |
| `nolf_mod_manager.py` | Private imports, transactional composition, native-menu metadata, isolated saves, launch and restart |
| `nolf_mod_discovery.py` | Inbox scanning through the same parser, disabled registration, native approvals and last-good fallback |
| `nolf_mod_portable.py` | Bundled component installation, portable metadata, app-local runtime and read-only user storage |
| `nolf_mod_helper.py` | Windowless entry point and retained error diagnostics |
| `nolf_mod_ui.py` | Optional previews, file drops and advanced profile operations |

Dependencies and explicit override edges constrain preferred ordering, with
stable IDs breaking ties. Resources are materialized into one final overlay, so
LithTech sees exactly one winning imported resource at each path. Trusted client
and layout layers cannot be replaced through imports.

Imports and profiles are constructed under `.staging-*`, validated and promoted
by same-volume rename. `active.json` is published only after a complete profile
is available. OS locks exclude simultaneous backend writers and release on process
death. Interrupted stages are quarantined in `recovery/` on the next operation.
The last-good configuration is validated before fallback; corrupted resources
are refused. This is application/filesystem crash recovery, not a guarantee of
power-loss durability on every filesystem.

The trusted base is copied once and reused; mutable saves/runtime files are
private copies. No hardlinks, symlinks or junctions propagate changes into the
input game. Hashes protect trusted resources and cached imports; space checks
precede expensive copies. Read-only installations initialize a staged private
copy under the user's local application data and keep using it consistently.
A changed trusted release requires explicit migration, preserving existing saves.
ANSI/MAX_PATH engine constraints remain; paths containing spaces are tested.

## Licensing

Read [LICENSE-NOTES.md](LICENSE-NOTES.md) and `src/MONOLITH-README.txt`.
This independently written manager does not change the repository's existing
source licensing or grant rights to retail assets. Attribution belongs to nolffan
for NOLFSRC and its reconstruction foundation.

The helper bundles CPython and standard-library notices, Tcl/Tk, the PyInstaller
bootloader (with its distribution exception), and app-local Microsoft runtime
DLLs from the official CPython installation. Notices are copied from the installed
runtime and the versioned source notices in `build/mod_helper_licenses/`.
`SOURCES.json` identifies their upstream origins and hashes. Review Microsoft
runtime redistribution terms, runtime security updates and signing as part of a
public binary release. No third-party runtime binary is added to this source PR.

## Developer checks

```text
python build/test_mod_manager.py
python build/test_mod_manager_v2.py
python build/test_mod_discovery.py
python build/test_bundled_mod_helper.py --bundle <helper-output>/ModManager
python build/test_mod_helper_launch.py --root <disposable-managed-game> --tools-root <initialized-NOLFSRC-toolchain-checkout> --output <new-validation-directory>
```

The final command starts real game processes and stops only verified processes it
owns. It writes test packages and selections: use a disposable installation, never
a player's active configuration. It exercises the production native restart
bridge without taking control of the keyboard/mouse. It does not claim human
visual acceptance. No retail data is downloaded by these tests.
