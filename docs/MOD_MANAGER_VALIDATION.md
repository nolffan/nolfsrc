# Mod manager contribution: scope and qualification

Validated on 2026-10-23. The contribution is based directly on upstream master
`79b19d03a9d6b98e9c6c9a6e74f260f173d8d06c`, freshly fetched for submission.
Framework source was selected from the verified development checkpoint
`67e6438c8b47fea8cabcf819a21b3ab49df215fa`. Its private development history is
not an ancestor added by this contribution.

## Source audit

Only the shared package manager, native UI/bootstrap, package build/install
tools, synthetic tests, runtime notices and documentation were transferred.
The optional campaign adapter, native gameplay overlay handling, custom physics
DLL launch environment, test-map launch mode, and three tests specific to those
excluded integrations were removed. Remaining tests use neutral synthetic
resource identifiers. The importer, dependency resolver, profile transaction
and recovery architecture were retained.

No weapons, ADS, suppressors, gore, ragdolls, dismemberment, character physics,
renderer changes, personal mod packages, game archives, saved games, imported
caches, test installations or compiled binaries are included. References to
protected weapon tables in the parser are rejection rules, not weapon features.
No gameplay extension module is bundled or imported. Frozen archive inspection
confirmed only the six shared framework modules and the helper entry point.
Their compiled filenames contain no development-machine paths.

All seven changed reconstructed client/launcher files match upstream exactly
after its `releasesrc.py` strips the modern/debug guards, using Git's canonical
line endings. No other engine or game source is modified. The layout recipe
applies with exact context to owned retail data and produces the tested menu
layout. The existing modern client source discovery builds the new folder.

## Automated qualification of this contribution

| Check | Result |
|---|---|
| `test_mod_manager.py` | 27 passed |
| `test_mod_manager_v2.py` | 63 passed, including 10 inherited repeats |
| `test_mod_discovery.py` | 25 passed, 2 host-dependent skips |
| `test_bundled_mod_helper.py`, final packaged executable | 11 passed |
| Modern client and server compile/link, pinned existing VC6 tools | Passed; 211 client objects, clean upstream server sources |
| Modern launcher compile/link and owned resource-payload comparison | Passed; 22 resource payloads unchanged |
| Non-modern launcher compilation | Passed |
| Stripped reconstructed source comparison | 7/7 identical to upstream |
| Native layout recipe application | Passed |
| Protected owned clean-game inputs | 29/29 hashes unchanged |

This is **116 distinct passing automated cases** (126 passing executions,
10 inherited repeats), plus native build/launch checks. The three removed cases
tested private native gameplay overlays or a private test map. The two skips
concern creating case-colliding sibling filenames on Windows and filesystem
symlinks without privilege. ZIP-internal collision and link rejection pass.

Security cases include traversal, absolute paths, Windows device names/streams,
links, case collisions, malformed/corrupted ZIP and REZ, renamed executable
content, nested archives, decompression/entry limits, protected tables, hash
mismatches, dependency cycles, version failures, undeclared conflicts, forged
approvals, interrupted publication, insufficient space and corrupted libraries.
The Windows disc-installer test executes its actual checksum preflight: a Mods
inbox is accepted, while changed shipped files and unexpected files elsewhere
are still rejected. Read-only storage tests use real temporary Windows ACLs and
verify every input file remains unchanged.

## Actual packaged startup and restart

A new disposable installation used the clean client/server/launcher built from
this branch, owned retail resources and existing locally built modern engine and
middleware. This is not a fresh Windows VM or a complete new engine/toolchain
build. The source, archives, and owner installations were not modified.

The final component ran with only Windows System32 on PATH, invalid PYTHONHOME
and PYTHONPATH, an unrelated working directory, and installation paths containing
spaces. Its runtime receipt identified the DLL inside `ModManager/_internal`.
No system interpreter, source script, downloader or desktop manager was invoked.

The actual normal NOLF.exe launched the bundled helper and real lithtech.exe.
A dropped synthetic ZIP was validated, registered disabled and represented in
native menu metadata. An unsafe ZIP was rejected without stopping launch.
The production native restart bridge applied the enabled resource to a different
profile, then disabled it and returned to the exact original profile. The missing
helper path was checked with a compiled harness of the production bootstrap and
returned to original launcher behavior.

The initial upstream test exposed a fast-exit restart race: a terminated game
could retain a queryable process handle while its image-name query failed. The
helper now accepts that query failure only for an already-signalled process
handle. An unrelated live process is still rejected and is never terminated.
The final binary restart test deliberately retains a handle to an already-exited
owned game, proving this case for both enable and disable. The source regression
also verifies rejection of a real unrelated live process.

These automated tests write the same pending INI as the native menu and exercise
the same C++ bridge; they do not claim automated mouse clicks or visual inspection.
Earlier, the owner personally enabled the standard data-only main-menu texture
ZIP through the native menu, saw its unmistakable replacement, disabled it, and
saw the original return. That acceptance was on the verified development build;
the package and its cache remain private and are not in this contribution.

Historical qualification of that development build was 119 distinct automated
cases, five campaign-adapter integration checks and 624 protected-file matches.
Those records are preserved as prior evidence, not represented as new tests of
this extracted branch. The campaign adapter and its checks are deliberately
excluded. Broader gameplay and physical controller acceptance remain separate.

## Component artifact and reproduction

The final source-only helper build produced a 1,708,618-byte executable, a
23,877,470-byte complete component (994 files), and a 10,409,547-byte ZIP.

```text
NOLF-ModManager-Windows.zip SHA-256
80b669060e700bc5eeb7b8a5c83c6910f82e93403b70f5501ee69f387d378c6d
NolfModHelper.exe SHA-256
52b5aaa35ae38e540f288feddfec0176ef7fb496aea2e8cdaee2b942aa40cfad
```

The artifact is retained locally; source control contains the generator and
notices, not runtime binaries. [MOD_HELPER_RELEASE.md](MOD_HELPER_RELEASE.md)
provides build and test commands. PyInstaller outputs are not claimed to be
byte-identical across machines; the receipt makes inputs and outputs auditable.

## Review boundaries and limitations

Windows x64 hosting the x86 single-player game is qualified. Linux/Wine,
CrossOver, Steam Deck and Windows x86 hosts are not. Existing engine path limits,
64-mod/profile limits, ASCII menu metadata, one installed version per mod ID,
and deliberate migration for updated read-only releases remain. Named-profile
creation/renaming and detailed previews use the optional advanced interface;
ordinary discovery, activation and restart are native.

The archive validator is not a sandbox for the game's legacy asset decoders.
Content hashes detect corruption but do not authenticate authors. Existing mixed
source/middleware licensing is unchanged; runtime updates, signing and public
binary redistribution terms remain release-maintainer decisions. Automatic
nightly distribution integration is not enabled. No personal gameplay code or
asset is required to build or use the component.

For review, start with the archive rules and storage contract, then inspect the
native process boundary and guarded menu hooks, and run the synthetic suites.
This contribution is one complete optional framework; it does not require any
separate gameplay pull request. Credit to nolffan for NOLFSRC and the reverse
engineering foundation; the mod framework is an independent contribution.
