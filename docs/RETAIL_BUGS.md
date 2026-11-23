# Bugs in the retail game

Bugs in *No One Lives Forever* (GOTY Edition, version 1.003) as it shipped,
found while reconstructing it. Each is a mistake in the code that a source
change would fix.

The release build reproduces every one of them, on purpose: compiling to the
retail machine code means compiling its mistakes too. They are fixed in the
**modern build** (`make modern`, the `NOLFSRC_MODERN` define; see
`BUILDING.md`, "Variant builds"): each fix goes behind `#ifdef
NOLFSRC_MODERN`, with the retail code kept in the `#else`, so the release
build is unchanged and still matches.

An entry that is fixed says so under its heading, with the file the fix is
in.

**Fixed so far** (in the modern build):

* [`CSoundMgr::GetFilterIndex`: the name comparison is inverted](#csoundmgrgetfilterindex-the-name-comparison-is-inverted)
* [The hit-sphere debug view loads a model that does not exist](#the-hit-sphere-debug-view-loads-a-model-that-does-not-exist)
* [The ammo HUD surface is sized for a 10000-shot clip](#the-ammo-hud-surface-is-sized-for-a-10000-shot-clip)
* [`GetMovementData` reads one past its table, and the word there is a NaN](#getmovementdata-reads-one-past-its-table-and-the-word-there-is-a-nan)
* [Doors and switches with no move direction normalise a zero vector](#doors-and-switches-with-no-move-direction-normalise-a-zero-vector)
* [`FACEOBJECT` normalises a zero vector](#faceobject-normalises-a-zero-vector)
* [`CCharacterFX::InitLocalPlayer` indexes the style table unchecked](#ccharacterfxinitlocalplayer-indexes-the-style-table-unchecked)
* [Player setup copies the skin and head settings into a 32-byte buffer](#player-setup-copies-the-skin-and-head-settings-into-a-32-byte-buffer)
* [Player names and chat are used as printf format strings](#player-names-and-chat-are-used-as-printf-format-strings)
* [The server browser indexes an option table with a value from the network](#the-server-browser-indexes-an-option-table-with-a-value-from-the-network)
* [`ar_FindFile` closes a find handle it never opened](#ar_findfile-closes-a-find-handle-it-never-opened)
* [`ci_GetRenderModes` never closes its find handle](#ci_getrendermodes-never-closes-its-find-handle)
* [`streamsim_Open` leaks the file when the stream cannot be allocated](#streamsim_open-leaks-the-file-when-the-stream-cannot-be-allocated)
* [`InitLevel`'s trace misspells a parameter](#initlevels-trace-misspells-a-parameter)
* [`CLTDirectMusicMgr::InitLevel` leaks the path it built when the control file will not open](#cltdirectmusicmgrinitlevel-leaks-the-path-it-built-when-the-control-file-will-not-open)
* [`CMemStream::Read`'s debug line reads a missing argument](#cmemstreamreads-debug-line-reads-a-missing-argument)
* [`ci_SetLightRadius` does not check its object handle](#ci_setlightradius-does-not-check-its-object-handle)
* [`ci_GetSurfaceDims` writes through unchecked pointers](#ci_getsurfacedims-writes-through-unchecked-pointers)
* [`sm_WriteObjectFiles` dereferences the first file identifier unchecked](#sm_writeobjectfiles-dereferences-the-first-file-identifier-unchecked)
* [Error messages that name the wrong function](#error-messages-that-name-the-wrong-function)
* [`CSoundMgr::GetReverbProperties`: damping is guarded by the wrong flag](#csoundmgrgetreverbproperties-damping-is-guarded-by-the-wrong-flag)
* [`CObjBase::InitFields` leaves `m_StandingLink` uninitialised](#cobjbaseinitfields-leaves-m_standinglink-uninitialised)
* [Level particle systems stop emitting at high frame rates](#level-particle-systems-stop-emitting-at-high-frame-rates)
* [Above 100 frames a second the game world runs fast](#above-100-frames-a-second-the-game-world-runs-fast)
* [`at_Advance` divides by the last keyframe's time](#at_advance-divides-by-the-last-keyframes-time)
* [Model shadows are switched off for good by the first cinematic](#model-shadows-are-switched-off-for-good-by-the-first-cinematic)
* [`StopSecondary` and `StopMotif` test the wrong object when given a name](#stopsecondary-and-stopmotif-test-the-wrong-object-when-given-a-name)
* [After a stop, the music does not start again at the same intensity](#after-a-stop-the-music-does-not-start-again-at-the-same-intensity)
* [`ci_GetModelNodeName` refuses node 0 and does not terminate](#ci_getmodelnodename-refuses-node-0-and-does-not-terminate)
* [`se_ModelHashCleanup` uses a global instead of its argument, and leaks](#se_modelhashcleanup-uses-a-global-instead-of-its-argument-and-leaks)
* [`ws_AddClippedContact` divides by a dot product that can be zero](#ws_addclippedcontact-divides-by-a-dot-product-that-can-be-zero)
* [The property-type guard normalises to a value that is also out of range](#the-property-type-guard-normalises-to-a-value-that-is-also-out-of-range)
* [The tree-path builder ignores the buffer length it is given](#the-tree-path-builder-ignores-the-buffer-length-it-is-given)
* [The object-DLL temp name: one copy unterminated, two unbounded](#the-object-dll-temp-name-one-copy-unterminated-two-unbounded)
* [`CBaseStream::ReadString` never reports a short read](#cbasestreamreadstring-never-reports-a-short-read)
* [`CBaseStream::WriteStream`: the source offset is ignored](#cbasestreamwritestream-the-source-offset-is-ignored)
* [`CString::Concat(LPCTSTR)`: appending leaves the length short](#cstringconcatlpctstr-appending-leaves-the-length-short)
* [`Model::LoadChildModels` reports success when it cannot size one table](#modelloadchildmodels-reports-success-when-it-cannot-size-one-table)
* [`ServerMessage::ReadData` and `WriteData` bound-check a truncated size](#servermessagereaddata-and-writedata-bound-check-a-truncated-size)
* [The fragment-record allocator passes a null to the list splice](#the-fragment-record-allocator-passes-a-null-to-the-list-splice)
* [Checks reported at the wrong debug level](#checks-reported-at-the-wrong-debug-level)
* [The value-returning readers quote the shutdown message](#the-value-returning-readers-quote-the-shutdown-message)
* [The compressed-vector codec does not round-trip](#the-compressed-vector-codec-does-not-round-trip)
* [The box-intersect test reports the wrong plane on three of six faces](#the-box-intersect-test-reports-the-wrong-plane-on-three-of-six-faces)
* [`LTMatrix::TransformPlane` never transforms the point it dots](#ltmatrixtransformplane-never-transforms-the-point-it-dots)
* [`w_IsPolyBlocking` has no bounds checks](#w_ispolyblocking-has-no-bounds-checks)
* [`wm_ResetGeometry`: wrong block centres, unguarded divisions, and a missing null check](#wm_resetgeometry-wrong-block-centres-unguarded-divisions-and-a-missing-null-check)
* [`ServerMessage::WriteData` clips the count, then grows the buffer anyway](#servermessagewritedata-clips-the-count-then-grows-the-buffer-anyway)
* [A server's join password is sent to anyone who asks](#a-servers-join-password-is-sent-to-anyone-who-asks)
* [The dedicated server's console prints text as a format](#the-dedicated-servers-console-prints-text-as-a-format)
* [The dedicated server's console line can lose its terminator](#the-dedicated-servers-console-line-can-lose-its-terminator)
* [The dedicated server fills the game's option table unbounded](#the-dedicated-server-fills-the-games-option-table-unbounded)
* [`-nowiz` is not seen as the last argument](#-nowiz-is-not-seen-as-the-last-argument)
* [`-NetLevel` and `-NetCALevel` set settings nothing reads](#-netlevel-and--netcalevel-set-settings-nothing-reads)
* [The server's setup wizard never closes its find handle](#the-servers-setup-wizard-never-closes-its-find-handle)
* [Stepping up onto a slope where it is not](#stepping-up-onto-a-slope-where-it-is-not)
* [On a scaled display the cursor is held to part of the screen](#on-a-scaled-display-the-cursor-is-held-to-part-of-the-screen)
* [Empty and placeholder sound names fill the console with errors](#empty-and-placeholder-sound-names-fill-the-console-with-errors)
* [M05S04's tunnel fans name their sound with a bar in place of a backslash](#m05s04s-tunnel-fans-name-their-sound-with-a-bar-in-place-of-a-backslash)
* [Each start leaves an empty file in the temp folder](#each-start-leaves-an-empty-file-in-the-temp-folder)
* [Twelve worlds use a music theme that is not in the game](#twelve-worlds-use-a-music-theme-that-is-not-in-the-game)
* [The default mouse buttons are bound by names that Windows can change](#the-default-mouse-buttons-are-bound-by-names-that-windows-can-change)
* [The console ends a command at some characters above 127](#the-console-ends-a-command-at-some-characters-above-127)

Every entry names the retail address it was read from, the binary, and the
file that reproduces it. An entry is listed only once its function is
byte-identical, or a known near match, so it describes what the retail code
does, not a guess. Addresses are as in the retail images
(`build/reference.sh` extracts them into `out/reference/`). The game DLLs
(`CShell.dll`, `Object.lto`) are built from Monolith's own source as it
shipped, so their entries name the function and the source file instead.

---

## Wrong results

### On a scaled display the cursor is held to part of the screen
`WinMain` (lithtech.exe) · `src/engine/lithtech/clientwin.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`): `WinMain` declares
> the process DPI-aware (per-monitor where Windows has it) before any
> window is made, so every size and position the game sees is in pixels.

The game declares nothing about DPI, so with Windows display scaling above
100% it is virtualised: window rectangles, `GetSystemMetrics`, the cursor
position and `ClipCursor` are all in scaled units (2560x1440 for a
3840x2160 screen at 150%), and Windows stretches the window to fit. The
game's own cursor then covers only the top-left 1/scale of the screen and
the system cursor is drawn larger than the game's. The modern build's
borderless window made it worse, as it is sized from the desktop's real
resolution (`EnumDisplaySettings`, in pixels) while the cursor is clipped to
it in scaled units.

### Level particle systems stop emitting at high frame rates
`CParticleSystemFX::Update` (CShell.dll) · `src/game/ClientShellDLL/ParticleSystemFX.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`): an update that owes
> less than one particle returns without restarting the clock, so the time
> adds up until one is owed.

Each update adds `floor(fParticlesPerSecond * dt)` particles, `dt` being
the time since the last update (capped at 1/15 s), and then restarts the
clock. When a frame is shorter than one particle's worth of time the
count rounds down to 0 every frame, and the emitter never emits: at 100
frames a second, every system slower than 100 particles a second goes
dark. M06S03's ambient underwater particles were never there with a fast
renderer.

### Above 100 frames a second the game world runs fast
`CServerMgr::Update` (lithtech.exe, server.dll) · `src/engine/server/smgr.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P1-23): a
> tick runs only once 10 ms have passed since the last one.

In single player the client runs the server once a frame, and each tick
takes the real time since the previous call, clamped to between 10 and 200
ms, as its step: `LTCLAMP(m_FrameTime, 0.01f, 0.2f)`. A frame shorter than
10 ms still advances the world by 10 ms, so above 100 frames a second game
time runs ahead of real time -- measured at 1.6 times at 159 frames a second
-- and with it everything on the server: AI, doors, moving platforms, the
cut-scenes' cameras and the timers that end their lines of dialogue. A
machine of 2000 never got near it; a fast renderer today does.

The animations are off at high frame rates too, the other way: client and
server advance them by `(int)(frame time * 1000)` milliseconds and drop
the rest, up to a millisecond a frame (`cs_UpdateAnimations`,
`clientmgr2.cpp`, and the sprite updates in `csvis.cpp`;
`sm_UpdateObjectTrackers`, `servermodel.cpp`), which at 300 frames a second
is a sixth of the time.

### Stepping up onto a slope where it is not
`ws_StairStep` (lithtech.exe 0x0041a0e0, and server.dll) · `src/engine/server/worldsim.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P5-20): a
> floor or a slope one can walk on lifts the feet straight up onto the
> highest point of it that is under them.

Before a player's move, the engine looks for floors the feet are moving
into and steps the move up onto them. It finds them by walking the world's
tree, which finds a polygon by its plane, and lifts the move by how far the
swept box's lowest corner is below that plane, along the plane's normal. It
never asks whether that corner is over the polygon, and the plane goes on
past the polygon's edges. Where the ground beyond an edge is lower than the
slope carried on -- the top of a ramp, a ridge between two slopes of
terrain -- a corner out there is below the plane, so the player is stepped
up onto the slope in mid-air. The lift along the normal is backwards, too,
off the slope, so the next move does not meet it: the player falls, gravity
adds up, and the next lift is bigger. Measured in the release build at a
ramp in M01S03 (a slope of 1 in 2, its top edge at y 53.8, the floor beyond
at 53): shoves of 6 to 16 units, lifting the player as high as y 72, and 44
frames in the air; walking a ridge up a hillside in M01S04, lifts of 4 to 9
units every few frames and 27 shoves back, the player in the air most of the
way. A stutter at the top of a ramp and up some terrain, at any frame rate.

### Snow ignores the size of its weather volume
`CWeatherFX::CreateSnow` (CShell.dll) · `src/game/ClientShellDLL/WeatherFX.cpp`

`CreateSnow` reads the flake rate for the volume's snow level
(`SnowNumLightFlakes` 100, `SnowNumNormalFlakes` 300 or `SnowNumHeavyFlakes`
700 a second, `CLIENTBUTES.TXT` `[Weather]`), scales it by the volume's size
against a 1000-unit cube into `fFlakesPerSec`, and then never uses it: the
particle system is created with the rate read from the attributes a second
time. Every snow volume snows at the same rate, however big it is. Rain
(`CreateRain`) uses its scaled rate, so a rain volume's density does follow
its size.

Not fixed: the snowy levels were made and tuned with it, and their volumes
are far from the 1000-unit cube the scaling assumes. Measured in the game,
M12S04's eight outdoor volumes are 45 to 548 times that cube, so with the
scaling every one would hit the particle system's cap of 5000 flakes a
second: about 250,000 flakes alive at once where the retail game has 2,000
to 3,000, a blizzard where Monolith placed light snow. The small volumes in
M13S01, M13S02 and M14S02, 0.003 to 0.06 times the cube, would fall to a
flake a second or less. Snow as the retail game draws it is what the levels
were designed to look like.

### The property-type guard normalises to a value that is also out of range
`0x100016be` (server.dll) · `src/engine/server/counter.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> an object DLL that declares a property type outside
> `PT_STRING`..`PT_ROTATION` is refused as an invalid object DLL
> (`src/engine/server/counter.cpp`). The release build keeps the rewrite to 8.

Walking a freshly loaded object DLL's class table, every `PropDef::m_PropType`
outside `PT_STRING`..`PT_ROTATION` is rewritten to 8, which is one past
`PT_ROTATION` and so not a valid type either. A level with a corrupt property
type is silently given a type nothing can interpret, rather than being
refused.

### `ar_FindFile` closes a find handle it never opened
`0x1000706a` (server.dll) · `src/engine/server/filemgr.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/filemgr.cpp` closes the handle only when `_findfirst`
> opened one. The release build keeps the retail close.

`_findclose` is called with the handle even when `_findfirst` returned -1:
the close sits after the `if`, not inside it.

### `CSoundMgr::GetFilterIndex`: the name comparison is inverted
`0x00482fd0` (lithtech.exe) · `src/engine/lithtech/clientsound.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientsound.cpp` tests `stricmp(...) == 0`. The
> release build keeps the retail test.

The function answers "which index is the filter called this?" by walking
Miles' filter enumeration, and the comparison is the wrong way round:

```c
if (stricmp(pFilter, pName))
    return LT_OK;      /* names DIFFER -> report found */

(*pIndex)++;           /* names MATCH  -> keep walking */
```

So the first filter whose name is *not* the one asked for is reported as
found, and a name that matches makes the walk carry on past it.
`CSoundMgr::GetFilterParamName` (`0x004830d0`) does the same search and gets
it right; this one is missing `== 0`.

### `CSoundMgr::GetReverbProperties`: damping is guarded by the wrong flag
`0x00482230` (lithtech.exe) · `src/engine/lithtech/clientsound.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientsound.cpp` tests `REVERBPARAM_DAMPING` (0x10) for
> the damping. The release build keeps the retail test.

`ReverbProperties::m_dwParams` says which fields the caller wants. The getter
tests bit 1 for the volume, 2 for the room type, 4 for the reflection time,
8 for the decay time, and 8 again for the damping, which is bit 0x10's.
`SetReverbProperties` (`0x00482010`) uses 0x10, so a caller can set the
damping but cannot read it back unless it also asks for the decay time.

### `CString::Concat(LPCTSTR)`: appending leaves the length short
`MFCStub.lib` (also in `ButeMgrNoMFC.lib`) · `src/lithshared/MFCStub/mfcs_string.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the length is set after the copy
> (`src/lithshared/MFCStub/mfcs_string.cpp`, the copy the game links). The
> release build keeps the retail code.

It expands the buffer, copies the new characters in and returns, without
calling `SetLength`. The stored length stays what `ExpandBuffer` carried over,
and `GetLength()` under-reports the string from then on.

### `CBaseStream::WriteStream`: the source offset is ignored
`0x10011402` (server.dll) · `src/engine/server/basestream.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> a start the caller gives is sought to before the copy; the default still
> copies from the source's current position, as every caller expects
> (`src/engine/server/basestream.cpp`). The release build keeps the retail
> code.

The function copies a range out of another stream, starting at `dwMin`.
`dwMin` is used to work out how many bytes to copy and for nothing else: the
body never seeks the source stream (no call through its `SeekTo` slot), so
the bytes are read from wherever the source happened to be. Every caller in
the game passes `WRITESTREAM_DEFAULT`, which is why it never shows.

### The compressed-vector codec does not round-trip
`0x10012d4e` writer, `0x10014830` reader (server.dll) ·
`src/engine/server/servermsg.cpp`, `src/engine/server/lmessage.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`): the reader divides
> by 2^18 - 1, the writer's own scale (`src/engine/server/lmessage.cpp`).
> Only the reader changes, so a vector from a retail writer decodes exactly
> too, and what the modern writer sends a retail reader is unchanged. The
> release build keeps the retail constant.

The writer scales each minor-axis fraction by `262143.0` (2^18 - 1); the
reader multiplies by `3.814697265625e-06`, which is 2^-18 (1/262144). They
are not reciprocals, so every decoded vector comes back short by one part in
262144 on both minor axes.

### `wm_ResetGeometry`: wrong block centres, unguarded divisions, and a missing null check
`0x1000a411` (server.dll) · `src/engine/server/worldmodels.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> a block with no vertices keeps a zero centre instead of dividing by zero,
> and the polygon half skips a null polygon or one with no vertices
> (`src/engine/server/worldmodels.cpp`). The block centre still weights a
> shared vertex once per polygon: that only moves the centre, and the radius
> pass reaches every vertex, so the sphere is still a bound. The release
> build keeps the retail code.

Three faults in one function:

* A block's centre is the sum of its vertices divided by a count that is
  summed over the block's polygons, not over its distinct vertices. A vertex
  shared by two polygons is added once and counted twice, so the centre is
  pulled toward the most-shared corners.
* That division (`0x1000a4ae`) is never checked. The loop above it skips null
  and vertex-less polygons, so a block with none that count divides 0 by 0;
  the radius loop then compares against a NaN, and the block ends up with a
  garbage centre and a radius of 5.0. The per-polygon division by
  `m_nVertices` (`0x1000a5bd`) is unguarded too.
* The block half checks each `m_ppPolies` entry for null (`0x1000a4df`); the
  polygon half dereferences the same array (`0x1000a562`) without checking.

### The box-intersect test reports the wrong plane on three of six faces
`0x1000f1f7` (server.dll) · `src/engine/server/objbank.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the three positive faces take their distance from the box's maximum
> (`src/engine/server/objbank.cpp`). The release build keeps the retail
> planes.

`w_IsectObjectShape` builds all six faces of the object's bounding box. The
three negative faces are right: the normal points along -axis and the distance
is the box's minimum, negated. The three positive faces set the normal to +1
along the axis and then take the distance from the minimum as well:

    1000f358:  fld1                     ; +1.0f, into the normal
    1000f366:  mov    0xf0(%ecx),%ecx   ; m_BBoxMin.x, not m_BBoxMax.x
    1000f640:  mov    %ecx,0xc(%eax)    ; m_Dist = min.x

(`0xf4` on the +Y face and `0xf8` on +Z are the same slip.) A hit on the far
side of any object comes back with a plane through the near face, displaced
by the width of the box, although the intersection point itself is right.
Anything that reflects or slides along the reported plane gets a surface in
the wrong place.

### `LTMatrix::TransformPlane` never transforms the point it dots
`0x1000f74e` (server.dll) · `src/engine/server/objbank.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the point on the plane goes through the object's transform before the dot
> product, as in the SDK's `ltmatrix.h` (`src/engine/server/objbank.cpp`).
> The release build keeps the retail code.

The plane transform inlined into `w_IsectObjectGeometry` is
`LTMatrix::TransformPlane` from a version of `ltmatrix.h` that is one line
shorter than the SDK's (`src/LT2/sdk/inc/ltmatrix.h`):

    startPt = thePlane.m_Normal * thePlane.m_Dist;
    MatVMul_3x3(&ret.m_Normal, this, &thePlane.m_Normal);
    MatVMul_InPlace_H(this, &startPt);        // <-- missing in the retail code
    ret.m_Dist = ret.m_Normal.Dot(startPt);

So the new distance is the transformed normal dotted with a point still in
the object's own space: the reported plane is right only when the transform
has no translation. (The source writes the shorter version out in place,
since calling the header would emit the missing line.)

### `Model::LoadChildModels` reports success when it cannot size one table
`0x1001c604` (server.dll) · `src/engine/server/commonmodel.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the failure goes to the shared exit with the recorded `LT_ERROR`, like
> every other (`src/engine/server/commonmodel.cpp`). The release build keeps
> the retail jump.

Every failure in the function goes to the exit that returns the `LT_ERROR`
recorded on entry, except one: when sizing the table at `+0x8c` fails, it
jumps to the success return instead:

    1001c6c3:  call   0x10019f29        ; SetSize2(...)
    1001c6c8:  test   %eax,%eax
    1001c6ca:  je     0x1001cb25        ; 0x1001cb25 is `xor %eax,%eax`

A model whose table cannot be allocated loads as `LT_OK` with none of its
pieces, nodes, child models or animations read.

### `ServerMessage::WriteData` clips the count, then grows the buffer anyway
`0x1002616a` (server.dll) · `src/engine/server/servermsg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/servermsg.cpp` keeps the clip (it is the packet-size
> cap) and reports the lost tail to `OutputDebugString`, the first eight
> times, so a message that outgrows a packet shows. The release build keeps
> the silent clip.

The byte count is clipped against `m_MaxDataLen` first, and the loop after it
grows the allocation until the write fits. A write longer than `m_MaxDataLen`
silently loses its tail, while the buffer is enlarged to a size that would
have held it.

The clip is the packet-size cap doing its job; the fault is that the loss is
silent. `WriteData` returns nothing, so telling the caller would mean
changing the writer's interface and every caller: the fix reports it
instead.

### `CBaseStream::ReadString` never reports a short read
`0x10011314` (server.dll) · `src/engine/server/basestream.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> a length that cannot be read is returned as the error it is
> (`src/engine/server/basestream.cpp`). The release build keeps the retail
> code.

The string is read in two parts, a two-byte length and then the body, and
only the body read's result is returned. A stream that runs out between the
two returns `LT_OK` with an empty string. (A caller that checks
`ErrorStatus` afterwards, as the SDK advises, still finds out.)

### The hit-sphere debug view loads a model that does not exist
Object.lto · `src/game/ObjectDLL/CharacterHitBox.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P2-11):
> the same file loads `364.abc` and scales it by twice the radius. The
> release build keeps the retail code.

`CCharacterHitBox::CreateNodeRadiusModels`, the debug view that shows each
model node's hit sphere, creates its spheres from
`SFX\Expl\Models\264.abc`; no archive has that file (the sphere is
`364.abc`), so the view shows nothing. It also scales the sphere by the
radius, where the model needs twice that to match the hit sphere. Both found
by NOLF Modernizer.

### `CLTDirectMusicMgr::Stop` discards the queue it was asked to wait for
`0x00440480` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp`

`Stop(LTDMEnactNextSegment)` means "stop when the current segment ends". The
enact type is honoured only for the primary segment, which is left to run out;
the pending queue is emptied at once, and the secondary segments and motifs
are stopped immediately, whatever was asked.

Not fixed: honouring the request means queueing a stop to run when the
segment ends, which the manager has no request type for, and the game only
ever calls `Stop` with the default enact type (`CMusic::Stop`).

### `StopSecondary` and `StopMotif` test the wrong object when given a name
`0x00440ae0`, `0x00440dc0` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> one walk serves both the by-name and the stop-everything case: each node is
> visited once, its next link read first, and only a node with a segment
> state is stopped, so no node can be found forever (`dm_StopPlayingNodes`).

Stopping everything asks each node `pNode->m_pSegState` (is it playing?);
stopping one by name asks `pNode->m_pSegment->m_pDMSegment` (is it loaded?)
instead. The by-name loop re-runs `FindByName` until nothing is found and
`StopNode` does nothing to a node that is not playing, so a node that is
loaded but not playing would be found forever: a hang. Nothing reaches that
state today (a node is only added once DirectMusic has returned a segment
state), so the wrong test is the only thing between the loop and the hang.

### After a stop, the music does not start again at the same intensity
`0x00440480` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp` (`CLTDirectMusicMgr::Stop`)

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P4-15,
> refs issue #68): `Stop` sets the current intensity to 0, as `InitLevel`
> does. The release build keeps the retail code.

`Stop` stops the performance and empties the queue, but it does not
change `m_nCurIntensity`. `ChangeIntensity` does nothing when the new
intensity is equal to `m_nCurIntensity` ("new intensity same as old
nothing to do"). Thus, after a stop, a request for the same intensity
does not start the music.

The game stops the music before it loads a level or a saved game
(`CInterfaceMgr::PreLoadingLevelState`), and the modern build's jukebox
stops it when the player leaves. To start the music again, `CMusic::Play`
calls `Play`, which asks for the initial intensity of the control file,
and then `ChangeIntensity`. When the intensity before the stop, the
initial intensity and the new intensity are the same, the manager refuses
both requests and the music stays silent until the next change of mood.
In the retail data, only the Game of the Year theme (`Music\GOTY`) has
an initial intensity that is not silent: intensity 2, "Ambience". Tested:
in `GOTY\M16S01`, with `LTDMConsoleOutput 4`, the console commands
`music i 2`, `music s x` and `music i 2` give `Stop` and then two
refusals.

### Model shadows are switched off for good by the first cinematic
`CGameClientShell::TurnOnAlternativeCamera`, `TurnOffAlternativeCamera` (CShell.dll) · `src/game/ClientShellDLL/GameClientShell.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-14):
> `TurnOffAlternativeCamera` and `FirstUpdate` set `MaxModelShadows` from
> the player's `DrawShadows` setting (`src/modern/cshell/p5_gameplay.cpp`,
> `P5_RESTORE_MODEL_SHADOWS`). This bug had been hiding a crash in our
> reconstructed `d3d.ren`'s shadow pass (Direct3D's `DrawPrimitive`
> declared without `__stdcall`), now fixed too.

Cinematics turn model shadows off: `TurnOnAlternativeCamera` saves
`MaxModelShadows` in `g_nCinSaveModelShadows`, sets it to 0, and
`TurnOffAlternativeCamera` writes the saved value back. But
`UpdateAlternativeCamera` calls `TurnOnAlternativeCamera` on every frame a
cinematic camera is live, so from the second frame on the value it saves is
its own 0, and the end of the cinematic restores 0. `TurnOffAlternativeCamera`
is also called by `ExitLevel` and `LoadGame` whether or not a cinematic ran,
and then writes the global's initial 0. Either way the variable is written
with `+`, so the 0 goes into `autoexec.cfg`: shadows stay off, in this and
later sessions, until the player sets them again in the display options.

### The ammo HUD surface is sized for a 10000-shot clip
`CPlayerStats::UpdateAmmoSurfaces` (CShell.dll) · `src/game/ClientShellDLL/PlayerStats.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-10): the surface
> is never made wider than the screen, and one that is gets made again
> (`P5_FitAmmoHUD`, `src/modern/cshell/p5_gameplay.cpp`).

The ammo bar's surface is made as wide as a full clip of ammo icons
(`nShotsPerClip * m_AmmoSz.x`) and only ever grows. The camera disabler's
`ShotsPerClip` in `ATTRIBUTES/WEAPONS.txt` is 10000, so selecting it once
creates a surface ten thousand icons wide, which every later weapon then
uses (cleared, drawn into and blitted, mostly off the screen's left edge)
until the HUD is rebuilt.

### A popup only closes with Escape
`CPopupText::OnKeyDown`, `CInterfaceMgr::OnCommandOn` (CShell.dll) · `src/game/ClientShellDLL/PopupText.cpp`, `InterfaceMgr.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-17): the key
> bound to Activate and the right mouse button close a popup too
> (`CInterfaceMgr::OnKeyDown`, `OnRButtonDown`).

A popup, such as the text of a picked-up intelligence item, is meant to close
with Activate as well as Escape: `OnCommandOn` closes it on
`COMMAND_ID_ACTIVATE`, and `OnKeyDown` has the test for the Activate key
commented out (`if (key == VK_ESCAPE)// || key == m_nKey)`). But showing a
popup pauses the game, which turns the input off, so the Activate command
never arrives, and Escape is the only way out.

### A server's join password is sent to anyone who asks
`0x100f4bb0` (Object.lto) · `src/game/ObjectDLL/MyGameSpyMgr.cpp`;
`0x1001cc90` (CShell.dll) · `src/game/ClientShellDLL/FolderJoin.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P6-10):
> a locked server answers queries with `\PasswordRequired\1` instead of the
> password (`MyGameSpyMgr.cpp`) and checks the password itself: the client
> appends it, masked, to its handshake reply (`GameClientShell.cpp`), and
> the server boots a client whose reply lacks it or carries the wrong one
> (`GameServerShell.cpp`). The join folders ask for it (the LAN one gains a
> Password field; it never asked), and `+connect` takes it from
> `NetJoinPassword`. The release build keeps the retail behaviour.

`CMyGameSpyMgr::OnRulesQuery` answers every GameSpy `\rules\` or
`\status\` query with `\Password\<the password>` when the server has
one, and the server never checks it: `CFolderJoin::JoinCurGame` compares
what the player typed with that value and refuses the join itself. Anyone
who can query the server (or the master server listing it) can read the
password, and a client that skips the check joins anyway.

A real check needs the server to see the password, which the 1.003
handshake does not carry. The fix adds it after the three values every
client sends, which a retail server never reads.

### `-nowiz` is not seen as the last argument
`0x00408380` (NolfServ.exe) · `src/engine/nolfserv/ServerDlg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-12):
> `CServerDlg::ParseCommandLine` reads `-nowiz` wherever it is, and only
> the options that take a value need one after them. The release build
> keeps the retail parser.

`NolfServ.exe -nowiz` is the obvious way to start the dedicated server
without its setup wizard, from the settings saved in `NetHost.txt`, and it
does not work: the parser stops as soon as the argument it looks at is the
last one, because it takes every option to have a value after it,
`-nowiz` included. `-nowiz` only counts with another argument behind it
(`-nowiz -NetPort 27888`, or `-nowiz` twice).

### `-NetLevel` and `-NetCALevel` set settings nothing reads
`0x00449550` (NolfServ.exe, the option table) · `src/engine/nolfserv/ServerDlg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-12):
> each `-NetLevel <world>` (or `-NetCALevel`) adds a world to the game
> type's list (`NetLevel0`, `NetLevel1`, ... and `NetNumLevels`), the first
> one replacing the saved list. The release build keeps the retail table.

The command line can set any of the server's 27 settings as `-<setting>
<value>`, and the table includes `-NetLevel` and `-NetCALevel`. They write
the settings `NetLevel` and `NetCALevel`, which nothing reads: the levels
are `NetLevel0`, `NetLevel1`, ... (and `NetCALevel0`, ...), counted by
`NetNumLevels` (`NetCANumLevels`). The level list can only be set by the
wizard, or by editing `NetHost.txt`.

### Empty and placeholder sound names fill the console with errors
Object.lto · `src/game/ObjectDLL/ServerSoundMgr.cpp`, `RotatingWorldModel.cpp`, `AISounds.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P5-22,
> issue #64): `CServerSoundMgr::PlaySound` ignores an empty sound name and
> does not print an error (`ServerSoundMgr.cpp`). `GetSound` gives "no
> sound" for the placeholder `"x"` (`AISounds.cpp`). A sound name that is
> not found is still reported. The release build keeps the retail code.

The console shows `ERROR in CServerSoundMgr::PlaySound() - Couldn't play
sound ''`, one time or more, when some levels start. The sound name is
empty. The game plays nothing in its place, thus the only effect is the
error lines.

The cause is level objects that read a sound property into a string and do
not check for an empty value. `RotatingWorldModel::ReadProp` makes a string
from `BusySound`, `SpinUpSound` and `SpinDownSound` when the property is
there, also when it is empty. `StartSound` only checks for no string, not
for an empty one, and plays it (for example, `SetNormalRotation` plays the
busy sound when the level starts). The engine cannot find a file with no
name, and `CServerSoundMgr::PlaySound` prints the error. M07S01 gives two of
these lines when it starts. The fix is in `PlaySound`, thus it also covers
other callers that give an empty name.

`ATTRIBUTES/ServerSnd.txt` also gives `"x"` as the sound for some character
events: `CombAggr0` and `CombDef0` for `[Partner]`, all events but `Death`
and `Pain` for `[Frogman]`, and all events for `[Animal]` (the dog's and
the shark's last step). The entry stops `GetSound` at the character's own
set, thus the character does not use the lines of its nationality or its
sex. But `"x"` is not a file: the game plays it, and the console shows
`Couldn't play sound 'x'`.

### M05S04's tunnel fans name their sound with a bar in place of a backslash
`WORLDS/M05S04.dat` (NOLF.REZ, level data) · `src/game/ObjectDLL/ServerSoundMgr.cpp`, `RotatingWorldModel.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P5-23;
> related to issue #64): `CServerSoundMgr::PlaySound`
> (`ServerSoundMgr.cpp`) plays a sound name that has `|` with `\` in
> place of each `|`. It uses a copy of the name, thus the caller's name
> does not change. A sound name that is still not found is reported. The
> release build keeps the retail code, and the level data does not change.

This is a mistake in the level data, not in the code. In M05S04 (the
fourth level of "A Tenuous Lead"), two `RotatingWorldModel` objects,
`RotatingWorldModel2` and `RotatingWorldModel3`, are the tunnel fans. Their
`BusySound` property is `Snd\Amb|tunnelfan2.wav`. The `|` is where the path
separator `\` was meant. The sound file that was meant,
`SND\AMB\TUNNELFAN2.wav` (104,400 bytes), is in NOLF2.REZ. The `SoundFX`
objects `fan0` to `fan5` near the fans use the correct name.

The two fans have `StartOn` set, thus each plays its busy sound when the
level starts. The engine does not find a file with that name, so the fans
are silent, and the console shows two lines:
`ERROR in CServerSoundMgr::PlaySound() - Couldn't play sound 'Snd\Amb|tunnelfan2.wav'`.
Tested: a run of the retail game on M05S04 shows the two lines at the
start. The other rotating objects of the 102 worlds have correct sound
names.

### Text-only dialogue lines try to play a voice file
`src/game/ObjectDLL/Character.cpp` (`CCharacter::PlayDialogue`), `src/game/ClientRes/ClientRes.rc` (`IDS_TEXT_DIALOGUE_9000` to `9017`)

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P5-25,
> refs issue #65): `PlayDialogue` does not play a voice file for a fixed
> list of text-only IDs (`IsTextOnlyDialogueId` in
> `src/game/Shared/SoundMgr.h`). It stops the current dialogue sound, as
> retail did, and shows the text. A voice file that is missing for any
> other ID is still reported. The release build keeps the retail code.

`CCharacter::PlayDialogue` does two things for each dialogue line. It plays
the voice file of the line's ID (`GetSoundFilenameFromId("Dialogue", id)`,
which is `Voice\<id>.wav`), and then it shows the ID's text in the dialogue
window. It does not know which lines have a voice.

Some lines are text only. `IDS_TEXT_DIALOGUE_9000` to `IDS_TEXT_DIALOGUE_9017`
(`ClientRes.rc`) are Cate's choices and her silent replies: `". . ."`,
`"(Question one.)"` to `"(Question eight.)"`, `"(Torture him.)"`,
`"(What now?)"`, `"(Which weapon would you like to practice with?)"` and
others. The worlds use them in the `Dialogue` and `Decisions` properties of
`CinematicTrigger` objects (M01S02, M01S03, M02S02, M03S01, M07S02, M07S03,
M13S02, T01S02, T01S03, T02S01, T03S01, T08S01). The weapon menu of T01S03
and T08S01 (`weaponchoice`) also gives four choices that are not dialogue:
4001, 4000 and 4008 (`IDS_WEAPON_2`, `IDS_WEAPON_1`, `IDS_WEAPON_9`, the
weapon names) and 37 (`IDS_NONE`, "None"). The player "says" the choice
that is picked, thus the game also plays its ID as a voice file.

No release of the game has a voice file for these IDs (we examined the
VOICE folder of 9 NOLF releases: EU, EU GOTY, USA, USA GOTY, German,
French, Spanish, Swedish and Italian). Thus the console shows two errors
for each such line, for example
`ERROR in CServerSoundMgr::PlaySound() - Couldn't play sound 'Voice\9017.wav'`
and the same from `CClientSoundMgr::PlaySound()`. The dialogue itself
works. Tested: in T01S03, in the modern build without the fix (this code
is retail), `trigger weaponchoice on` in the console, then the first
choice, gives four errors (`Voice\9017.wav` and `Voice\4001.wav`).

Not all missing voice files are of this type. Some lines with real text
have no voice file in any release, for example `IDS_DIALOGUE_13717` ("I
guess the blokes in the lab have something new to show you.", T01S03,
`guardconv2`). The modern build still reports these.

### Twelve worlds use a music theme that is not in the game
`WORLDS/T01S03.dat` and 11 other worlds (NOLF.REZ, NOLFGOTY.REZ, level data) · `0x0043fcd0` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp` (`CLTDirectMusicMgr::InitLevel`)

> **✅ FIXED in the modern build, the message only** (`NOLFSRC_MODERN`,
> MODERN.md M-P5-24): the error from `CLTDirectMusicMgr::InitLevel`
> (`clientmusic.cpp`) gives the path of the control file that it cannot
> read. It is still an error. These levels still have no level music, as
> in retail. The release build keeps the retail message, and the level
> data does not change.

This is a mistake in the level data. In 12 worlds, the `WorldProperties`
object has `MusicControlFile` = `Music\UrbanSpy\UrbanSpy.txt`. No release
of the game has a `Music\UrbanSpy` theme. It is not in the GOTY archives
(NOLF.REZ, NOLF2.REZ, nolfu003.rez, NOLFGOTY.REZ). It is also not in the
EU, USA, USA GOTY, German, French, Spanish, Swedish, Italian or Mac
releases. Thus there is nothing to restore.

| world | archive | where |
|---|---|---|
| `T01S03` | NOLF.REZ | "The Assignment", level 3 |
| `T03S01` | NOLF.REZ | "Care to Explain?", level 1 |
| `M08S02` | NOLF.REZ | "Safecracker", level 2 |
| `M11S02` | NOLF.REZ | "Low Earth Orbit", level 2 |
| `MULTI/ASSAULTMAP/01CONSTRUCTIONSITE_AM` | NOLF.REZ | multiplayer, assault |
| `MULTI/ASSAULTMAP/03MARRAKECH_AM` | NOLF.REZ | multiplayer, assault |
| `MULTI/ASSAULTMAP/04CITYINRUINS_AM` | NOLF.REZ | multiplayer, assault |
| `MULTI/ASSAULTMAP/06STORM_AM` | NOLF.REZ | multiplayer, assault |
| `MULTI/DEATHMATCH/001THEPAD_DM` | NOLF.REZ | multiplayer, deathmatch |
| `MULTI/DEATHMATCH/002REBAR_DM` | NOLF.REZ | multiplayer, deathmatch |
| `MULTI/DEATHMATCH/004LOTSABRICK_DM` | NOLF.REZ | multiplayer, deathmatch |
| `MULTI/DEATHMATCH/008INC2001_DM` | NOLFGOTY.REZ | multiplayer, deathmatch |

The paths are under `WORLDS/`. A later archive does not replace these
worlds. The other 90 worlds of the GOTY archives (102 in all) use a theme
that is in the game, or no theme. The list comes from a read of the object
list of each world, not from a search for the text.

When one of these levels starts, the server splits the property into the
directory `Music\UrbanSpy` and the file `UrbanSpy.txt`.
`CLTDirectMusicMgr::InitLevel` cannot read that file, and stops. The level
has no level music, and the console shows
`ERROR! Unable to read control file. (CLTDirectMusicMgr::InitLevel)`. The
message does not give the file, thus the cause is not clear.

### The default mouse buttons are bound by names that Windows can change
`0x00438140`, `0x00438470`, `0x00438ad0`, `0x00438d80` (lithtech.exe) · `src/engine/lithtech/clientinput.cpp` (`di_FindDeviceObject`, `cc_SaveInputBindings`), `diblit.cpp` (`di_AddDeviceObjectQ438`, `di_MatchObjectCallbackQ438`); `defctrls.cfg`, `defaults.cfg`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P1-33,
> issue #59): each mouse button has a fixed name by its number,
> `##button0` to `##button7` (`src/modern/engine/p1_mousebutton.cpp`). A
> binding finds the button by this name, by the name that DirectInput
> gives, or by the retail name `Button <N>`. The config save writes the
> fixed name, and the default controls use it. The release build keeps the
> retail code.

A binding names a device object (a key, an axis or a button) by its name.
The engine finds the object only by its name: `di_FindDeviceObject`
compares the name with the two names of each object that has a binding,
and `di_AddDeviceObject` compares it with the DirectInput name (`tszName`)
of each object of the device. A name that starts with `##` is different:
the engine finds that object by its type and position. The default
controls use such names for the keys (`##<code>`) and for the mouse axes
(`##x-axis`, `##y-axis`, and `##z-axis` for the wheel).

But the default controls (`defctrls.cfg`, `defaults.cfg`) bind the three
mouse buttons by their DirectInput names: `"Button 0"` (Fire), `"Button 1"`
(Activate) and `"Button 2"`. DirectInput gets these names from Windows. The
names are `Button 0` and so on with an English Windows, but Windows in
another language, or a mouse driver, can give other names. Then the button
bindings do not attach to a button, and the mouse buttons do nothing. The
config save (`cc_SaveInputBindings`, 0x00438d80) writes only the bindings
that are attached, thus `autoexec.cfg` loses the three lines when the game
stops. The mouse turns the view and the wheel operates, because their names
are fixed names.

The engine has fixed names for three buttons (`##Button 0` to
`##Button 2`, by type and data offset, 0x00438470), but the default
controls do not use them. Also, the config save writes the name that the
binding used, thus a binding made in Customize Controls is saved with the
DirectInput name. When that name has a character above 127, the binding
can also fail in Customize Controls (see the next entry).

### The console ends a command at some characters above 127
`0x0041b710` (lithtech.exe), `0x10005e24` (server.dll) · `src/engine/server/console.cpp` (`parse_Token`)

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, MODERN.md M-P1-33,
> issue #59): `parse_Token` gives the character to `iscntrl` as an
> `unsigned char`. The release build keeps the retail code.

The console parser tests each character of a command with `iscntrl(c)`,
and `c` is a signed `char`. Thus a character above 127 is a negative
number, and `iscntrl` uses it as an index into the type table of the C
library: it reads memory before the table. For some of these characters,
the bit for "control character" is set in that memory, and the parser ends
the command at the character. Which characters these are depends on the
data before the table in the program. In the file of the retail
lithtech.exe, 22 of the 128 characters have the bit, for example 0xDF
(German "ß" in code page 1252) and 0xE0 to 0xE2 (Russian "а" to "в" in code
page 1251). In the modern build before the fix, 41 characters have it, for
example 0xF3 (Spanish "ó"). The data can change while the program runs,
thus these lists are not certain.

Customize Controls binds a mouse button by its DirectInput name, with a
console command (`rangebind "Mouse" "<name>" ...`). When the name has one of
these characters, the command stops at that character. It then has too few
arguments, and it does nothing, without a message. The menu shows the new
binding, but the binding is not there: the button does nothing, and the
config save does not write it. Keys are not affected when they are bound,
because Customize Controls binds a key by its code (`##<code>`). But it
removes the old binding of a key by the key's name.

---

## Crashes and corruption: checks that are missing

### `GetMovementData` reads one past its table, and the word there is a NaN
`GetMovementData` (Object.lto) · `src/game/ObjectDLL/AIMovement.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-01): the time is
> clamped to 0..1 and the last sample is used without a neighbour
> (`P5_GetMovementData`, `src/modern/object/p5_fixes.cpp`).

AI dodges and cover moves are driven by tables of distances sampled over the
animation (`s_afRifleShuffleLeft` ... `s_afPistolCornerLeft`, each `cData+1`
floats). The lookup interpolates between `afData[int(fTime*cData)]` and
`afData[int(fTime*cData+1)]`, with `fTime` the animation's time over its
length. When the animation reaches its end, `fTime` is 1.0 and the second
index is `cData+1`, one past the table. Its weight is 0, which hides the read
for most tables (the next table's first value follows), but not for the last
one: in every retail `OBJECT.lto` the word after `s_afPistolCornerLeft` is
`0xffffffff`, a NaN, and 0 times a NaN is a NaN. A pistol-armed AI finishing a
cover move to the left is moved by a NaN distance and vanishes. (An animation
of length 0 makes `fTime` 0/0, a NaN, with the same result.)

### Doors and switches with no move direction normalise a zero vector
`Door::InitialUpdate`, `Door::CalculateNewPos` (Object.lto) · `src/game/ObjectDLL/Door.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-03): a zero
> direction is left zero, a door already at its destination arrives, and a
> door loaded from a retail save gets its NaN direction and open position
> repaired (`src/modern/object/p5_fixes.cpp`).

`InitialUpdate` normalises `MoveDir` with `VEC_NORM`, which multiplies by
`1/|v|`: a zero vector becomes `0 * inf`, NaN in every component, and the open
position computed from it is NaN too. `Switch` declares `MoveDir` with a
default of (0, 0, 0). Opening such a door moves it towards a NaN position
(`CalculateNewPos` takes its `else` branch: the x87 compare with a NaN is
unordered), and `vNewPos == m_vOpenPos` is never true, so it never reaches
the open state. A `Switch` still sends its trigger message (that happens in
`SetOpening`), which is why such switches seem to work. `CalculateNewPos`
also normalises the remaining distance without a check, so a door that has
arrived, or does not move, computes NaNs as well. The NaNs are saved with the
game and come back on load.

### `FACEOBJECT` normalises a zero vector
`CPlayerObj::ProcessCommand` (Object.lto) · `src/game/ObjectDLL/PlayerObj.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-04): with no
> horizontal direction the player's rotation is left alone.

Turning the player to face an object, the direction is flattened to the
horizontal and normalised with `VEC_NORM`. An object straight above or below
the player leaves a zero vector, which becomes NaN, and the player's
rotation is set from it.

### `CCharacterFX::InitLocalPlayer` indexes the style table unchecked
`CCharacterFX::InitLocalPlayer` (CShell.dll) · `src/game/ClientShellDLL/CharacterFX.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-06): an invalid
> style falls back to the default one.

The player's model style comes from the server in the character's create
struct and goes straight into `CModelButeMgr::GetStyleName`, which returns
`m_aStyles[eStyle].m_szName` with no range check, and into
`GetPlayerHeadOffset`. `eModelStyleInvalid` is 0xFF, so an invalid style
reads 255 entries into a heap array that holds a handful.

### Player setup copies the skin and head settings into a 32-byte buffer
`CFolderPlayer::OnFocus` (CShell.dll) · `src/game/ClientShellDLL/FolderPlayer.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P5-09): the
> settings are compared where they are, without a copy.

Leaving the multiplayer player setup, the folder checks for changes by
copying each console value into `char szTemp[32]` and comparing. The model
and name copies are bounded (`SAFE_STRCPY`, which truncates them, so a long
one always reads as changed); `NetPlayerSkin` and `NetPlayerHead` are copied
with `GetConsoleString`, a plain `strcpy`, and hold "display name,file name"
pairs that are usually longer than 31 characters. In the shipped build's
stack frame the overrun lands in space only another branch uses, so it does
no visible harm there; another frame layout crashes.

### `CObjBase::InitFields` leaves `m_StandingLink` uninitialised
`0x10024802` (server.dll) · `src/engine/server/objtypes.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/objtypes.cpp` ties the link off like the other four. The
> release build keeps the retail setup.

Of the five links the function sets up, `m_StandingLink` (`+0x114`) is the
only one given its `m_pData` and not tied off, so an object that has never
stood on anything carries a link whose `m_pPrev`/`m_pNext` are whatever the
object bank's memory held. (It also ties off the link at `+0x060` twice,
which is harmless.)

### The fragment-record allocator passes a null to the list splice
`0x10023495` (server.dll) · `src/engine/server/netmgr.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> a record that cannot be allocated is not appended, and the fragment is
> dropped as an invalid one would be (`src/engine/server/netmgr.cpp`). The
> release build keeps the retail code.

When the six-record cap is not reached the function does `new CNetFragRec`,
and on failure (`xor %edi,%edi` at `0x100234f2`) both paths still call
`LMessageList::Append` with the result. `Append` writes through its argument
either way (`0x10024311`, `0x100242f5`), so running out of memory here is a
write through null, not a dropped fragment.

### `ws_AddClippedContact` divides by a dot product that can be zero
`0x10004512` (server.dll) · `src/engine/server/worldsim.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> a move parallel to the contact plane is left as it is
> (`src/engine/server/worldsim.cpp`). The release build keeps the division.

The divisor reduces to the dot product of the move with the contact plane's
normal. A move exactly parallel to a contact plane divides by zero, and the
infinity or NaN is copied into the object's position.

### `w_IsPolyBlocking` has no bounds checks
`0x10001cad` (server.dll) · `src/engine/server/worldsim.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> a polygon of more than 40 vertices, or a clip that would run past the
> 200-entry pool or pointer list, is answered as blocking, the safe mistake
> for a polygon already inside the sphere test
> (`src/engine/server/worldsim.cpp`). The release build keeps the retail
> code.

The clip pool (200 vertices) and the working pointer list (200) are
allocated across all six clip rounds and never reset, and the input list
holds 40 pointers filled straight from `m_nVertices`. A polygon with more than
40 vertices, or a clip sequence producing more than 200 new vertices, writes
past the end of the stack frame (`0x10001d36`-`0x10001e6d`).

### `at_Advance` divides by the last keyframe's time
`0x100012b6` (server.dll) · `src/engine/server/modelanim.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/modelanim.cpp` wraps a zero-length animation to time 0.
> The release build keeps the retail division.

A looping animation past its end wraps with `m_Cur.m_Time %= nLength`, where
`nLength` is the last keyframe's time. The only guard is `nKeyFrames > 1`, so
an animation with several keyframes all at time 0 divides by zero. (The
blend-percentage division just before it, `0x10001320`, does check.)

### `sm_WriteObjectFiles` dereferences the first file identifier unchecked
`0x10029087` (server.dll) · `src/engine/server/servercommon.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/servercommon.cpp` sends `0xffff` for a missing first
> identifier, as for the other two. The release build keeps the retail read.

Of the three file identifiers it sends, the first is read through
`m_pSData->m_pFiles[0]->m_FileID` (`0x10029092`) with no null test; the other
two are tested and replaced by `0xffff` when missing. An object with no model
faults here.

### `ci_SetLightRadius` does not check its object handle
`0x0040bf30` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientinterface.cpp` returns on a null handle. The
> release build keeps the retail read.

Every other accessor in the client interface checks the handle for null; this
one reads the object type straight off it, so a null handle is a read of
address `0xbc`.

### `ci_GetSurfaceDims` writes through unchecked pointers
`0x0040cfd0` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientinterface.cpp` zeroes only the outputs it was
> given. The release build keeps the retail writes.

Given a surface, it checks both output pointers and fills in whichever were
asked for. Given a null surface, it zeroes both without checking either.

### `ci_GetModelNodeName` refuses node 0 and does not terminate
`0x004363f0` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> only the node-count bound refuses a node handle, and the copy is terminated
> (`src/engine/server/commonmodel.cpp`). The release build keeps the retail
> checks.

The node handle is tested against null before it is tested against the node
count, so node 0, a valid node, is refused as an invalid parameter. The copy
is `strncpy(pName, ..., maxLen - 1)` with nothing writing the terminator, so
a name of `maxLen - 1` characters or more comes back unterminated.

### The tree-path builder ignores the buffer length it is given
`0x100070b6` (server.dll) · `src/engine/server/filemgr.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the path is written with `_snprintf` bounded by `nOutLen`, always
> terminated, and one that does not fit is reported as a failure
> (`src/engine/server/filemgr.cpp`). The release build keeps the `sprintf`.

The fourth argument, `nOutLen`, is the size of the caller's buffer; the body
is `sprintf(pOutPath, "%s\\%s", pTree->m_szDir, pFilename)` and never reads
it, so a long path overruns the buffer (`cb_LoadServerObjects` passes 256
bytes).

### The object-DLL temp name: one copy unterminated, two unbounded
`0x1000e05b` (server.dll) · `src/engine/server/counter.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the `strncpy` copy is terminated, the temp-directory path is written with
> a bounded `_snprintf`, and the tree path goes through the bounded builder;
> a name that does not fit fails as a copy error would
> (`src/engine/server/counter.cpp`). The release build keeps the retail
> copies.

Extracting an object DLL to a temporary file, the `GetTempFileName` fallback
copies with `strncpy(pOutPath, sUnique, nOutLen - 1)` and nothing writes the
terminator, which `strncpy` does not add when the source fills the count. The
other two paths `sprintf` into the caller's buffer without any bound.

### `ServerMessage::ReadData` and `WriteData` bound-check a truncated size
`0x100260d9`, `0x1002616a` (server.dll) · `src/engine/server/servermsg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> both compare against the allocation's full 32-bit size
> (`src/engine/server/servermsg.cpp`). The release build keeps the 16-bit
> compares.

Both compare the position against `(uint16)m_Data.m_AllocSize`, a 16-bit
truncation of a 32-bit field (`0x1002619e`, `0x10026102`), while
`CPacket::WriteType` (`0x10013957`) compares the same field as a full 32-bit
value. Only the `0x1000`-byte cap on packets keeps it from mattering.

### Player names and chat are used as printf format strings
`0x100d7be0`, `0x100d6d40` (Object.lto) · `src/game/ObjectDLL/GameServerShell.cpp`;
`0x10103e10` (Object.lto) · `src/game/ObjectDLL/PlayerObj.cpp`;
`0x100343d0` (CShell.dll) · `src/game/ClientShellDLL/InterfaceMgr.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-01):
> `FilterFormatChars` (`src/game/Shared/CommonUtilities.cpp`) removes `%`
> from player names and chat on the server (`GameServerShell.cpp`,
> `PlayerObj.cpp`), from names and chat a client receives
> (`InterfaceMgr.cpp`, `src/modern/cshell/p6_multiplayer.cpp`), from the
> host's own session and player names (`FolderHost.cpp`, `FolderPlayer.cpp`)
> and from server and player names in the browser (`FolderJoin.cpp`). The
> release build keeps the retail code.

`CGameServerShell::OnMessage` passes the chat line a client sent straight
to `g_pLTServer->CPrint(szString)` as the format (`MID_PLAYER_MESSAGE`, the
team and ghost variants alike), then relays it to every client, whose
`CMessageMgr::AddLine` does `g_pLTClient->CPrint(pStr)` with it. Player
names travel the same way: `CInterfaceMgr::OnMessage` formats them into
the kill, score and join messages and prints the result with
`CPrint(GetStringData(hStr))`. A `%n` in a name or chat line writes
through whatever the stack holds, on the server and on every client: in
the release build, a client named `Retail%n%s` joining and chatting
`%n%n%n%n%s%s%s%s` crashed the host with a write access violation inside
the engine's formatter (tested with two copies of the game on one docker
network). This is the publicly documented LithTech format-string
vulnerability.

### The server browser indexes an option table with a value from the network
`0x1001e330` (CShell.dll) · `src/game/ClientShellDLL/FolderJoin.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-02):
> `CFolderJoin::UpdateOptions` treats a value outside the table as 0. The
> release build keeps the retail code.

For a toggle or cycle option, the value a server reports in its GameSpy
reply is used as an index, `pOpt->nStringId[nVal]`, into a five-entry
array (`SO_MAX_STRINGS`), and the string ID found there is loaded and
shown. Any server, or anything answering the browser's query, can make the
browser read past the array.

### The dedicated server's console prints text as a format
`0x00404cc0` (NolfServ.exe), called from `0x00405300` and `0x00406090` · `src/engine/nolfserv/ServerDlg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-14):
> `CServerDlg::UpdateUI` and `OnConsoleSend` pass their text with a `"%s"`
> format. The release build keeps the retail calls.

`CServerDlg::WriteConsoleString` is printf-style, and its two callers with
text from outside pass that text as the format: `UpdateUI`, every line of
the engine's and the game's console output (which carries player names),
and `OnConsoleSend`, the command the operator typed. A `%` in a line is read
as a conversion: `%s` and `%n` in a player's name read and write through
whatever the stack holds. (The same flaw as the game's own,
[Player names and chat are used as printf format strings](#player-names-and-chat-are-used-as-printf-format-strings),
on the dedicated server's side.)

### The dedicated server's console line can lose its terminator
`0x00404cc0` (NolfServ.exe) · `src/engine/nolfserv/ServerDlg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-14):
> the formatted line is cut to 497 characters, terminated, before the line
> end is added. The release build keeps the retail buffer handling.

`WriteConsoleString` formats into a 500-byte buffer with `_vsnprintf(str,
499, ...)`, which leaves the buffer without a terminating zero when the
text is 499 characters or more; the `strlen` that follows then runs past
it, and the `strcat` of `"\r\n"` writes past the buffer's end for a line
of 498 characters or more.

### The dedicated server fills the game's option table unbounded
`0x00404902` (NolfServ.exe, in `CServerDlg::DoHost`) · `src/engine/nolfserv/ServerDlg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-14):
> `DoHost` stops at the table's 20 entries. The release build keeps the
> retail loop.

`DoHost` copies one value per server option of
`Attributes\ServerOptions.txt` into the `NetGame` it hands the game,
whose `m_fOptions` has room for `MAX_GAME_OPTIONS` (20), with no check. The
retail file has 17 options; a modified one with more than 20 overwrites
the level count and the levels that follow the table.

---

### The loading screen draws from a second thread while the main thread uses the renderer
`CLoadingScreen::RunThread` (CShell.dll) · `src/game/ClientShellDLL/LoadingScreen.cpp`; `RenderDLLSetup` (d3d.ren, `0x10021410`) · `src/engine/d3d/w26_10020fd0_osoiqip.cpp`

> **Not fixed in `d3d7.ren`.** The modern build's own renderers,
> `d3d9.ren` (the default) and `opengl.ren`, let one thread in at a time
> (MODERN.md, M-P0-06). `d3d7.ren` is the retail renderer and still has
> the problem.

`CLoadingScreen::Resume` starts a thread that draws the loading screen
about ten times a second (`ClearScreen`, `Start3D`, the interface effects,
2D, `FlipScreen`) while the main thread loads the level. During the load
the main thread also calls the renderer: it binds textures, makes surfaces
and makes the world's render context. Nothing in the game, the engine or
`d3d.ren` stops the two threads from being in the renderer at the same
time, and `d3d.ren` sets its DirectDraw cooperative level without
`DDSCL_MULTITHREADED`. Thus the two threads can change the renderer's
state and call DirectDraw and Direct3D at the same time. The playtests of
the modern build's debug renderer show this at each level load.

## Leaks

### `ci_GetRenderModes` never closes its find handle
`0x0042de00` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientinterface.cpp` closes it after the walk. The
> release build keeps the leak.

It walks every `*.ren` beside the executable with `_findfirst`/`_findnext`
and has no `_findclose` on any path.

### `CLTDirectMusicMgr::InitLevel` leaks the path it built when the control file will not open
`0x0043fcd0` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientmusic.cpp` frees the path on the failure path
> too. The release build keeps the leak.

For a control-file name with no directory, the function allocates a full path
(`0x0043fde5`) and frees it only when the reader's `Init` succeeds
(`0x0043fecb`). The failure path returns at `0x0043fec8` without freeing it.

### `streamsim_Open` leaks the file when the stream cannot be allocated
`0x10035a8d` (server.dll) · `src/engine/server/streamsim.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/streamsim.cpp` closes the file when the stream cannot
> be allocated. The release build keeps the leak.

`fopen` succeeds, the stream object fails to allocate, and the function
returns null without closing the file.

### `se_ModelHashCleanup` uses a global instead of its argument, and leaks
`0x1002af75` (server.dll) · `src/engine/server/serverengine.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> the record is freed whether or not a manager is left
> (`src/engine/server/serverengine.cpp`). The hash is still reached through
> the global, which is null once the manager has gone, where the record's
> copy would dangle. The release build keeps the leak.

The callback exists to free a dependency record, using the manager it is
handed. It reads the global manager pointer (`0x10058bf0`) instead, and when
that is null the whole body is skipped, so the eight-byte record is lost.
(`se_LoadChildModel` stores the manager in the record, and nothing reads it.)

### The server's setup wizard never closes its find handle
`0x00409870` (NolfServ.exe, `CWizardDlg::CWizardDlg`) · `src/engine/nolfserv/WizardDlg.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P6-14):
> the search is closed once it has answered. The release build keeps the
> leak.

To decide whether to show its customization page, the wizard looks for a
`.rez` file in the `Custom` folder with `_findfirst`, and never calls
`_findclose` on the handle it gets back.

### Each start leaves an empty file in the temp folder
`0x0042e110` (lithtech.exe) · `src/engine/lithtech/clientdsi.cpp`;
`0x1000e05b` (server.dll) · `src/engine/server/counter.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`, M-P1-32):
> when the second copy fails, the function deletes the file that
> `GetTempFileName` made. The release build keeps the empty files.

The engine copies each game DLL out of the resource files into the Windows
temp folder, and loads it from there. If the copy to `<temp>\<name>`
fails, the function gets a new name from `GetTempFileName` and copies to
that name. `GetTempFileName` makes the file. If this second copy also
fails, the function returns the error and does not delete the file.

The client always asks for `cresl.dll`, and no resource file has it. Thus
each start of the game adds one empty file, `cre<hex>.tmp`, to the temp
folder. The server's function (`object.lto`, `sres.dll`) has the same
mistake, but its files are always there.

Not changed: a copy to a new name that succeeds also stays in the temp
folder after the game stops. The engine makes such a copy when a second
game runs at the same time, because the first game uses `<temp>\<name>`.

---

## Wrong diagnostics

Minor: these change what the engine reports, not what it does.

### Error messages that name the wrong function

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`): every one in the
> table reports its own name (`src/engine/lithtech/clientinterface.cpp`,
> `clientphysics.cpp`, `src/engine/server/servermessage.cpp`, and
> `commonmodel.cpp`'s twin of `ci_GetModelNodeName`), and
> `CPhysicsLT::SetObjectDims`'s first check uses the client's debug level.
> The release build keeps the retail names.

The engine's parameter checks report through a macro taking the function's
own name. These were written by copying a neighbour, and the name was never
changed:

| function | address | reports itself as |
|---|---|---|
| `ci_GetParticleTotalLifetime` | `0x0040b750` (lithtech.exe) | `CLTClient::GetParticleLifetime` |
| `ci_FreeServiceList` | `0x004081e0` (lithtech.exe) | `CLTClient::GetServiceList` |
| `ci_SetSurfaceAlpha` | `0x0040ecc0` (lithtech.exe) | `ILTClient::GetSurfaceAlpha` |
| `ci_UnoptimizeSurface` | `0x0040cdf0` (lithtech.exe) | `OptimizeSurface` |
| `ci_RenderObjects` | `0x00408f90` (lithtech.exe) | `RenderCamera`, at three of its four checks |
| `ci_GetModelNodeName` | `0x004363f0` (lithtech.exe) | `GetNextModelNode`, at both |
| `CPhysicsLT::SetObjectDims` | `0x00405000` (lithtech.exe) | `SPhysicsLT::SetObjectDims`, at the first check (which also uses the server's debug level, 2, where the next uses the client's 1) |
| `CLTServer::UnloadModel`, `UnloadTexture` | server.dll | `ThreadLoadTexture`, in both |

### Checks reported at the wrong debug level

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`): all three report
> at 1, like their siblings (`src/engine/lithtech/clientinterface.cpp`,
> `ScaleSurfaceToSurface`'s internal half included). The release build
> keeps the retail levels.

A check's message prints only when `g_DebugLevel` is at least the level it
names, and in the client interface that is almost always 1:

* `ci_ScaleSurfaceToSurface` (`0x0040dc50`) reports at 4, where its seven
  siblings in the blit family use 1.
* `ci_RenderCamera` (`0x00408ea0`) reports all three checks at 15, which
  nothing sets, so they can never print.
* `ci_GetPolyGridInfo` (`0x0040bc80`) reports at 0, so it always prints.

### The value-returning readers quote the shutdown message
`0x0041f150`-`0x0041fbd0` (lithtech.exe), and their server.dll twins ·
`src/engine/server/csbase.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`): the two error
> macros they use pass the single space every other check passes
> (`src/engine/server/de_error.h`). The release build keeps the retail
> description.

`ILTCSBase`'s error reporter takes a description as its last argument. Every
method returning `LTRESULT` passes a single space; every method returning
void or a value passes the engine's pending shutdown-message buffer (the one
`ci_Shutdown` hands to `ci_ShutdownWithMessage`). It is empty in practice; if
a shutdown message were ever pending, all twenty-nine of those errors would
quote it.

### `CMemStream::Read`'s debug line reads a missing argument
`0x10007854` (server.dll) · `src/engine/server/fsstream.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/server/fsstream.cpp` passes the size. The release build
> keeps the retail call.

The format is `"read from stream %p size %u"` and only `this` is passed, so
`%u` prints whatever is next on the stack. It fires only at stream debug level
4 or above; `CTreeStream::Read` passes both.

### `InitLevel`'s trace misspells a parameter
`0x004ab834` (lithtech.exe, the string)

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientmusic.cpp` spells it `sDefine3`. The release
> build keeps the retail string.

`... sDefine1=%s sDefine2=%s sDeine3=%s`: `sDeine3` for `sDefine3`, in every
trace the music system's level setup writes at debug level 3.
