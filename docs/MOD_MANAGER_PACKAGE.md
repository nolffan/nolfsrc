# Data-only mod packages (manager 2)

ZIPs may contain resource directories directly, beneath `payload/`, or inside
one unambiguous enclosing directory. Data-only REZ v1 files are expanded and
validated; they are never mounted as opaque imported archives. Multiple possible
roots require repackaging. An optional root `nolf-mod.json` describes the package.

```json
{
  "schema": 2,
  "id": "example-palette",
  "name": "Example palette",
  "author": "Example author",
  "version": "1.0.0",
  "description": "An original demonstration texture.",
  "category": "Textures",
  "manager_version": ">=2.0,<3",
  "requires": [{"id": "example-base", "version": ">=1.0,<2"}],
  "conflicts": ["alternative-palette"],
  "overrides": [{"id": "example-base", "paths": ["TEX/EXAMPLE.DTX"]}],
  "preview": {"image": "INTERFACE/EXAMPLE.PCX", "caption": "Original demonstration art"},
  "files": {
    "TEX/EXAMPLE.DTX": "<SHA-256>",
    "INTERFACE/EXAMPLE.PCX": "<SHA-256>"
  }
}
```

`files` must enumerate the exact expanded resources, including those inside REZ.
Preview metadata names a declared local resource, never a URL; the first UI shows
metadata and resource contents, not an automatic PCX/DTX thumbnail decoder.
Schema 1 packages remain accepted. Without a manifest the filename supplies the display name, version is `local`,
and the stable ID is derived from expanded resource hashes. Renaming/repacking
identical resources therefore does not create another mod.

Versions support exact matches (`1.0`, `==1.0`, `!=1.0`) and comma-separated
numeric comparisons (`>=1.2,<2`). Numeric components compare as integers, with
missing components treated as zero. Exact nonnumeric versions are supported;
prerelease ordering, wildcard ranges and full SemVer expressions are not.
`game_version` is optional; a requirement cannot pass when the installation's
version is unknown. The manager does not infer a game edition from a filename.

Dependencies are ordered before dependents; overridden packages precede their
replacement. Remaining choices use the saved preferred order and stable ID order.
Cycles and absent/incompatible requirements fail with an explanation. Conflicting
resources require both a package declaration and the player's approval, bound to
package fingerprints. An imported configuration never grants that approval.

Resource files are materialized into one final overlay in the resolved order.
Thus there is one winner per resource path before LithTech mounts it. The trusted
UI compatibility layers still mount afterward; imported packages cannot
replace their native code or managed layout/gameplay tables.

Limits: 64 installed mods, 4,096 entries/resources, 256 MiB expanded data, bounded
paths and metadata, ZIP stored/deflate only. The importer rejects traversal,
absolute/drive paths, streams, Windows device names, links, encrypted entries,
case collisions, file/directory collisions, nested archives, self-extractors,
executable signatures, unsupported extensions, malformed REZ bounds/loops/overlaps
and mismatched hashes. Integrity hashes detect changes; they do not authenticate
an author. Game resource decoder robustness remains the engine's responsibility.

Packages do not run install scripts or modify the original installation. Native
code extensions, arbitrary weapon tables and campaign scripting require separate
reviewed integration. This manager is an unofficial add-on and is not made or
supported by Monolith Productions.
