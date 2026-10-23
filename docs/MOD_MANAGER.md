# Native ZIP/REZ mod manager

The optional modern-build mod manager installs supported **data-only** mods through
NOLF's own menu. It is an independently developed enhancement built on nolffan's
NOLFSRC reconstruction. It is not made or supported by Monolith Productions.

## Playing with mods

Use a modern distribution prepared with the bundled mod component:

1. Put a supported `.zip` or `.rez` file in `Mods` beside `NOLF.exe`.
2. Launch `NOLF.exe` normally and open **Mods**.
3. Select the package and turn it **On**. New imports always start **Off**.
4. Choose **Save choices**, then **Restart game**.

Turn it **Off** and restart to restore the resources from your other selected
mods and the base game. There is no manual extraction, asset replacement, Python
installation, dependency installation, or external manager window in this flow.
The complete `ModManager` directory must remain beside the modern launcher.
An ordinary retail installation needs the modern build and trusted managed-library
setup first; dropping the helper executable alone into it is insufficient.

The menu uses NOLF's fonts, colors, help panel, toggles and navigation. Mouse
wheel, keyboard and the existing controller navigation path remain available.
Hover/select a package for its description; F1 opens its metadata and active
versus pending status. Changing choices does not change a running game's assets.
Restarting from gameplay asks about unsaved progress first.

Rejected packages and failed activation appear under **Package notices**. An
invalid inbox package does not prevent other packages or the previous validated
configuration from launching. A failed selection remains pending for correction.
The manager never reports that a failed selection was activated.

## Dependencies, conflicts and profiles

Dependencies are enabled with their dependents. Missing or incompatible versions,
cycles and undeclared resource conflicts block activation. **Review overrides**
asks for approval of explicitly declared replacements. Each approval is bound to
the exact package contents and resource path; importing a profile cannot grant it.

**Profile** selects a named saved configuration. Classic NOLF has no imported
mods enabled; Enhanced NOLF and Custom provide starting configurations, with no
gameplay enhancements bundled. The optional **Advanced mod manager** provides
profile creation, naming, duplication, deletion, preferred ordering, manifest
export/import, detailed previews and troubleshooting. Both interfaces use the
same library and validation code.

Each resolved mod combination owns a separate save directory. Returning to that
combination restores access to its saves. Renaming/deleting a named configuration
does not remove its combinations or saves. Original installation saves are never
copied over, deleted or modified.

## Removing or updating packages

`Mods` is an inbox, not a resource mount. Its immediate ZIP/REZ files are scanned
at startup; files inside imported-library directories are not scanned as packages.
Validated resources are copied into the private library. Removing or renaming the
inbox ZIP therefore does not uninstall the cached mod or damage profiles/saves.
Repeated archives are identified by content hashes and stable manifest IDs.

Changed content under an already installed ID is reported and the pinned copy is
retained. Currently one installed version per ID is supported. Keep a distinct
ID for a separately installed edition; there is no automatic updater/uninstaller.
Do not delete cached library directories used by existing profiles.

Dropping a package into Mods performs the same validation as **Import ZIP/REZ** in
the optional advanced interface. The advanced path adds a preview and lets you
import a file from another folder. Both leave new imports disabled.

## Scope and limits

This framework includes no weapons, ADS, suppressors, gore, ragdolls,
dismemberment or other personal gameplay modifications. Packages cannot replace
DLLs, executables, scripts or protected game tables. See
[MOD_MANAGER_PACKAGE.md](MOD_MANAGER_PACKAGE.md) for package rules.

The qualified runtime is Windows x64 with the x86 game. Linux/Wine, Steam Deck,
CrossOver and Windows x86 hosts remain unqualified. Native menus support up to
64 mods/profiles and printable ASCII metadata. Physical-controller acceptance
and broad edition/campaign testing remain separate from automated checks.
Read-only installs use private local application data and require enough space
for one safe copy; they never link writes back into the installation.

Developers: [build and storage contract](MOD_HELPER_RELEASE.md),
[validation and provenance](MOD_MANAGER_VALIDATION.md).
