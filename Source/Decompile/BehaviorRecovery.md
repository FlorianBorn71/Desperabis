# Object behavior recovery scaffold

The first compilable C++ target is a **selector/recovery scaffold**, not a
playable port or an implementation of object movement. Its purpose is to give
each recovered case an editable, named home without hiding missing behavior.

## Game context and application flow

[GameContext.h](Scaffold/GameContext.h) now owns the `GameContext` class,
formerly the `GameState` struct embedded in the dispatch header. Existing
context-owned operations remain member functions: sound, random numbers,
collision/height queries, object allocation, and prompts. All `GameContext`
member definitions, including application flow and page hooks, live together in
[GameContext.cpp](Scaffold/GameContext.cpp). Their private implementation helpers
also live there. Public gameplay state remains
directly accessible. There is no compatibility alias for the old type name.
The gameplay free-function and behavior-handler APIs still receive
`GameContext&`; converting all of them to members is deliberately out of scope.

[ApplicationFlow.h](Scaffold/ApplicationFlow.h) defines `MenuPage`, retaining
the original application-state numbers, and `MainMenuAction`. The recovered
outer state machine lives in
[GameContext.cpp](Scaffold/GameContext.cpp), implemented as
`GameContext` member functions:

- `RunApplication()` dispatches modal pages until Quit; `RunApplicationStep()`
  performs one page invocation for host-driven execution.
- `SelectMainMenuAction()` routes to chapter selection, load/save, settings,
  configuration, resumed gameplay, or Quit. It returns false for a disabled
  choice without changing pages. Resume/save use `hasActiveGame` together with
  the original shared suppression gate. Escape retains its distinct native gate.
- `OpenInventory()` represents the gated gameplay-to-F2 transition.
- `StartNewGame(chapter)` invokes `InitializeNewGame(chapter)` before marking
  the game active and selecting gameplay. The original selector calls
  `0000:0D30(1, chapter)`; cancellation returns to the main menu without loading.
  See [chapter-selection evidence](ui-chapter-evidence.txt).

Page bodies are protected virtual member functions, so future implementations
operate on their own context rather than receiving another context reference.
Ordinary pages return their next `MenuPage`; the main menu returns an action,
and chapter selection returns an optional chapter number. Settings/load/save
and inventory outcomes are supplied by their page bodies, not guessed by the
outer dispatcher. Unknown presentation bodies have an explicit shared hook;
verified forced outer transitions are still applied after those bodies return.

**This is an executable skeleton, not playable UI.** Default page hooks,
the complete gameplay frame loop, and new-game loading throw descriptive
`std::logic_error` exceptions. No page silently succeeds, loads resources,
or substitutes a partial object update for a gameplay loop.
`InitializeNewGame` is responsible for the unrecovered complete initialization,
including level resources and gameplay-mode flags. Chapter availability belongs
to the chapter selector. The existing console remains a behavior probe; it does
not call the unimplemented application loop.

[ApplicationFlowTests.cpp](Scaffold/ApplicationFlowTests.cpp) exercises all
65,536 state words against the recovered routing table, menu availability,
chapter acceptance/cancellation/failure, inventory entry, and a complete scripted
main-menu/submenu/new-game/gameplay/inventory/quit sequence.

## Existing object knowledge

The author identifies the renderer's behavior/collision values as original
in-game values, and describes logical object number 0 as the player.
Sources:

- [RawGameObjectData](../NewRenderer/SharedSource/base/Asset/RawGameObject.h).
- [BehaviorType, CollisionType, and accessors](../NewRenderer/SharedSource/base/GamePlay/GamePlay.h).
- [Accessor implementations](../NewRenderer/SharedSource/base/GamePlay/GamePlay.cpp).

The game code does not include those renderer headers: they bring in unrelated
renderer dependencies. [GameEnums.h](Scaffold/GameEnums.h) brings over the
renderer `BehaviorType`, `CollisionType`, `DisplayType`, and `WeaponType`
definitions and comments without those dependencies. The generator preserves
their values and augments `BehaviorType` with additional executable selectors.
Names whose meanings remain unknown use identifiers such as `Behavior_1`
and `Behavior_300`; these are proper enum members, not raw IDs in dispatch.
The recovered object is a standalone type, not a replacement renderer ABI.

`BehaviorType` uses `uint16_t` to preserve the original full-word selector.
`CollisionType`, `DisplayType`, and `WeaponType` use `uint8_t`; all imported
values fit their original byte storage. The behavior, collision, and display
members of `RawGameObject` have those enum types directly, without changing
the record's size or offsets. Item value remains a signed word because it is
overloaded across behaviors, not universally a `WeaponType`.

Runtime dispatch results contain a `BehaviorType`, not a handwritten string.
Descriptors use that same enum and derive display names from catalog tokens.
Strings are only for console name input/output. Numeric conversion belongs
at binary/CLI boundaries and in evidence, not in gameplay decisions. An
unnamed full-word selector still retains its exact value and follows the
original common path. `CollisionType` describes the object's collision byte;
it is distinct from the collision-map raster's provisional blocking-wall value.
Renderer-only `VisibilityFilter` flags remain in the renderer.

### Fixed-layout RawGameObject

[GameContext.h](Scaffold/GameContext.h) defines `RawGameObject` as
the canonical 54-byte struct with explicit members matching the author's Excel
table and renderer property accessors. Dispatch and every handler receive
`RawGameObject&` directly; there is no wrapper, field snapshot, or `ReadFields`
step. For example, the recovered Pillar case calls:

```cpp
context.SetCollisionMapPixelAtWorldXZ(
    object.posX, object.posZ, CollisionMapValue_BlockingWall);
```

Size and every named member offset are checked at compile time, without
compiler-specific anonymous structs. The layout is immutable; the field values
are mutable. All members default to zero. The struct is standard-layout and
trivially copyable, so binary I/O reads the original 54 bytes directly into it.
The original little-endian layout is required; configuration rejects
big-endian hosts.
Unknown fields retain hexadecimal offset names. The former packed
`field_04` is split into byte `renderPlacementMode` at offset 4 and byte
`soundEffectId` at offset 5. The former `candidate_tileAssignment` word at
offset 48 is now signed `renderSortKey`: its render-list sorting role is
confirmed, rather than a literal tile index.
Signedness follows existing accessors where known; otherwise an unsigned raw
word is only storage, not a claim about the original Pascal type.

`RawGameObject` is plain data with no raw-access methods. Gameplay uses named
members directly; binary I/O reads/writes the trivially copyable 54-byte struct.
Tests keep byte/word snapshots local to the test file for layout and mutation
checks. The former union, raw-access API, and field-copy indirection have
been removed.

Comparison with the renderer clarified byte 2 as the second model **frame**
(`GetModelFrame2`), while `GetModelAnim2` reuses byte 1. `winX/Y/Z` match
the Euler accessors but remain behavior-dependent, and player velocity words
also serve as view direction. Group/lock interpretations at 44/46 come from
the specialized accessors. The behavior field stays a full 16-bit word even
though the renderer accessor reads only its low byte.

### Common 54-byte layout

Offsets are byte offsets, hexadecimal. Words are little-endian. These meanings
come from the existing accessors and author, not independent proof that every
behavior uses every field this way.

| Offset | Width | Existing interpretation / caveat |
| --- | --- | --- |
| `00` | byte | Model ID |
| `01` | byte | Animation number, accessor subtracts 1 |
| `02` | byte | `modelFrame2`: second model frame, accessor subtracts 1; table's "Anim2?" |
| `03` | byte | Model frame, accessor subtracts 1 |
| `04` | byte | `renderPlacementMode`: common placement/culling selector; individual mode meanings remain provisional |
| `05` | byte | `soundEffectId`: configured sound-effect ID, not a sample reference or volume |
| `06` | byte | Collision type |
| `07` | byte | Collision eligibility mask: an overlap candidate needs at least one bit in common with the source |
| `08..09` | word | Original dispatch discriminator; renderer accessor reads only byte `08` |
| `0A` | signed word | Health; can also encode a door's switch lock |
| `0C`, `0E`, `10` | signed words | Position X, Y, Z |
| `12`, `14`, `16` | signed words | Velocity-like values; player accessor interprets these as view direction |
| `18`, `1A` | signed words | Collision radii XZ and Y |
| `1C`, `1E`, `20` | signed words | Euler/sprite-dependent values; yaw low bit also controls sprite flipping |
| `22` | word / bytes | Behavior-dependent parameter; item amount, key/switch index, weapon type, etc. |
| `24..29` | 6 bytes | Behavior-dependent; no universal common meaning yet |
| `2A` | signed word | Animation blend/transparency parameter |
| `2C` | word / bytes | Group index or behavior-dependent parameter |
| `2E` | word | Key lock / group bitmask / behavior-dependent parameter |
| `30..31` | signed word | `renderSortKey`: overwritten by original render-list postprocessing and used for signed descending sort |
| `32` | byte | `field_32`: nonzero enables generic inventory pickup and participates in player collision selection |
| `33` | byte | Display type |
| `34` | byte | `behaviorParameter`: contact-damage factor, door occupancy, switch latch, or other behavior-specific value |
| `35` | byte | `spriteScale`: independently initialized and grown by sprite behaviors |

These final two bytes have independent native byte accesses: contact damage
reads `34h` at `0000:DB56`, smoke creation writes `35h` at `10AC:04EA`,
and smoke growth compares/adds to `35h` at `0000:1AE7/1AFB`.
The scaffold names them separately while preserving the 54-byte binary layout.
This does not prove the original Pascal declaration used two byte members.
The existing renderer's `PropertyAccessorBase::GetScale` still reads both bytes
as a little-endian word and divides by `64 * 256`; that calculation is unchanged
pending verification of the original rendering consumer.

Byte `04` configures assignment/visibility; it is not general-purpose object
scratch. Word `30h` is render-owned scratch, not free behavior storage:
`0000:511D` overwrites it after case dispatch, and the list inserter at
`0000:100B` orders objects by signed comparisons of this word. Object links
are stored separately in DS:`D3DD + slot`, not in a second field of the
54-byte object. A tile-corner cache record holds the list head at offset
`17h`. These render-list operations are documented but remain outside the
gameplay-only update implementation. See
[render-list findings](Findings.md#render-owned-object-values-and-tile-list-links)
and [object-render-list-evidence.txt](object-render-list-evidence.txt).

The original loop processes only objects with **signed health > 0**. The
renderer accessor's `Isvalid()` tests `!= 0`; these predicates differ for
negative values. The scaffold follows the executable, without changing the
renderer.

### Coordinates and numbering

- Author recollection: Y is up; 100 raw units define a tile and are about a
  meter. Y-up and 100-unit tiles are supported by renderer code.
- Current renderer `DespToFloat` defaults to `0.005`, so 100 raw units become
  0.5 renderer units, not 1. Its physical-meter interpretation is not assumed.
- [Level::DespToPos](../NewRenderer/SharedSource/base/Asset/Level.cpp) offsets
  X and Z and reverses the raw Z direction. Keep recovered simulation in
  integer DOS coordinates; do not insert renderer transforms into movement.
- Logical player object 0 is consistent with DS:`8272`. The original routine's
  internal index 1 addresses that slot; index 2 addresses DS:`82A8`, the first
  non-player object. Do not confuse the Pascal loop index with logical numbering.
- The author recalls up to 140 objects, matching the bridge's copy count. The
  binary still iterates internal indices 2..164. The purpose of the extra
  slots is unresolved, not overridden by the remembered count.

## Recovered dispatch skeleton

[BehaviorCases.inc](Scaffold/BehaviorCases.inc) records the enum member and
original case entry, without repeating its numeric ID.
[BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp)
contains a separate, editable `Function_<Name>` for every specific case.
[BehaviorDispatch.cpp](Scaffold/BehaviorDispatch.cpp) routes the object to it.

For example, the generated catalog contains:

```cpp
BEHAVIOR_CASE(BatIndoor, 0x161D)
BEHAVIOR_CASE(BatOutdoor, 0x16A3)
```

The dispatcher expands those entries into calls equivalent to:

```cpp
case BehaviorType::BatIndoor:
    return Function_BatIndoor(object, context);
```

Pillar, LightSource, PulsatingLight, Door, Flies, RatAndSnake, FloatingDebris, behavior 18, FireParticle, BurningFireplace,
IdleAnimation, SmokeFading, SpawnInCrater, Switch1, SwitchGrouped, WaterDrop, LevelStartOverride,
and DisabledSentinel return `ExecutedCase`. The other
handlers return `UnimplementedHandler` without moving objects or writing
globals. Every handler returns its enum identity, for example:

```cpp
return { DispatchStatus::UnimplementedHandler, BehaviorType::BatIndoor, 0x161D };
```

### Coverage and word-sized behavior values

The full comparison spine has **186 non-sentinel cases plus `FFFF`**:

- 185 cases fit in a byte.
- **Behavior 300**, absent from the renderer enum, has a case at `0000:5108`
  that calls `10AC:29F5`.
- **`FFFF`** has a distinct path at `0000:1156`, not an ordinary byte behavior.
  Its path clears health; this write is now implemented.
- The renderer enum's `None`, `Behavior_201`, and
  `CrystalBallTableInCutScene` have no specialized case in this cascade.
  They still reach shared postprocessing in the original.

Together these give 190 named/numeric catalog entries. The scaffold reads the
**full word** at `08`, preserving byte `09`; `0x010E` must not become BatIndoor.
The behavior enum is ordered numerically, including recovered selectors;
the generator preserves this ordering and the renderer's inline comments.
Renderer enum values remain useful 1:1 identifiers for their named values,
but its byte accessor does not represent all runtime discriminator values.

## BatIndoor: first bounded recovery target

Pillar has since been selected as the first behavior investigation. Its writer
and shared collision-map pointer are explained in [Pillar.md](Pillar.md).
Its handler and point writer are now implemented and host-tested; the following
BatIndoor lead is retained for later investigation.

Verified selector: `14`, comparison at `0000:1618`, body at `0000:161D`.
The branch wrapper is equivalent to the following pseudocode:

```text
for step = 1 to tickCount:
    call unresolved module 10AC:3391(
        originalObjectSlot,
        rawWord(object + 22h),
        rawWord(object + 24h),
        10000,
        step == 1)
continue at shared object postprocessing 0000:5110
```

This describes the original instructions, **not** an implemented C++ handler.
The parameter meanings and helper's movement/collision/global effects remain
unrecovered. Do not guess names such as speed/radius for the two object words.

BatOutdoor has a separate wrapper at `0000:16A3` calling `10AC:5BE7`.
These are not assumed to be the same movement routine.

## Globals and dependencies

[BehaviorMap.json](Scaffold/BehaviorMap.json) records, for every catalog entry:

- Original case entry.
- Whether a specific branch exists.
- Reachable instruction count inside that case.
- Absolute DS addresses referenced directly.
- Direct near/far calls, using module-relative addresses.

These are dependency leads, not assigned variable meanings. The traversal
follows case-local branches/loops but **does not enter callees**, and stops
before shared postprocessing at `5110`. Register-indexed memory and segment-
overridden memory are not claimed as absolute DS globals.

Thus BatIndoor's direct-global list is empty even though its movement helper
may use many globals. Follow `10AC:3391` next, identify reads/writes, give a
global a tentative address-based identity, then use the author's context and
runtime evidence to assign a meaningful name. Shared globals must be modeled
once, not independently inside each handler.

The scaffold models the existing DS:`04CD` behavior-enable gate as an explicit
context input. It does not reconstruct all preconditions/global state.
The shared gameplay prefix and slot iteration are implemented by `UpdateObject`
and `UpdateObjects` below. Render-list postprocessing, player movement,
platform input polling, rendering, and hardware audio mixing remain unimplemented.
Sound-effect selection, its shared random-seed update, and backend dispatch
are implemented below. There is no
integration into the renderer yet.

### Shared object updates and allocation

`UpdateObjects(context)` processes original slots **2..164** sequentially,
in place, returning 163 `DispatchResult` values in slot order. Slot 1 is the
player; slots 0 and 165 are excluded. Later slots observe changes from earlier
handlers, including any future spawns. `UpdateObject(object, context)` exposes
the same recovered gameplay prefix for one object. The console now uses this
loop. `DispatchObjectCase` remains the lower-level, isolated case API, so it
does not apply the shared prefix a second time.

Native order, from `0000:1094`:

1. Skip objects whose signed health is <= 0 at entry.
2. For display values **50..120 inclusive**, request effect 62 (`Burning`),
   at distance scale 3, shared source 451, concurrency limit 2.
   Subtract `Random(frameTickCount + 1)` from the display value; a result
   below 50 becomes `VectorBrightness2` (22).
3. When `objectCollisionsEnabled` (DS:`D500`) is true and health is < 1000,
   approach zero by `tickCount`, without overshooting.
4. Respect `behaviorDispatchEnabled` (DS:`04CD`), then execute the case.
   **Do not recheck health here:** an object expiring in step 3 still gets its
   last behavior update. A handler can set health back to 1000.

`frameTickCount` is the separate DS:`D4CA` snapshot used by burning;
`tickCount` is the object routine's argument. Hosts normally set both to the
frame's pending ticks, as the console does with `--ticks`. Keeping them
distinct preserves the native helper's actual source. `GameContext::Random`
shares the existing Turbo Pascal seed/update with sound variant selection.

There is **no unconditional position/velocity or angular-speed integration**
in this prefix or the shared suffix. Motion remains in the individual
behaviors/helpers: moving WaterDrop or SmokeFading again here would double
their movement. The suffix at `5110` works on distance/scene-list assignment,
not a universal physics update; it is not implemented in the renderer-free
library. The original loop's reset of player-movement flag DS:`D4FF` is also
outside the current player-controller implementation.

Two context methods reproduce the allocators, including their side effects:

| API | Native address | Selection |
| --- | --- | --- |
| `FindFreeObjectIndex()` | `1E48:2049` | First slot 2..164 whose health is exactly zero; otherwise 165 |
| `FindFreeOrLowestHealthObjectIndex()` | `1E48:2085` | First zero-health slot; if none, first strict minimum health below 1000; otherwise 165 |

**Both clear all 54 bytes of the selected record before returning.** Neither
marks it occupied; the caller must initialize it immediately. Negative health
is not free for the first allocator, but participates in the second allocator's
signed minimum. Equal minima keep the first slot. Slot 165 is a native scratch
fallback, not an exception, null result, or additional updated world object.
An exhausted allocation can therefore overwrite an earlier scratch result.

Evidence: [object-update-evidence.txt](object-update-evidence.txt). Tests cover
all signed health words at representative ticks and both countdown gates,
expiry ordering, unchanged positions/angles, allocation bounds/clearing,
burning bounds and audio admission, and all seven independent input actions.

### Host-driven runtime integration

The renderer-independent `GameContext` can now serve as the game manager.
Its new host-driven lifecycle is separate from the older, modal
`RunApplication` / `RunApplicationStep` recovery harness. Do not run both loops
on the same live context. All context member definitions remain in
[GameContext.cpp](Scaffold/GameContext.cpp); behavior implementations remain
in their free handlers.

#### Loading and ownership

The host subclasses `GameContext` and implements one service hook:

```cpp
class RendererGame final : public Desperabis::GameContext
{
protected:
    void LoadLevel(const Desperabis::LevelLoadRequest& request) override;
};
```

`StartLevel(levelNumber)` calls this hook exactly once and enters `Loading`.
The hook may complete immediately or start asynchronous asset loading.
Marshal completion back to the game thread; the context is not thread-safe.
Keep the context alive until host jobs are cancelled or joined, and do not
move it while jobs or renderer references target it.

Construct a `LevelLoadData` containing:

- An owned `Level`, including its mutable collision map, decoded geometry,
  and recovered surface metadata needed by the chosen level's behaviors.
- The 166-slot object array in **native indexing**: reserved 0, player 1,
  ordinary objects 2..164, fallback 165. Do not assume the renderer's vector
  indices match these slots.
- The level's `floorRepeatHeight`, when applicable.
- For a fresh level, call `InitializeLevelPlayerSpawn(data, viewDirection)` after
  loading geometry, or explicitly supply `playerSpawn = PlayerCamera{eyePosition, viewDirection}`
  for a recovery fixture.
  `GameContext::InitializeLevelPlayer`, called by `CompleteLevelLoad`, replaces
  slot 1 with a clean normal player (health 1000, mask 3, radii 15/23) and converts
  eye Y to object-center Y. Normal play supplies direction only, not
  gameplay constants. If `playerSpawn` is absent, the supplied slot-1 record is
  preserved unchanged for explicit player-state restoration and recovery fixtures.

For fresh levels, the host's current `LoadLevel` override calls
`LoadLevelGeometry(request.levelNumber, data)`, then
`LoadLevelObjects(request.levelNumber, data)` and
`InitializeLevelPlayerSpawn(data, viewDirection)` before completing the load.
This GameContext helper constructs `LEVEL\OBJPOS<n>.DAT` and calls the protected
virtual `LoadFile(std::string_view)`, returning `std::vector<std::uint8_t>`.
The host implements byte retrieval only; file failures must throw (the default
implementation throws an explicit missing-service error). Names are logical
ASCII asset paths, not host filesystem paths.

GameCore validates complete 54-byte records and a maximum of 163 objects,
then decodes ordinary slots 2..164, clearing unused slots and preserving
reserved slot 0, player slot 1 and fallback slot 165. An empty, successfully
read file represents zero objects, not a missing file. Invalid sizes are
rejected before changing the destination. Record `i` maps to slot `i+2`,
retaining the existing integration mapping; native initialization is not fully
traced. File/parsing exceptions propagate through `StartLevel` into the existing
load-failure state, leaving the previous world intact.

`LoadLevelGeometry` reads `LEVEL\FELD<n>.256`, decodes its planes and calls
`Level::BuildCollisionMap` before objects run. The host supplies only level
number and initial view direction, not geometry bytes, spawn position, or gameplay object records.
The default `LoadLevel` does not yet load a complete level; other metadata,
surface-height tables, sound configuration and presentation assets remain separate.

#### Normal map start and one-shot LevelStartOverride

[player-start-evidence.txt](player-start-evidence.txt) traces normal selection
at `1E48:1CC6`, its fresh-load call with argument 1 at `0000:05D3`, and
the behavior-121 case at `0000:37DD`.

`InitializeLevelPlayerSpawn` scans the native wall-texture plane for the
**exact byte `0xE1`**. Columns are scanned outermost, rows innermost; the last
matching tile wins. Other shape-7 bytes such as `0xEF` are not normal starts.
For zero-based column `c`, row `r`, eye X/Z are `150+100*c`, `4050-100*r`.
Eye Y is the pending level's native `GetTerrainFloorHeight(X, 0, Z) + 45`,
including its repeat-height setting, not a renderer float height or a query
against the previous installed level. View direction is preserved.

The original helper retains existing coordinates when no marker matches.
For a fresh GameCore load, missing markers instead fail explicitly rather than
silently inheriting a previous level's position. All 36 complete original FELD
fixtures have a normal marker. Missing geometry and invalid terrain corners
also fail before assigning the spawn. Explicit player records and recovery
spawn overrides remain supported; save restoration does not reselect a marker.

`Function_LevelStartOverride` uses the existing native `OverlapsPlayer` helper:
collision gating enabled, matching masks, and strict XYZ overlap. Only health
**exactly 1000** activates it. It copies just the object's six XYZ bytes into
slot 1's **object-center** position and consumes itself by setting health to 0.
It does not change direction, movement speeds, inventory, sound, or RNG state,
and does not require the interaction key or positive player health.
The loader does not unconditionally use every behavior-121 object as a spawn.

Tests cover all complete original level markers, duplicate scan priority,
pending-level terrain interpolation, eye/center conversion, error atomicity,
the exact contact/health gates, and both original behavior-121 records.
In level 17, the actual map start overlaps the health-1000 object at
`(1095,66,3392)`: its first behavior update puts the eye at Y=91, then gravity
begins the drop without retriggering. Level 23's initial health-2000 record
correctly does not fire unless other gameplay changes its state to pass the gate.

Call `CompleteLevelLoad(request.id, std::move(data))`. Validation occurs before
replacing the current world, including validation of lighting-center coordinate
division. The context takes level ownership and installs its internal
geometry/collision views. It resets per-level messages, group masks, collision
selection, light contributions, and the shade-noise accumulator, then selects
`Gameplay`. Inventory, gold, weapon state, RNG, and host service settings survive
level transitions. **This is a direct level start, not recovered full new-game
initialization.** Initial spawn coordinates/direction and level-specific data
still come from the host; normal player-state construction belongs to GameContext.
Stop previous-level audio voices in the host as part of loading.

Only one load may be pending. Every request has a new ID, so duplicate or stale
completions/failures throw rather than install the wrong world.
`FailLevelLoad(id, message)` records `LoadFailed` and an inspectable error string;
an exception from the synchronous hook is also recorded and rethrown.
Invalid completion data throws without partially installing it; the host can
report failure or retry completion with valid data. During loading/failure,
the previous world remains available for display, but simulation does not
silently resume. `StartLevel` can retry after failure with a fresh request ID.

Contexts are noncopyable because they own mutable level state. Move support
retains the recovery harness's reset-by-temporary idiom; it is not a mechanism
for relocating a running async host. The existing public state and borrowed
`level` / `collisionMap` fields remain for low-level recovery tests. Host-driven
code must not replace those installed pointers; `Tick` checks them.

#### Tick, input, presentation, and timing

After installing a level, the host advances it with:

```cpp
const auto result = game.Tick({ behaviorTicks, frameTicks }, input);
if (result.status == Desperabis::GameTickStatus::Advanced)
{
    // Inspect result.interaction and each entry of result.objects.
    // Synchronize renderer instances from game.GetWorldObjects().
}
```

`InputHandler` and `AudioHandler` remain the narrow input/audio service
interfaces. `Tick` does not invoke the old modal UI virtuals. `SetPaused(true)`
or a menu page other than `Gameplay` suspends gameplay; the host processes UI
commands through the existing page/action APIs and renders the exposed state.
The loading lifecycle is independent of native numeric `MenuPage` values.
No menu rendering is invented here. A partial basic horizontal controller is
now wired into `Tick`, with its exact scope documented below.

An advanced tick runs in this host-integration order:

1. Install the explicit behavior/frame counts; run basic player movement when
   enabled, then refresh listener X/Z from player slot 1.
2. Clear dynamic lighting once around that position, preserving the selected
   active radius, and clear `field_DC34` once.
3. Resolve player contact and interaction input, including the door occupancy
   marker needed by the following object updates.
4. Run the recovered sequential object loop. Prompt storage and countdown belong
   to the app; the DX11 app advances its timer once after an advanced tick.

This orchestration is a host-facing integration policy, not proof that the
complete original frame loop has been recovered. Rendering, native player
movement/contact damage, menus, and many behaviors are still incomplete.
`GameTickResult` exposes the interaction result and all 163 dispatch results,
including `UnimplementedHandler` and `NoSpecificCase`; hosts must not interpret
an advanced tick as a claim of complete gameplay.

The host owns wall-clock conversion and scheduling. Both counts must be
nonnegative signed words; no assumed DOS frequency, hidden clock, or arbitrary
catch-up cap is added. One call with ten ticks is not necessarily equivalent
to ten calls with one tick, because several behaviors consume RNG once per
call. A **zero-count tick still updates**. Do not call it as a render-only
poll: render the last world state instead. Loading/paused/failed ticks consume
no input/RNG and do not clear lighting or age prompts.

The game world is authoritative. Use the const `GetWorldObjects`, `GetLevel`,
and `GetLightingCache` views to synchronize renderer-owned presentations by
stable native slot. Handle allocation, disappearance, and changed model/display
fields; do not cache an assumption that a slot always represents the same
object. Disable duplicate renderer-side simulation for game-controlled objects.
The actual renderer synchronization and visual-lighting adapter are not
implemented in this step.

`GetPlayerCamera()` returns a value snapshot based on player slot 1: `position`
contains DOS eye X/Y/Z, and `direction` contains the raw view-vector
words (`velocityX/Y/Z`), **not** the player's Euler fields. The renderer can use
`Level::DespToPos` for position and normalize `(-direction[0], direction[1],
direction[2])` for its forward vector, matching the existing player accessor.
For an installed world, eye Y is reconstructed as
`posY + collisionRadiusY + 2`, with native word wrapping: `E1F4..E210`
stores the object center by subtracting those values from working/view Y.
**Do not add a further renderer-side eye offset.** No hover-camera adjustment,
projection parameters, or renderer dependency is added. A zero direction remains
zero: the host must not normalize it as a valid orientation.
The getter always reads the current record, even when paused or during a level
transition/failure; use `GetLoadedLevelNumber()` to establish whether a world
has been installed. Before a level is installed it exposes the initial
zero-filled player record, not a fabricated valid camera.

#### Duplicate enums and the 54-byte boundary

Do **not** `reinterpret_cast` the unrelated renderer/game object types.
The renderer's `RawGameObject` wrapper is larger than 54 bytes: it also contains
level/index metadata. Even two equal-size structs are not automatically legal
aliases under C++ object-lifetime and strict-aliasing rules.

Copy just the native payload using the checked boundary:

```cpp
static_assert(sizeof(rendererObject.m_rawBytes) == sizeof(Desperabis::RawGameObject));
const auto object = Desperabis::DecodeGameObject(
    rendererObject.m_rawBytes, sizeof(rendererObject.m_rawBytes));
Desperabis::EncodeGameObject(object,
    rendererObject.m_rawBytes, sizeof(rendererObject.m_rawBytes));
```

Both functions require a non-null, exactly 54-byte buffer. Game-side size,
offsets, and trivial-copyability remain compile-time checked; renderer payload
size is also asserted. Renderer wrapper metadata is not copied or overwritten.
This preserves behavior as a 16-bit ID, `behaviorParameter` at 52, and the
independent `spriteScale` byte at 53. The renderer's existing combined-word scale
accessor is intentionally unchanged pending native rendering verification.

Enum **numeric IDs** are the interoperability contract, not their C++ storage
sizes or current names. The `game-runtime` test compares every renderer entry
in `BehaviorType`, `CollisionType`, `DisplayType`, and `WeaponType` against the
game header, including implicit enum increments. The intentional behavior-name
alias `SwimmingParticles` -> `FloatingDebris` preserves ID 117. Game-only recovered
entries are allowed. A negative test verifies that changing an existing ID fails.
The long-term direction is one shared canonical definition, but consolidating
enums or renderer accessors is not required for this byte-copy bridge.

### Gameplay lighting cache

`GameContext::lightingCache` owns a **21x21 signed-word contribution grid**.
It represents only the dynamic-light word at offset `10h` in the original
24-byte corner records, not their projection or render-list fields.
[Lighting.cpp](Scaffold/Lighting.cpp) implements the gameplay portion of
`10AC:0000`: source X/Z are rounded using wrapped `coordinate + 50`, divided
by 100 with the native signed-byte quotient limits, then made relative to
the center's truncated tile coordinates. Contributions add with signed-word
wrapping, including negative values; no brightness clamp is invented.

```cpp
context.lightingCache.Reset(playerX, playerZ);
context.soundListenerX = playerX;
context.soundListenerZ = playerZ;
UpdateObjects(context); // LightSource objects contribute automatically.
// Optional additional contributions during the same frame:
context.lightingCache.AddAtWorldXZ(lightX, lightZ, contribution);
const auto value = context.lightingCache.contributions[10][10];
```

Storage is `[relativeX + 10][relativeZ + 10]`, with **positive world Z** in
the second index (not the reversed row order of the level planes).
`Reset` clears every contribution and captures the frame's center. Its
optional radius models DS:`3117` and defaults to 10; only 0..10 fits the
native cache. `AddAtWorldXZ` returns false when culled by that radius,
otherwise true. Invalid radii and signed-byte division overflow throw before
changing the cache, rather than allowing DOS-style out-of-bounds writes.

Reset **once per frame before collecting lights**, not per object. It is an
explicit frame operation, just as the native cache clear is outside
`0000:1094`; `UpdateObject`, `UpdateObjects`, and direct case dispatch do not
clear it. This lets multiple update passes contribute to the same frame.
Moving the center requires another reset, since stored indices are relative.

`LightSource` (`0000:1488`) now populates this cache during ordinary
object updates and isolated case dispatch. It preserves the native order:

1. If the signed, wrapped Manhattan listener distance is in **0..300**,
   add `Random(0xEFFF)` to `GameContext::field_DC34` with word wrapping.
2. Add **`Random(30) + 5`** to the object's nearest X/Z cache corner.
3. Draw `Random(100)` even when the light was outside the cache window.
   If `tickCount` is strictly greater, play `soundEffectId` at object byte 5
   as the sound-effect ID, using attenuated object volume, the object's
   world slot, and instance limit **2**. Effect byte zero means no sound.

The same shared distance calculation is exposed by `GetSoundDistanceAt`;
DS:`D4FA` suppression forces distance 10000 for both the near-light test and
sound attenuation, but does not disable lighting. The object's own 54-byte
record is unchanged by its light case.

The legacy-named `field_DC34` preserves the observed accumulation.
A subsequent trace found that the timer passes it to the RNG and stores
the result in DS:`DC36`; a shade-selection helper adds that unsigned word
before discarding a fixed-point fraction. This supports **shade dithering**,
not a large thunder flash. The timer and shading consumer are not ported.
The original clears DS:`DC34` each frame (`0000:E345` / `E6EB`), but
`LightingCache::Reset` does not clear this separate context field; frame
integration must reset it separately. See
[the follow-up findings](Findings.md#light-noise-versus-a-lightning-flash)
and [lighting-shade-evidence.txt](lighting-shade-evidence.txt).

Static brightness remains packed in `Level::surfaceFlags`. No textures,
interpolation, projection-field updates, rendering, or renderer integration
are included. `PulsatingLight` is also implemented below; other unrecovered
light-emitting cases remain explicit placeholders.

Evidence and full DOS record details:
[lighting findings](Findings.md#static-brightness-and-dynamic-tile-corner-lighting)
and [lighting-evidence.txt](lighting-evidence.txt). Tests cover all 65,536
source-coordinate words on both axes against independent arithmetic,
negative/edge centers, active-window culling, full-grid indexing, clearing,
accumulation/wraparound, validation, and context isolation. LightSource tests
add all 65,536 low-word seeds at near/far distances and both strict sound
threshold outcomes, configured sample selection and source limits, signed
distance/suppression boundaries, unchanged object records, multi-light
world-loop accumulation, culled-light RNG, dispatch gates, and successive
frames with explicit resets.

### PulsatingLight: randomized pulse speed and one-corner lighting

Behavior **23**, `Function_PulsatingLight`, implements the complete case at
`0000:19CD..1A94`. Evidence is preserved in
[pulsating-light-evidence.txt](pulsating-light-evidence.txt), including its
existing `GetStepToward` and clamped `Sine` dependencies.

The native order matters:

1. Advance phase `field_26` using the **old** `velocityZ`:
   `(field_26 + velocityZ * ticks) & 511`.
2. Draw `Random(15)` once per call. On zero, set `velocityX` to
   `8 + Random(10)`, the target pulse speed.
3. Move `velocityZ` toward `velocityX` using `GetStepToward` with `ticks`,
   preserving signed-word subtraction, absolute-value, and addition wrapping.
4. Read the **clamped** 16.16 sine table at the new phase. Set `field_28` to
   `800 + floor(750 * sine / 65536)`, in **50..1550**. The downstream meaning
   of this object field is not established by this case.
5. Add `30 - floor(25 * sine / 65536)`, in **5..55**, to the single local
   lighting-grid corner selected by the object's X/Z, using the existing
   `LightingCache::AddAtWorldXZ`.

Negative products round down, not toward zero. At zero ticks, target selection,
phase masking, both pulse outputs, and the grid write still occur. Culling the
corner does not skip object updates or RNG. The case does not move the object,
play audio, allocate objects, or write `field_DC34`. Grid clearing remains the
caller's once-per-frame responsibility. No rendering changes are made.

Tests compare all 54 object bytes, RNG state, and the complete grid across
65,536 fixtures, including extreme speed/tick values, conditional draws,
culled corners, and contribution overflow. Additional checks cover all 512
phases in zero-tick world updates, clamped extrema, multiple sources, and dispatch/inactivity
gates. Run `--behavior PulsatingLight --ticks 3` for pulse-state and light-total
diagnostics.

### Basic horizontal player controller

`GameContext::Tick` now calls the private `UpdateBasicPlayerMovement` before
refreshing listener/light-grid centers, contact selection, and object behaviors.
`basicPlayerMovementEnabled` defaults to **true**. Set it to false if the host
already controls the player, or for a stationary recovery fixture.
`GameTickResult::playerMovement` is present when the basic controller is enabled;
it reports `simulatedTicks` and `blockedTicks`. Loading/paused/failed ticks have
no movement result. Inactive players and zero frame ticks simulate zero steps
and do not query movement keys.

This is a **partial normal walking/posture controller**, not the complete native controller.
It uses the existing [player-movement-evidence.txt](player-movement-evidence.txt)
and reusable math/collision helpers:

- Sample the six horizontal movement keys and jump/crouch once per advanced host tick with
  positive `frameTicks`, in native keyboard order. Add/subtract 4 to forward
  and sideways speeds, add/subtract 7 to turn speed, then reduce each speed
  toward zero by 1 (`C603..C679`, `C74E..C774`, keyboard mode zero).
  Jump/raise precedes crouch (`C679..C69F`); neither input is edge-triggered.
- Run one normal horizontal substep per **frame tick**, not per behavior tick,
  matching the normal loop's repeated `D060(6)` calls at `E39A..E3BC`.
  `Tick({0, 3}, input)` therefore moves the player three substeps while leaving
  the separate object behavior argument at zero. No wall-clock frequency is
  inferred.
- Clamp straight speed to 5, diagonal axes to 3 (native `2*5/3`), and turn
  speed to +/-6. Approach target heading using the existing native angle-pair
  normalization and `GetStepToward(..., 6)`, wrapping phase to 0..511.
  Crouched height below 45 limits speed to 2 when `behaviorDispatchEnabled`
  (DS:`04CD`); underwater overrides that with 3. Diagonal limits use `2*limit/3`.
  Other special speed gates and the alternate loop's approach step 2 remain deferred.
- Compute the 100-scale horizontal direction from the **ordinary**, not
  clamped, sine/cosine table (`D699..D6D2`). Integrate local forward/sideways
  speeds with the word-product/truncating-division formulas at `D763..D79C`.
  The normal speed bounds keep the signed-byte quotient in range.
- With `objectCollisionsEnabled`, use the existing 3x3 collision query at the
  proposed X/Z with **step 5 and mask `18h`**, reducing forward speed by one
  toward zero per correction (`DD97..DE08`). No radius-based capsule or
  renderer physics is substituted.
- Decrement physical vertical speed by one per frame tick (`D6F6`), integrate
  working/view Y (`D79D`), and clamp it from below to interpolated terrain plus
  the current body height, normally **45** (`E8E4`, `D844..D8AE`). Landing clears falling
  speed. Convert working/view Y back to object-center Y by subtracting the
  vertical radius and 2 (`E1F4..E210`). Uphill movement lifts the player onto
  terrain; downhill movement falls onto the lower support. Terrain support uses
  `GetTerrainFloorHeight`, not the special flat-depth shortcut in `GetFloorHeight`.

Explicit host policies, not recovered native initialization/fallbacks:

- Level installation resets all controller speeds and derives initial heading
  from the supplied player's horizontal view vector. A zero X/Z direction
  starts at heading 0 (+Z). No record Euler field is assumed to be player yaw.
  Body height/request reset to 45/0, grounded/submerged flags to false, as at
  native `0DD4..0DF2`. Complete restoration of posture from a save is not implemented.
- Each positive substep writes horizontal direction at the native 100 scale to
  player `velocityX/Z`, so `GetPlayerCamera` reflects yaw. The supplied
  `velocityY` view component remains untouched. Falling speed is a separate
  private controller word, reset to zero on level installation, rather than
  reinterpreting the host's view Y as the native physical velocity at DS:8286.
  The host should start with a level forward vector (`velocityY == 0`);
  preserving arbitrary camera
  pitch across the native 100-scale rewrite is not implemented.
- Terrain support is sampled at the final map-corrected X/Z, including on
  horizontally blocked steps. This keeps the normal host controller grounded;
  it does not reproduce the original ordering around special contact modes.
  The native bob and other support offsets are omitted. Initial position is
  retained until the first simulated substep, so an airborne start falls rather
  than silently snapping during loading.
- The native map-correction loop is unbounded. The host preview attempts at
  most **16 queries per substep**, then rejects unresolved X/Z and reports a
  blocked tick. A blocked center also rejects motion when symmetric neighboring
  contributions cancel; it must not silently allow walking through a solid
  map cell. Valid corrections can move the player away from a wall even with
  no pressed movement key.
- Keyboard input is accumulated once per host call. Multiple substeps do not
  reapply key acceleration or damping. Different frame batching can therefore
  yield different acceleration, as it can in the recovered input/frame split.

TODOs are placed beside the partial implementation in
[GameContext.cpp](Scaffold/GameContext.cpp):

1. Load the actual working camera position, applied/target headings, and speed
   state from native level/save initialization.
2. Recover the remaining view-vector pipeline, steep-slope X/Z corrections
   (`1E48:3621`), bridges/platforms, bobbing and pitch. Normal gravity,
   posture, room-clearance gates and center-terrain support are implemented;
   full ceiling collision and object-supported floors are not.
3. Add remaining special speed states, mouse/look, swimming controls and the
   alternate controller path.
4. Recover DS:`2DDC` collision gating and native stuck recovery.
5. Implement automatic player damage, teleporting and other contact modes in
   the shared contact pipeline, not in this normal movement subset.

Fresh levels loaded through `LoadLevelGeometry` have a real static FELD-derived
collision map. Explicit dummy fixtures still have no walls unless populated.
No object support or renderer-scene collision is invented.

#### Jumping, crouching and water-forced posture

Evidence: [posture helpers/controller](player-posture-evidence.txt),
[input/initialization/context](player-posture-context-evidence.txt), and
[water forcing and eye/center conversion](player-posture-tail-evidence.txt).

- `playerBodyHeight` is DS:`D4B2`, normally 45 standing and 28 crouched.
  `playerPostureDelta` is DS:`D4B4`, **not a dedicated death-state word**
  (correcting the earlier `playerDeathState` interpretation). Life depletion
  still writes -1 to this word; keyboard death gating now checks the actual
  fixed-point player-life meter, not a posture request.
- `RequestPlayerJump` (`10AC:CF23`) does nothing when player `field_28` is
  nonzero. Otherwise, body height exactly 45 plus `playerGrounded` (DS:`05D4`)
  sets physical vertical speed to 7. All other cases request raising with +6.
  A crouched player stands first; another input sample can jump once standing.
- `RequestPlayerCrouch` (`10AC:CF4B`) requests -6 and cancels positive physical
  vertical speed. Releasing it leaves the stance unchanged; jump/raise stands up.
- Each simulation substep applies `GetStepToward(height, height+request, 3)`,
  including word wrapping. With player `itemValue == 0` and `field_D500`,
  overshooting 28/45 clamps height and clears the request; equality alone
  does not clear it (`D1AE..D1FE`).
- Query relative room height at `X/Z + (directionX/Z SAR 3)`. Below 96 cancels
  ascent; below 65 additionally approaches height 28 at step 8 (`D6FA..D745`).
  These are the native clearance gates, not a generic absolute-ceiling clamp.
- Landing uses a strict `supportY > eyeY` comparison. Normal gravity clears
  grounded when not clamped, including exact equality (`D88C..D8BE`).
  Vertical radius is `(height SAR 1)+1`: 23 standing, 15 crouched. Camera and
  collision center remain related by `objectY = eyeY-radiusY-2`.
- The **collision-map byte**, not FELD `surfaceFlags`, selects water contact.
  If it is not exactly 1 and player `field_28 == 1`, clear that latch and
  invoke jump/raise (`DE45..DE5B`). For low-three-bit category 1 or 3 with
  `eyeY-supportY < 5`, request crouch and set that latch to 1 (`DE61..DE86`).
  The full-byte exit comparison is intentionally preserved, including category 3.
  Contact can force crouching while the head is still above water.
- On that contact path, `GetHeightSurfaceDepth` reads the current tile's full
  FELD surface byte; `eyeY+depth < 0` sets `playerUnderwater` (`DE87..DEBD`).
  Normal controller substeps recompute the flag. Hosts disabling the controller
  must maintain their own posture/environment state.

The native DS:`D4FF` special-vertical mode, swimming/buoyancy, oxygen,
underwater presentation and environmental damage are **not ported here**.
Normal terrain gravity still applies underwater. The native death animation
also remains separate. `player-posture` tests cover the jump arc, input
ordering/batching, posture boundaries, camera/radius coherence, clearance and
speed thresholds, water forcing/exit/submersion, and lifecycle gates.

The existing low-level `UpdateObjects`/`DispatchObjectCase` APIs still do not
move the player; this controller is specific to the host-driven `Tick`.

Tests cover all 64 movement-key combinations at four cardinal initial headings,
held acceleration, released-key damping, opposing keys, yaw wrapping, diagonal
limits, multiple substeps, zero ticks, separate clocks, collision masks, bounded
rollback, walls, movement-before-contact, same-frame listener/light updates,
load-reset, pause/disable/inactive gates, and unchanged player Y/unrelated bytes.

### First implemented case: Pillar

[Function_Pillar](Scaffold/BehaviorHandlers.cpp) reads the original signed
X/Z words and calls `GameContext::SetCollisionMapPixelAtWorldXZ(x, z, value)`
with `CollisionMapValue_BlockingWall` (`0x38`).
[GameContext.cpp](Scaffold/GameContext.cpp) implements the helper at
module-relative `10AC:387B`, retaining the three word-shaped arguments,
signed divisions, 16-bit wrapping, header-derived stride, and low-byte store.

Supply `GameContext::collisionMap` with a `CollisionMap*` directly.
The struct contains both header words and inline byte data, not a data pointer
or a field-copy view. Missing map state throws.
The original has no coordinate bounds check, so the helper does not clamp
to the header dimensions. A completed case reports `ExecutedCase`, which
does not claim shared pre/postprocessing or the entire object update is done.

`CollisionMap` directly contains the stored words `widthMinusOne` and
`heightMinusOne`, followed by `std::uint8_t data[VARIABLE_SIZE]` at offset 4.
There is no separate header structure or container in this layout.
Its temporary capacity is 40000 pixels, matching the current dummy dimensions.
`Level` owns a `std::unique_ptr<CollisionMap>`; the map is not embedded in the
level. `CreateDummyLevel()` allocates exactly `width * height + 4` bytes
(40004), then initializes the struct in that allocation. The owning pointer
releases it automatically. Compile-time checks ensure the declared inline
array fits this allocation. The writer retains 16-bit address wrapping with
the header normalized to offset zero, but throws if the computed address
falls outside the actual allocation. Importing
a physical segment capture with a nonzero map offset requires rotating its
bytes first; that import path is not implemented. See
[Pillar](Pillar.md#first-identified-shared-global) for details.

`CreateDummyLevel()` in [CollisionMap.cpp](Scaffold/CollisionMap.cpp) returns
a `Level` with a zero-filled **200x200** collision map. Its header
stores `199, 199`, with 40000 pixel bytes and no extra backing capacity.
This is an explicit test level, not a recovered level loader. Variable-sized
levels, geometry, and real object population will follow later.

### Switches: local animation, indexed activation, and group masks

Evidence: [switch-evidence.txt](switch-evidence.txt). `Switch1` and
`SwitchGrouped` per-frame handlers are implemented in
[BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp), with shared group masks
in `GameContext`. Ordinary-switch interaction now uses the complete shared
damage/activation dispatcher for indexed targets; other switch variants remain partial.

**Ordinary switch (`Switch1`, 50, `0000:1FDD`):** its per-frame case is
object-local. Byte offset 42 is an animation phase, offset 38 its signed rate,
and offset 36 the return speed (initialized to 10 if zero). It adds
`low16(rate * ticks)` to the phase, preserving word wrapping and signed tests:

- Below zero: stop the rate and clamp phase to zero.
- Above 100: clamp phase to 100 and set rate to the negation of offset 36.
- At phase 100, a nonzero byte at offset 52 stops the rate, latching the switch.

Thus ordinary auto-return is in this case, but there is no separate countdown
here. Offset 52 is `behaviorParameter`; for this behavior it is a latch flag.
The separate `spriteScale` byte at offset 53 is preserved.

**Index-based target activation:** the renderer's switch index at offset 34
is confirmed by the interaction paths `10AC:194C` and `10AC:2094`, which
call `10AC:2060`. That helper takes one word index (`RETF 2`), adds 1000
with word wrapping, scans internal object slots **2..140 inclusive**, and
calls `10AC:10F5(slot, 0)` for each object whose signed health word equals
that encoded value. It does not operate on a global switch-bit mask.
This includes doors but is not limited to them. The 2..140 bound is separate
from the main update's previously observed 2..164 bound.

**Ordinary switch interaction (`0EF0:0F04`, `10AC:2094`):** implemented by
`HandleSwitchInteraction` in [Door.cpp](Scaffold/Door.cpp), alongside the shared
general `GameContext::ActivateObjectGroup` helper it reuses. Only phase **zero** accepts a pull.
The switch copies return speed `field_24` into animation rate `field_26` even
when its index is nonpositive or its positive health is not exactly 1000.
No independent rate-zero guard is added. Only positive index plus health 1000
dispatches targets and requests the switch's configured sound (instance limit 1).
This branch returns directly, never falling into inventory pickup.

Matching door targets reuse the recovered `10AC:1312..1370` door activation:
positive key locks still refuse opening, negative switch locks allow it, phase
below 100 starts rate +3, and the configured closing delay is refreshed.
Encoded target health is preserved: the `10AC:1132..1196` normalization prefix
does **not** include doors. Doors continue animating and writing collision-map
state through their existing behavior. The common epilogue has no extra effects.
Additional bounded evidence is in
[simple-switch-activation-evidence.txt](simple-switch-activation-evidence.txt).

The complete `10AC:10F5` target cascade is now implemented by `DamageObject`.
The former non-door preflight has been removed: targets execute sequentially
in native slot order, including their activation, destruction and secondary
effects. See [weapon damage and shared activation](#weapon-damage-and-shared-activation).

**Grouped switch (`SwitchGrouped`, 51):** the case at `0000:2065` passes
object slot then ticks to `10AC:CAEC` (`RETF 4`). The helper sets collision
type to `CollisionType::GroupedSwitch` and updates/clamps the phase at 0..100.
It uses these additional object fields:

| Byte offset | Named member | Observed grouped-switch role |
| --- | --- | --- |
| 34 | `itemValue` | Configured return delay, copied to offset 36 on interaction; must be signed-positive to enable countdown |
| 36 | `field_24` | Signed countdown, decremented by ticks while fully active |
| 38 | `field_26` | Signed animation rate |
| 42 | `blendWeightOrSpriteOpacity` | Signed animation phase |
| 44 | `groupIndexOrLock` | Index into the shared group-mask byte array |
| 46 | `doorLockOrGroupBitmask` | Bit mask; only the low byte persists in shared storage |

At phase 100 it clears its bits from `DS:EC70 + groupIndex`; at phase zero
it ORs those bits back. At phase 100, if both offsets 34 and 36 are positive,
it subtracts ticks from offset 36. When the signed countdown drops below 1,
it sets it to zero, sets animation rate to -10, and restores its mask bits
**immediately**, before the return animation completes.

The separate object-interaction dispatcher at `0EF0:09F8` supplies the missing
timer setup. Its `CollisionType::GroupedSwitch` case at `0EF0:0F52` only enters
the activation body when the animation rate at offset 38 is zero. Phase 100
sets rate to -10; phase zero sets rate to +10. It then copies the configured
delay at offset 34 into the countdown at offset 36 (`0EF0:0F9D`). Thus the timer
is armed/rearmed on interaction but only counts down while fully active.
This case does not write the group masks itself. These instructions are in
[player collision evidence](player-collision-evidence.txt). Both the per-frame
helper and this gameplay interaction are now implemented; the original
audio/UI helpers are not.

**Shared state is five bytes, not one mask.** Initialization at `0000:048E`
and `1E48:211C` fills exactly five bytes starting at DS:`EC70` with `FFh`.
The called byte-fill helper at `2CE4:1B8C` is included in the evidence.
The corresponding `GameContext` member is:

```cpp
std::uint8_t switchGroupMasks[5] = { 0xff, 0xff, 0xff, 0xff, 0xff };
```

The ordinary door helper at `10AC:3F56` uses its offset-44 group index to
read the same array. If that byte equals zero, it calls `10AC:10F5(slot, 0)`.
The latter's door-collision case at `10AC:1312` starts opening when unlocked
and copies the configured delay into the door's timer. This establishes a
shared switch/door dependency, not the complete door or interaction system.
The host grouped-switch handler rejects indices outside the recovered
five-byte array before changing object or group state, rather than imitating
unchecked DOS addressing. Inactive objects and disabled dispatch still skip
the handler, including this check.

The per-frame handlers do not need access to other objects: the ordinary case
only updates its own animation, and the grouped case updates its own animation
and shared masks. Index-based activation now scans the shared object collection
at slots 2..140 and calls the recovered `10AC:10F5` target cascade. Ordinary doors
are implemented below; the automatic player-contact cascade and other explicit
interaction cases remain separate. Explicit grouped-switch interaction and
timer arming/rearming are implemented separately from per-frame dispatch.

**Unlocking subsequently traced:** `10AC:3DCE` checks movement state and the
signed lock word, searches all fourteen inventory slots for a live Key with
matching `itemValue`, consumes the first match by zeroing its health, clears
the object's lock, and calls `10AC:10F5(slot, 0)`. Negative locks refuse key
unlocking; zero locks activate without a key. The complete routine and its
failure/hand-slot details are documented in
[door-unlocking findings](Findings.md#door-unlocking-and-inventory-keys) and
[door-unlock-evidence.txt](door-unlock-evidence.txt). This closes the
key-matching/consumption uncertainty; ordinary-door unlocking is now implemented.

Tests cover every 16-bit phase, rate, and countdown/return-speed value at
representative ticks, latch low-byte semantics, signed timer guards, masked
byte writes, object-field isolation, multiple switches sharing a group,
immediate restoration on expiry, independent contexts, invalid indices, and
health/dispatch gates. Console output shows phase, raw rate/timer words, and
the selected group mask, offers explicit grouped-switch interaction through
`--interact`, and supports ordinary-switch interaction with indexed door targets.
Runtime tests also exercise both target-scan endpoints, unmatched/out-of-range
targets, wrapped health links, key locks, inactive and moving/latched switches,
unsupported-target atomicity, configured sound selection, and Enter-driven
level-11 switch-to-door animation using original OBJPOS records.

**Other variants:** `RopeSwitch` (52) generates a periodic phase through
`1E48:0051`, with a special object-slot comparison against DS:`DC60`;
`Switch3` (53) moves `winY` toward `velocityY` through `GetStepToward` and
disables its collision/behavior on arrival; `Switch4` (56) updates rotation
word `winZ`, copies it to phase before clamping, returns at its configured
limit in `velocityZ`, and also supports the offset-52 latch flag. These cases
do not by themselves establish the entire corresponding interaction paths.

### Player collision and object interaction

The executable has both a player-contact cascade and a separate object
interaction dispatcher. Both select on the **collision byte at offset 6**,
not the behavior word at offset 8. The following names describe our findings;
they are not surviving Pascal symbols.

| Original address | Role / suggested name | Observed contract |
| --- | --- | --- |
| `1E48:2B33` | `FindCollidingObject` | One word source slot; candidate slot or zero in AX; `RETF 2` |
| `0000:CE9E` | `FindPlayerCollision` | No explicit arguments; selected slot in AX; near return |
| `0000:D8FC`, inside `0000:D060` | Player-contact collision-type cascade | A block within the player update, not a separate function |
| `0EF0:09F8` | `HandlePlayerObjectInteraction` | One word object slot; `RETF 2`; return-value contract not established |

**Overlap query.** `1E48:2B33` returns zero when DS:`D500` is zero. Otherwise,
it scans slots 2 through 164 inclusive, excluding the source and objects with
signed health <= 0. Source and candidate byte-7 masks must have a nonzero AND.
It returns the first candidate satisfying all three strict bounding-box tests:

```text
abs(source.posX - candidate.posX) < source.radiusXZ + candidate.radiusXZ
abs(source.posY - candidate.posY) < source.radiusY  + candidate.radiusY
abs(source.posZ - candidate.posZ) < source.radiusXZ + candidate.radiusXZ
```

This is conceptual geometry, not exact host arithmetic: the original
differences, sums, and absolute-value operations are 16-bit, including the
signed `-32768` absolute-value overflow. Equality rejects the candidate, so
boxes merely touching at a face do not collide.

**Player selection.** `0000:CE9E` first calls the generic query with slot 1,
the player at DS:`8272`. If a candidate exists, it searches later slots for
an overlapping candidate with signed health > 999 and nonzero byte 50,
applying the same mask and radius tests. The first such later candidate
replaces the original; otherwise the original remains. This selects one
object, not the nearest object or every overlapping object.
The loop tests its upper bound after incrementing and examining a candidate:
normally it ends at slot 164, but an initial hit at 164 causes a single read
of slot 165. The role of that adjacent record is not established. The host
reserves storage for it rather than reading past its object array.

**Contact response.** Inside `0000:D060`, the block at `D8CB` clears
DS:`DC60`, conditionally queries `CE9E`, and stores the selected slot there.
`D8FC` loads its collision byte; the comparison cascade begins at `D900`.
Confirmed branches include `LevelEnd`, `TriggerTextPrompt`, `PushPlayer`,
enemy contacts, `Door`, `BurnableOilPuddle`, `Teleporter`, `DestroyOnCollide`,
`CurtainOpens`, `PickableTorchlight`, and `LiftUp`. For example, the door
branch at `DB06` sets the contacted object's byte 52 to 1. Ordinary and
grouped switch activation are not branches of this contact cascade.

**Separate interaction.** `0EF0:09F8` has another collision-type cascade,
loading byte 6 at `0A67`. The ordinary `Switch` branch at `0F04` calls
`10AC:2094(slot)`; `GroupedSwitch` at `0F52` starts return/activation animation
and arms the timer as described above. The author confirms that interaction requires explicit user input, normally
ENTER, including medicine/gold pickup. Input gates at `0000:C5A5` and `C6E8`
conditionally call the dispatcher only with a positive selected slot;
complete input mapping and polling are not implemented. Merely touching a
switch does not activate it.
Encoded far calls at `0000:0FF6`, `0000:C5B9`, and `0000:C701` confirm the
dispatcher address and segment. Its module-linear address also aliases
`0000:F8F8`, but its actual encoded target is `0EF0:09F8`.

All eight bounded ranges in [the instruction evidence](player-collision-evidence.txt)
decode completely, including both overlap functions and the complete
`0EF0:09F8` dispatcher through its return at `1178`. Both geometry queries
and the collision-to-input-to-interaction path are now implemented. The
automatic player-contact cascade remains unimplemented.

### Per-update player interaction

[GameContext.cpp](Scaffold/GameContext.cpp) implements
`GameContext::FindCollidingObject` and `GameContext::FindPlayerCollision`.
[PlayerInteraction.cpp](Scaffold/PlayerInteraction.cpp) implements the
host frame hook `UpdatePlayerInteraction(GameContext&, InputHandler&)`.
The input interface has virtual `IsKeyPressed(InputKey)` and a virtual
destructor. Its actions are `ForwardKey`, `BackwardKey`, `StrafeLeftKey`,
`StrafeRightKey`, `TurnLeftKey`, `TurnRightKey`, and `InteractionKey`.
A renderer/platform implementation supplies the actual bindings and input;
no movement simulation is triggered just by querying a key. Unconfirmed
jump/crouch actions are not guessed into the movement implementation.

Populate `context.worldObjects[1]` with the player and slots 2..164 with the
original world records. Slot 0 is reserved. The fixed array has 166 records
so the original slot-165 boundary read also has valid host storage; slot 165
is not an additional generic collision candidate or an inventory slot.
`objectCollisionsEnabled` models the DS:`D500` query gate. Enabled queries
reject source indices outside allocated storage explicitly.

Call once per game update:

```cpp
const auto result = UpdatePlayerInteraction(context, input);
```

It recomputes `playerCollisionSlot` (original DS:`DC60`), then calls
`IsKeyPressed(InputKey::InteractionKey)` only if a target was selected. A true result
invokes `HandlePlayerObjectInteraction` on that world record. No target or
no input returns `NoEffect`; the handler's other statuses, including inventory
full and unsupported interactions, propagate unchanged. Consumed objects are
excluded on the next update, which clears stale selection.
The ordinary duplicate-weapon branch explicitly clears DS:`DC60` immediately;
it now clears `playerCollisionSlot` as well as disabling that world pickup.

The hook adds no key-edge detection, debounce, or repeat policy: those belong
to the supplied input handler. It is independent of the behavior-dispatch
enable flag. It preserves strict axis boundaries, wrapped signed-word
coordinate differences/radius sums, and the original signed ABS overflow.

This is not a lift of all of `0000:D060` or a complete game loop: player
movement and automatic damage/teleporter responses remain separate work.
The ordinary door contact branch is implemented: the selected door's occupancy
byte is set even without interaction input. World-object updates and recovered
audio requests are available separately; drawing remains outside this port.

The console supports `--object target54.bin --player player54.bin --interact`
to exercise the full implemented path. Omit `--interact` to test selection
without pressing the key. Without `--player`, the existing `--interact` mode
continues to test only the handler on the explicitly supplied object.
Tests cover all 65,536 position words on each axis, all XZ radius-sum words,
mask/health filters, priority selection, both end slots, absent/disabled
collisions, input call counts, and input-driven pickups.

GameContext's normal level-player initialization uses a save-derived collision
shape, not placeholder radii: all 52 original `Data/SaveGames` player records at byte
245 have mask **3** and horizontal radius **15**; 50 have vertical radius **23**
and two have **15** (alternate state not assigned here). The normal initializer uses
23, giving object-center Y `floor + 20` at standing eye height 45. The former
10-unit vertical radius put the center at `floor + 33`, preventing overlap with
floor medicine even though Enter reached the interaction path correctly.
For a concrete fixture, `Data/SaveGames/Nice/STAND4.SPL` stores the mask at
`245 + 7 = 252`, horizontal radius word at `245 + 24 = 269`, and vertical radius
word at `245 + 26 = 271` (little endian). The existing renderer save reader also
locates this record as `299 - 54`. These observations establish saved normal
player state; **the corresponding EXE initializer has not yet been traced**.
`InitializeLevelPlayer` is a conservative gameplay-owned initialization policy,
not a claim that the original initializer has been fully recovered.
The runtime suite uses a direct `GameContext` subclass with original level assets
and checks level-11 medicine pickup, unlocked-door activation, and linked-switch
animation. The playable app displays interaction
outcomes in its window title; it does not silently discard the result.

### Inventory and explicit interaction

`HandlePlayerObjectInteraction` in [ObjectInteraction.cpp](Scaffold/ObjectInteraction.cpp)
is a separate action, never implicitly invoked by `DispatchObjectCase`.
The host passes the selected world object by reference, as the behavior
handlers do; the original dispatcher receives its word slot instead.
The renderer/game input layer should call it on an interaction request.
The console's `--interact` option models one ENTER request instead of a
per-frame behavior update.

The relevant state is owned directly by `GameContext`:

| Original DS address | Host member | Evidence / meaning |
| --- | --- | --- |
| `2E22` | `inventory[14]` | Original slots 1..14: two hands and twelve backpack slots |
| `2DCA` | `gold` | Signed word; gold pickup adds object word 34 |
| `2DE5` | `arrowPoison` | Signed word; poison pickup takes the signed maximum with object word 34 |
| `D4FB` | `hasMap` | Set by `PickableMap` |
| `DD36` | `weaponAmounts[32]` | 32 word ammunition/condition/resource entries |
| `DD76` | `weaponInventorySlots[32]` | 32 word references: zero or an original 1-based inventory slot |

The author explains that poison is applied to arrows, causing a struck enemy
to lose health over subsequent frames; it is not poison applied to the player.
The counter pickup is implemented, but projectile application and the precise
damage/time units are not recovered here.

Initialization at `0000:0E15` clears 810 bytes: fifteen complete 54-byte
records. Slot zero at DS:`2DEC` is a preview/scratch object, not a fifteenth
usable inventory slot. The host uses a local snapshot instead of storing this
UI scratch record in the inventory. The original world-to-preview copy is
confirmed at `0000:0FDA` and `0000:DCF1`; the memory-copy helper is
`2CE4:0E00`. Inventory numbering in results and weapon references remains
1..14, while the host array uses indices 0..13.

Confirmed gameplay implemented:

- **Generic pickup, including keys:** byte 50 must be nonzero. Choose the
  lowest numbered slot with health exactly zero, copy all 54 bytes of the
  snapshot, and clear only the world object's health. Negative inventory
  health still occupies a slot. Full inventory leaves the pickup in place.
- **Medicine:** first stack into the lowest numbered medicine slot whose
  health is exactly 1000, adding word 34 with 16-bit wrapping. This succeeds
  even with full inventory or a zero pickup flag. Otherwise use generic
  pickup. Picking up medicine does not heal the player.
- **Gold, poison, map:** update their global counter/flag and clear world
  health without needing any inventory space. Gold and medicine additions
  wrap rather than saturate; poison uses signed comparison.
- **Ammo:** nonzero byte 50 enables the pickup. Word 34 indexes the full
  32-entry amount table; word 44 is the signed amount, defaulted to one when
  zero. Add with wrapping and clear world health.
- **Weapons:** newly stored weapons register their inventory slot. Fireball
  amount comes from word 36; dagger, sword, and weapon IDs Stone through
  Showel get 360. Other entries retain their previous amount.
  Duplicate fireball weapons merge word-36 charges into an existing
  health-1000 weapon and update the counter. Stones and traps/poles allow
  separate duplicate objects. Other matching duplicates remain in the
  world with unchanged health but have collision type, byte 7, and byte 50
  cleared; they do not consume another slot.
- **Grouped switches:** stationary switches start forward/return movement,
  reload their countdown, and default byte 5 to 2 if it was zero. The
  existing per-frame behavior handles masks and timed return. No direct mask
  change is made by the interaction itself.
- **Object-local interactions:** puzzle tiles set word 40 to one; puzzle
  spools set velocity X to one; grandfather-clock items add word 36 to
  velocity X with word wrapping.

**Preserved but not yet understood:** generic inventory copying changes
byte 4 from 20 to 1 and copies byte 53 into word 46 when that original
byte-4 value is 20. No semantic field name is guessed for this conversion.
The 32-entry amount table starts with index 8 = 100, 20 = 1000, and 23 = 100,
all others zero. Index 8 corresponds to Showel; the author does not yet
identify indices 20 and 23. Their values are retained without speculative
resource names.

The gameplay branches for books, purchasing, rope wheels, vases/mirrors,
level exits, teleporters, sarcophagi, picture maps,
and lorries explicitly return `UnimplementedHandler` without mutating the
object or inventory. Other collision values follow the original generic
pickup tail. Door prompts and sound requests are implemented; the playable DX11
host now renders messages with the original font assets. Native message placement,
preview refresh timing, inventory-item use, and platform keyboard polling are not. Input
interface polling and collision-driven interaction are implemented by the
frame hook above. The local snapshot
does not reproduce the original UI's possibly older preview copy.
As in the original, local case effects can precede a failed generic pickup:
for example a pickable grouped switch can start moving even when inventory
is full. Invalid amount/table indices throw before accessing host memory.

See [inventory initialization, cleanup, and input evidence](inventory-evidence.txt)
and [complete interaction dispatch](player-collision-evidence.txt). Debug and
Release tests cover original capacities/defaults, all word values for
gold/poison/medicine arithmetic, all 32 ammo indices, duplicate weapons,
inventory exhaustion, exact copies/conversions, explicit-input separation,
and grouped-switch activation through automatic return.

### Ordinary doors and key unlocking

`Function_Door` implements the wrapper at `0000:14FF` and helper at
`10AC:3F56`. Activation is the ordinary Door branch of `10AC:10F5`, not a
partial stand-in for that general dispatcher. See
[switch/door evidence](switch-evidence.txt),
[unlock evidence](door-unlock-evidence.txt), and
[door integration and message evidence](door-message-evidence.txt).

Door-specific uses of shared object fields:

| Member | Ordinary-door meaning |
| --- | --- |
| `blendWeightOrSpriteOpacity` | Opening progress, normally 0..100 |
| `field_26` | Signed motion rate; opening +3, closing -3, stopped 0 |
| `field_24` | Configured automatic-close delay; signed positive enables it |
| `itemValue` | Remaining close-delay ticks |
| `behaviorParameter` | Previous contact/overlap slot; `spriteScale` is preserved |
| `velocityY` | Saved collision-map value restored when fully open, not movement |
| `groupIndexOrLock` | Shared switch-group index 0..4 |
| `doorLockOrGroupBitmask` | Signed key lock |

The handler first sets the collision radius to 54 for health exactly 1000,
otherwise 10. A zero group mask requests activation. Positive locks block
group activation; negative locks do not. Activation starts opening below
progress 100, hides the object when `modelFrame2` is zero, requests its
configured sound, and refreshes the timer. No common activation-prefix
health reset applies to ordinary doors.

Positive timers decrement without clamping. Recorded occupancy while progress
is positive refreshes the delay and reverses to +3. An expired timer at
progress 100 starts closing at -3. Movement uses low-word multiplication and
addition followed by signed comparisons. Overshoot above 100 clamps; exactly
zero configured delay additionally disables the behavior and object collision.
Landing exactly on 100 does not disable it. Negative progress clamps to zero
and requests `SoundEffect::DoorClosed` (8). Both endpoints stop motion.
Opening/closing uses the configured sound with source limit 1; the closed
effect uses limit 2. Configured sound zero is silent.

The native dedicated writer stamps nine bytes: three consecutive columns on
each of three rows. The starting column is `wrap16(x - 137) / 25`; each row
is `wrap16(162 - wrap16(z - 38) / 25 + offset)`, with offset -1..1.
The stride and destination offsets wrap as unsigned words. A fully open door
writes the low byte of `velocityY`; all other progress writes `0x38`.
This is not equivalent to shifting the point writer near signed-division
boundaries. Wrapped header access is preserved, including changes to the next
row's stride; each row is allocation-checked before writing. Invalid host
addresses and missing map memory throw explicitly.

The final object-overlap query excludes player slot 1 and stores the selected
slot in the occupancy low byte. `UpdatePlayerInteraction` supplies the separate
player-touch marker before polling the interaction key. The host must schedule
that contact hook before door updates when it wants same-frame reopening.

Manual interaction requires health exactly 1000 and zero motion rate. A
positive lock searches all fourteen inventory records for the first live
matching Key; success zeros only its health, clears the lock, and activates.
Negative locks and no-key cases store `"abgeschlossen"` for 60 ticks only if
no prompt is active. A wrong key overwrites any prompt for 90 ticks with the
original German wrong-key bytes (`0x81` for the umlaut). A selected key ID of zero
takes the no-key path. Nonstandard-health doors retain the original generic
pickup tail if explicitly flagged pickable.

The consumed-hand helper `1E48:CAFE` queues the held-item overlay index in both
drawing buffers' redraw lists, stored as short strings of index bytes; it does
not clear additional inventory/world records. That rendering housekeeping is
omitted. See the [UI architecture](HighLevelStructure.md#per-buffer-redraw-lists).
Specialized doors, general target activation, and the full player controller
are still outside the implemented scope.

Tests cover all 65,536 progress words at representative rates/ticks, extreme
word arithmetic, delay signs, one-shot endpoints, group locks, occupancy,
wrapped collision footprints, sound source limits, all fourteen key slots,
feedback gating, and exact inventory-byte preservation. The console provides
a valid opening-door fixture with `--behavior Door --ticks 3`.

### Gameplay user messages

`GameContext::PromptUserMessage(text, duration)` is an overridable app hook
corresponding to `1E48:2459`. The core passes borrowed original DOS-encoded bytes
and the native duration; it does not store text or maintain a countdown.
`IsUserMessageActive()` lets locked-door feedback respect the app's current
message. Hosts without a presenter can retain the default no-op prompt hook
and inactive result. Successful level completion sends an empty reset request.

The DX11 app's `GameAppContext` derives directly from `GameContext`. Its
`PromptUserMessage` override copies the complete message without transcoding
or the original Pascal string's 150-byte limit and stores the signed-word absolute duration,
including the original -32768 overflow edge case. Empty input clears both.
Its `UpdateUserMessage(ticks)` follows the countdown part of `1E48:3F2A`:
only a positive lifetime decrements, without clamping. The app advances
the timer once after an advanced tick using the separate frame count;
paused, loading, failed and standalone object updates do not advance it.
Negative timer updates are invalid.

The same override builds or replaces the text mesh directly; expiration removes
the mesh. There is no intermediate host adapter, forwarded callback, message
snapshot polling API or per-frame text callback. The app loads the same three
`SCHRIFT0/1/2.AN2` font animations as the desktop sample, keeps their assets alive
while text meshes reference them, and displays the current positive-lifetime
message in light orange (`RGB(255, 192, 128)`) at the viewport center. The font mapping
uses the original DOS byte values directly, including German glyphs. Identical-text
refreshes reset the app-owned timer without rebuilding the mesh; no wall-clock fade extends expiry.
Reloads clear the mesh, and resize updates its center without rebuilding text.
Core integration tests trigger a locked-door message through a direct subclass,
checking hook dispatch, app-owned activity gating, identical refresh and load reset.
They do not instantiate the DX11 presenter.

Negative prompt durations originally selected alternate screen placement.
Their magnitude remains supported, but the screen-layout globals, calculated
pixel position, and text drawing are deliberately left to the host.
Core hook tests cover all duration arguments, byte preservation, replacement rules,
and the executable's wrong-key text. App-owned rendering and countdown remain
outside the headless core suite.

### Flies: randomized frame blending and buzzing

`Function_Flies` implements behavior 16, wrapper `0000:1690`, and the complete
helper `10AC:C9EC..CAEB` in
[behavior-candidates-evidence.txt](behavior-candidates-evidence.txt).
It uses existing object fields, context RNG, and positional audio; it introduces
no additional mutable state or rendering code.

Each update, in original order:

1. Display type `VectorBrightness2` (22, the post-burning state) sets health
   to zero. The handler still finishes this update; subsequent dispatch skips
   the inactive object.
2. Draw `Random(5)` once. Add
   `(signed itemValue + randomValue) * tickCount` to the signed blend phase,
   retaining the low word before any comparisons.
3. Set `winX = (old winX + 5) & 511`, `winY = (new winX + 3) & 511`, and
   `winZ = (new winX + 7) & 511`. These are per-call changes, not tick-scaled
   angular velocity.
4. While the signed blend phase is **greater than 100**, subtract 100,
   copy `modelFrame2` to `animFrame`, and choose `Random(3) + 1` until the
   new `modelFrame2` differs from that previous frame. Large phase values
   can cross several times. Exactly 100 stays unchanged; negative phases
   are not clamped or normalized.
5. Request fixed effect **74**, `SoundEffect::Flies`, with the object's world
   slot and source-instance limit **1**. Attenuation uses twice the wrapped
   listener distance, not twice the computed volume. The object's configured
   `soundEffectId` is not read by this behavior.

There is no X/Y/Z translation. Zero-tick updates still draw the rate RNG,
change angles, process any preexisting phase crossings, and request sound.
Frame rejection and sound-variant draws share the context seed, so preserving
their order matters even when audio is muted, inaudible, or channel-limited.
As with other sound-producing handlers, standalone objects work without an
audio backend; a configured backend requires a source in `worldObjects`.

Tests compare complete object bytes and RNG state across all low-word seeds,
all phase/rate/X-angle words, representative extreme ticks, valid and unusual
starting frames, and every display byte. They also cover attenuation overflow
and suppression, unavailable/muted audio, sample selection, the one-instance
source limit, burning-prefix integration, and inactive/disabled dispatch.
The console reports blend phase, previous/current frames, and angles for
`--behavior Flies --ticks 3`; supplying the sound table enables sample logging
through the existing console audio backend.

### IdleAnimation: object-local blend phase

**Observed:** behavior 15 enters at module-relative `0000:1666`. Its entire
case is 13 instructions, with no helper calls or direct DS global accesses.
The case reads only two object words and the tick-count argument, and writes
only the word at byte offset 42:

| Byte offset | Named member | Role in this behavior |
| --- | --- | --- |
| 34 (`22h`) | `itemValue` | Per-tick blend-phase increment; animation-rate interpretation |
| 42 (`2Ah`) | `blendWeightOrSpriteOpacity` | Current/output animation blend phase |

Evidence is in [simple-behavior-evidence.txt](simple-behavior-evidence.txt).
The report includes the comparison/case through its jump to common
postprocessing at `0000:5110`, plus the common object-update entry.
These ranges can be reproduced with `InspectCode.java` using their report
headings. No instruction from another behavior is needed to explain this case.

Its exact arithmetic is:

```text
sumBits = low16(objectWord34 * tickCount + objectWord42 + 200)
sum     = interpret_as_signed16(sumBits)
objectWord42 = signed_remainder(sum, 200)
```

`MUL` is unsigned, but only its low word survives the subsequent additions
and `CWD`. `IDIV` then computes a signed remainder, which the `XCHG AX,DX`
moves into AX for the final store. Signed remainder follows truncation toward
zero; it is not an always-positive mathematical modulo. Preserve the 16-bit
wrap before interpreting the sum as signed when implementing it.

For ordinary values without overflow, this reads as:

```text
phase = (phase + rate * ticks + 200) % 200
```

Example: phase 195, rate 3, and four ticks produce phase 7.
The `+200` also supports modest negative increments before the remainder.
Unusual inputs or overflow can produce negative remainders, so do not
silently normalize these to 0..199.

**Repository connection:** `PropertyAccessorBase::GetAnimBlendWeight` in
[GamePlay.cpp](../NewRenderer/SharedSource/base/GamePlay/GamePlay.cpp) reads
that same signed word. For a normal phase cycle it converts 0..100 into
0..1, and 100..200 back into 1..0. This is a triangular/ping-pong vertex
animation blend, consistent with the renderer's `IdleAnimation` comment
mentioning a spider web. This case does not change animation-frame bytes,
position, velocity, collision data, or health.

The case is object-local, not the whole object update: shared preprocessing
and postprocessing still surround it and use global state.
[Function_IdleAnimation](Scaffold/BehaviorHandlers.cpp) now implements this
case using the named object fields and reports `ExecutedCase`. It performs
the low-word multiply/add before interpreting the result as signed and taking
the remainder; no extra globals or helper parameters are introduced.

Tests verify phase 195 + rate 3 * four ticks produces 7, negative increments,
zero ticks, signed overflow, and negative remainders. They exercise every
16-bit phase and every 16-bit rate at representative tick counts against an
independent modular-arithmetic oracle, compare all 54 object bytes to ensure
only the phase word can change, and check health/dispatch gates. The case is
implemented and host-tested, not DOS-runtime validated.

### BurningFireplace and FireParticle

For executable-wide allocation sites and other runtime-created behavior
leads, see [Object-allocation discovery](SpawnDiscovery.md). This separates
provenance-linked assignments from unresolved paths and does not implement
all newly indexed child behaviors. Behavior 18 and the crater smoke emitter
have subsequently been implemented as described below.

Implemented in [BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp):
`BurningFireplace` (22), entry `0000:18C6`, and `FireParticle` (21),
entry `0000:181E`. The latter is the recovered name for the former
`Behavior_21`, absent from the renderer's behavior enum.
[fireplace-evidence.txt](fireplace-evidence.txt) preserves both complete
wrappers, the fire branch of factory `10AC:091C`, its return, the signed
random helper, and the clearing allocator. Range headings are reproducible
`InspectCode.java` arguments.

The emitter's operation order is:

1. Draw `Random(20)`. If the signed word at offset 46,
   `doorLockOrGroupBitmask`, is **greater than or equal** to that result,
   request `SoundEffect::Burning` (62), distance scale 2, source object,
   instance limit -1. Sample selection can consume another random draw,
   even without audible playback, according to the existing sound policy.
2. Iterate X offsets -2..2, then Z offsets -2..2. Each of the 25 samples
   contributes `(5 - abs(x) - abs(z))^2 * Random(30) / 8` at the wrapped
   world position `(posX + 40*x, posZ + 40*z)`. This is a sample grid,
   not 25 distinct lighting tiles: several samples can accumulate in the
   same cache cell. All draws occur even outside the active cache window.
   Lighting is independent of emission intensity.
3. Draw `Random(100)`. Spawn **at most one** particle if the signed,
   low-word product `intensity * tickCount` is strictly greater than this
   result. This is not a per-tick spawn loop.
4. The spawn position is `(posX, low16(posY + collisionRadiusY), posZ)`.

`GameContext::SpawnFireParticle`, defined alongside every other context
member in [GameContext.cpp](Scaffold/GameContext.cpp), ports the native
factory's collision-49 (`DamageObj`) branch, not its other fragment types.
It uses the existing zero-health allocator, including cleared fallback
slot 165. Coordinates are captured before allocation; the source collision
type is read afterward. If allocation clears the source itself, its now-zero
collision byte produces no particle and consumes no initialization draws.
Other nonzero collision types are explicitly rejected by this fire-only API;
unrelated generic-factory fragment dispatch is not claimed as implemented.

Initialization leaves all unspecified bytes zero:

| Field | Initial value |
| --- | --- |
| Model / animation / second frame / frame | 39 / 3 / 2 / 5 |
| Renderer placement mode | 130, renderer-owned configuration |
| Blend/opacity word | 100 |
| Vertical velocity | `600 + Random(500)` |
| Behavior | `FireParticle` (21) |
| Angular velocity (`field_24`) | `Random(4)` |
| Lifetime (`health`) | `50 + Random(60)` |
| Initial display mode | 5 (`VectorUnlit`), not a sprite mode |
| Vertical acceleration (offset 46) | `clamp(-((RandomSigned(30) + velocityY/health) SAR 1), -30, 1)` |
| XZ / Y collision radii | 10 / 10 |
| X / Y angles | `Random(512)` / `Random(512)` |

The initialization draw order is 500, 4, 60, 61, 512, 512.
`RandomSigned(radius)`, recovered from `1E48:0AA0`, draws
`Random(2*radius+1)-radius` and replaces zero with **positive radius**;
it does not reroll. Its scaffold interface accepts radii 0..32767.

Each particle update:

1. Adds low-word `velocityY * ticks` to the fixed-point Y accumulator
   (`itemValue`), transferring the signed high byte into `posY`.
2. Moves the blend/opacity word toward zero by low-word
   `ticks * Random(3)`, using the recovered signed-step helper.
3. Adds low-word `acceleration * ticks` to vertical velocity.
4. Adds low-word `field_24 * ticks` to `winY`, without masking to 511.
5. Sets display mode to `clamp(37 - (health SAR 2), 30, 36)`.

There is no horizontal motion and no health decrement inside this handler.
The shared update prefix supplies lifetime countdown when object collisions
are enabled, including the final update at health zero. A newly spawned
particle in a later world slot updates during the same world pass; fallback
slot 165 does not. Physical up/down interpretation and visual meanings of
display modes 30..36 are not inferred merely from these integer operations.
This is distinct from behavior 24, `SmokeFading`.

All six extracted fireplace records corroborate the source collision type:
level 23 file records 3, 4, 6, 7 and 12 start with intensity **0**;
level 24 record 42 starts with intensity **7** (zero-based file indices,
not assumed world-slot indices). Zero intensity disables particle emission
but retains lighting and the inclusive 1-in-20 sound chance.
With seven and 15 ticks, the product is 105, so one spawn is guaranteed.

[FireplaceTests.cpp](Scaffold/FireplaceTests.cpp) compares full 54-byte records,
sound selection/admission, all lighting cells, RNG state, signed-word
arithmetic, zero/large ticks, allocation alias/fallback/negative-health
cases, same-pass updates, lifetime gates, and all six actual asset records.
Run the console's minimal synthetic swamp-style demo with
`--behavior BurningFireplace --ticks 15`; real object input is never replaced.
The code is host-tested against static instruction-derived expectations,
not validated against a running DOS capture.

### RatAndSnake: movement and separate contact damage

Behavior 13 is implemented directly in `Function_RatAndSnake` in
[BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp). The wrapper
`0000:15EF..1617` calls native `10AC:3058..3390` once per behavior tick,
including repeated calls after a helper changes behavior or health. Zero ticks
do nothing locally. [rat-snake-evidence.txt](rat-snake-evidence.txt) preserves
the complete wrapper and helper; supporting instructions and constants are in
[rat-snake-support-evidence.txt](rat-snake-support-evidence.txt).

The native health gates are exact, not general alive/dead tests:

- **Health 1000:** normal movement. When DS:`D500` is enabled and the proposed
  map pixel's low three bits are **not** 1, add whole-unit X/Z velocities.
  Otherwise turn `winY` by 16 modulo 512. The predicate is deliberately not
  inverted to a guessed "walkable" interpretation.
- A signed-positive `field_24` gives a `1/field_24` steering chance toward
  DS:`D4D2/D4D6` (the same position globals exposed as sound listener X/Z).
  Convert the wrapped difference to an angle, normalize the angle pair,
  turn by at most 13, and recalculate velocity with the clamped sine table.
  With probability 1/20 clear both horizontal velocities.
- Set Y to the terrain-only floor. `Random(field_28)==0` perturbs heading by
  `RandomSigned(20)`, calculates clamped-table velocity and sets `field_28=15`.
  Initial zero is intentional: **native Random(0) advances the seed and returns
  zero**, so it guarantees this first direction selection. This corrects the
  scaffold's earlier explicit rejection of zero bounds, including smoke radius 0.
- When moving, update animation phase to signed
  `wrap16(2*itemValue*itemValue + phase) % 200`.
- Draw one signed turn of 64 units; repeatedly sample the 3x3 map neighborhood
  at the proposed position with step `wrap16(itemValue+1)` and mask `38h`.
  Nonzero X/Z corrections cause another turn and an **ordinary-table**
  direction calculation. Retry at most 16 times. Reaching attempt 16 invokes
  the native fallback even if that attempt found no correction.
- Native `10AC:1F5A` uses DS:`D501`: true clears health; false disables behavior,
  sets phase -100, adds 50 to Y and shows `WC-Error Obj <slot>` for 50 ticks.
  `discardBlockedObjects` exposes this flag; the host default is diagnostic
  mode, not a claim that the native startup flag has been recovered.
- Animation 1 has a 1/300 chance per tick to request sound effect 17 with
  source-object slot, limit 1 and the volume calculated before the tick loop.
- **Health >1100:** decrement vertical velocity, add it to Y, clamp to terrain
  and clear velocity on impact; increment phase and disable behavior when the
  signed phase becomes positive. Other health values perform no local work.
  The shared prefix still counts down health below 1000 before dispatch.

Reusable math now includes both direction-table variants, native angle-pair
normalization, and the vector-angle helper. The latter preserves the signed
ABS overflow at -32768, Pascal Real48 rounding between stages, the executable's
approximate constants `pi=3.1415927`, `pi/2=1.57079635` and `128/pi`, and
nearest-even final integer rounding. It does not simply replace the routine
with an unqualified `atan2`. Host libm is used for arctangent; bit-for-bit
equivalence to the DOS software transcendental implementation is not claimed.
The neighborhood reader shares the existing raw map reader, preserves in/out
corrections, wrapped arithmetic and center output, and is independent of the
object-collision gate.

#### Where player damage happens

It is **not in the movement helper**. The player-contact collision cascade at
`0000:DB1A` routes **collision type 13** to `0000:DB42..DB7A`, together with
several other damaging collision types. If the object's **`behaviorParameter` byte (offset 52)**
is positive, it multiplies that byte by the dword at DS:`0300`, passes the
low-dword product and player meter DS:`2DBA` to `1E48:D579`, and sets
DS:`D514` to 8. The four level-30 snake records have factor **3**; the
other 36 initial rat/snake records have factor **1**.

`1E48:D579` is a shared signed fixed-point meter update: the current value is
a dword at meter +4, with maximum at +2. For the player meter it suppresses
the incoming delta when DS:`2DDD` is nonzero, clamps the meter and can invoke
the shared player-death path on depletion. Subsequent skeleton recovery identified
DS:`0300` as the immutable signed dword **-29491** and implemented the player
meter update/depletion state; see [skeleton recovery](#skeleton-movement-combat-and-death).
The automatic rat/snake contact cascade and the DS:`D514` presentation consumer
remain separate player systems; **rat/snake contact damage is not implemented**.
`UpdatePlayerInteraction` still handles interaction-key/door contact only.
Do not subtract guessed damage from the player's object `health` or add
damage in the rat movement handler: that would use the wrong storage or
duplicate the original shared contact path.

[RatSnakeTests.cpp](Scaffold/RatSnakeTests.cpp) covers 65,536 seeded movement
fixtures, exact health gates, tick/RNG order, steering, zero bounds, both
direction tables, angle normalization, all 512 binary neighborhood patterns,
16-attempt diagnostics/discard, sound admission and **all 40 original records**.
Asset checks use explicit dummy terrain/map, not native level reconstruction.
This is statically recovered and host-tested, not DOS-runtime validated.

### Skeleton: movement, combat and death

Behavior 30 dispatches `10AC:4DAB` once per behavior tick. The full local skeleton
routine is implemented in [BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp),
with supporting context operations in [GameContext.cpp](Scaffold/GameContext.cpp).
Evidence is split into the bounded exports
[dispatch](skeleton-dispatch-evidence.txt),
[movement](skeleton-movement-evidence.txt),
[animation/map predicates](skeleton-support-evidence.txt),
[combat and damage](skeleton-combat-evidence.txt),
[fragment creation](skeleton-fragments-evidence.txt),
[meter initialization and fragment physics](skeleton-lifecycle-evidence.txt),
[fragment tail](skeleton-fragment-tail-evidence.txt),
[water effects](skeleton-water-evidence.txt), and
[sinking transition](skeleton-sinking-evidence.txt).

**Movement and animation**

- Only health **1000** executes the live path. Health 1010, present in three
  original level-11 records, is deliberately not treated as an awake skeleton.
- Each substep samples terrain before moving and adds the object's vertical
  collision radius. State `itemValue == 0` adds the stored X/Z velocity even
  when the general object-collision-enable flag is off.
- At tile centers (`abs(position % 100 - 50) / 2 < 2` on both axes), select
  pursuit along the dominant player-relative axis, with Z winning ties.
  Probe 25, 50 and 75 units ahead against mask `0x38`.
- A blocked path, or a one-in-eight wander choice, scans four cardinal paths
  starting from a random index. **The last clear path wins**, not the first.
  Movement is two units along the chosen axis. The existing DS:`D4FA` flag
  suppresses pursuit as well as positional sound.
- Native signed-word arithmetic, angle normalization, randomized turn limits,
  frame-pair blending, and walking/attack facing rules are preserved.
  Walking sprite direction uses the existing applied player heading DS:`D4D0`.
- Distance and normal sound volume are cached once at dispatch entry, before
  all substeps. Footsteps (effect 106) use three times that cached distance;
  effect 19 accompanies attack frame three.

**Attacks and player life**

- Start an attack when cached distance is **38..60** and walking, or when
  `doorLockOrGroupBitmask > 0`. Choose animation 4 or 5, then clear the request.
- On an animation advance to frame four, hit if cached distance is **<72**.
  A sword selected at DS:`DDB6`, with weapon-animation position **30..40**
  (DS:`DDBD`), parries: set DS:`DDBB` to -4 and play effect 39 at full volume.
  Otherwise set hit-effect ticks DS:`D514` to 5 and update player life.
- EXE data at DS:`0300` is `CD 8C FF FF`, signed **-29491** (16.16 units).
  Multiply it by the object's `behaviorParameter` byte. This is not a
  subtraction from `worldObjects[1].health`.
- Native initialization `0000:0E64..0E71` calls `1E48:D4F7` with current and
  maximum **360**, type 1. `playerLife` retains the signed 16.16 dword;
  `playerLifeMaximum` retains the maximum word. These are context-owned,
  initialized once and preserved across ordinary level loads.
- `ApplyPlayerLifeDelta` implements the player-specific `1E48:D579` branch:
  wrap the dword sum, suppress the delta when DS:`2DDD` is nonzero, clamp,
  and install the `1E48:D544` depletion state once. This sets life to zero,
  hit-effect ticks to 1000, DS:`D4B4` to -1, DS:`D4FA` true, and slot-1
  `velocityZ` to 400. The partial keyboard controller stops accepting movement
  in this state rather than overwriting the native death view marker.

**Skeleton death and fragments**

- Health **3000..3050** calls the collision-type-30 branch of `10AC:10F5`
  with damage 1000. Health **3051..3900** counts down one per substep.
- `DamageSkeleton` subtracts damage from a positive `groupIndexOrLock`.
  A surviving skeleton plays effect 11 when damage exceeds one.
  Once the field falls below one, create eight fragments and clear source health.
  This is the recovered skeleton-specific damage branch, not a guessed general
  object-activation implementation.
- Factory `10AC:091C` receives part selectors **1,2,3,6,4,260,4,260**.
  Preserve the 260 angle bias before masking the animation selector to a byte.
  Part 6 is a **pickable sword** (collision type Weapon, item value 2), not a
  bone. Allocation follows the existing clear/free-slot/fallback-165 behavior.
- The model-ID >=30 fragment paths for subtype 8 (skeleton body parts)
  and subtype 0 (including the dropped sword) are implemented in behavior 1:
  gravity, bounce/damping, ceiling/wall reflection, rolling/blending, and settling.
  Other behavior-1 styles, including model-metadata-dependent vector fragments,
  still return `UnimplementedHandler` without mutation.
- Water contact creates the native `WaterImpact` (33) animation; ordinary
  water leaves a five-tick remnant, while effective-surface value 16 installs
  the native `SinkingObject` (118) path. Their update cases are implemented too.
  `SinkingObject` moves Y toward the terrain floor by at most `tickCount` per
  update; it does not itself remove the object. These names replace the numeric
  placeholders without changing the original behavior values.
  The native wall-bounce sound uses an uninitialized local source ID on these
  fragment paths; the port deliberately uses the actual fragment as source.

**Nonfatal sword-hit particle: identified separately**

The sword path (`0000:B8DB`, selected weapon 2) creates a temporary behavior-2
contact object through `10AC:0384` at `0000:B9C9..B9DB`.
See [sword contact evidence](skeleton-sword-contact-evidence.txt).
Its collision handler `0000:1198` finds another object and initially clears
the contact's health. For collision type **30 (Skeleton)** or 35, however,
`0000:11E3..11F8` calls `10AC:07F2` with **frame 2** and the target's X/Z,
then applies damage through `10AC:10F5`.
See [hit dispatch evidence](skeleton-hit-dispatch-evidence.txt).

That initializer is already recovered: it **reuses the contact object** as
behavior **18** (`DriftingParticle`),
model 39, animation 3, frame 2, second frame 1, blend 100 and health 50.
It preserves the contact Y and sets X/Z to the hit target's position, with
outward drift derived from the contact's former X/Z. This runs for a
nonfatal hit too; it is not the behavior-1 death-fragment factory.
Behavior 2 is now named `WeaponDamageCollider`; its collision path and the
shared damage/activation helper are implemented. The player sword driver
that creates these temporary contacts is now implemented below.

**Integration boundary:** no unknown global blocks the recovered skeleton
routine. Dagger/sword input, attack timing and automatic contact creation are
implemented in [the melee sequence](#player-melee-attack-sequence).
Remaining dormant-object activation drivers, meter HUD, damage-screen
effect, death camera animation and menu/restart flow are not implemented here.
The new weapon/parry fields and `DamageObject` expose their native contracts
for that separate work. Merely pressing Enter is still interaction, not attack.
The DOS death-path recording/string update is presentation-specific and is not
reproduced. This is a static recovery with host tests, not DOS-runtime validation.

[SkeletonTests.cpp](Scaffold/SkeletonTests.cpp) checks 65,536 independent
full-record/arithmetic/RNG fixtures, all four-direction obstacle combinations
at all three probe depths, strict distance/parry/frame gates, cached sound
distance, meter suppression/depletion, eight-part creation and settling,
fallback allocation, water animation, and all **15** original skeleton records
(models 37/38 in levels 0/11/13).

### FloatingDebris: floating underwater parts

Behavior 117, formerly `SwimmingParticles`, is named `FloatingDebris` at the
author's request: parts floating in the water near the large octopus, not smoke
or an emitter. The behavior name override survives scaffold regeneration;
the renderer's original name and collision enum are not changed.

The wrapper at `0000:372C` calls the complete movement helper
`10AC:AF92..B2C7` only when `objectCollisionsEnabled` (DS:`D500`) is true.
The helper's logic is implemented directly in `Function_FloatingDebris` in
[BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp), with reusable context
operations defined in [GameContext.cpp](Scaffold/GameContext.cpp) and
instruction evidence in
[floating-debris-movement-evidence.txt](floating-debris-movement-evidence.txt).
It allocates nothing and requests no sound:

1. Save the original `winX`. Object overlap, or player overlap if no object
   overlapped, sets `field_28 = 100 + Random(300)` and replaces vertical
   velocity with its wrapped absolute value. Player overlap ignores player
   health, shares the byte-7 mask, and uses strict signed-word axis bounds.
2. If `behaviorParameter` is nonzero, `Random(behaviorParameter) == 0` clears `field_28`.
   The separate `spriteScale` byte is not used for this probability.
3. Execute exactly `tickCount` iterations. Signed-positive `field_28` loses
   `Random(2)`; there is a 1/10 chance to perturb heading `field_24` by
   `RandomSigned(20)`, modulo 512. `winX` and `winY` advance by random amounts
   bounded by the post-decrement state shifted by four/five, plus one.
   A 1/50 chance chooses vertical velocity `Random(30)-12`; Y moves by its
   sign when its wrapped absolute value exceeds `Random(10)`.
   Nonpositive state instead moves `winX` toward 256 by one, decrements Y
   with probability 1/3, and with probability 1/300 selects `Random(100)`
   as the next state. High-bit-set state words therefore take the idle branch.
4. Once per call, including zero ticks, decrement Y with probability 1/3.
   Clamp it from below using **terrain interpolation**, not the flat-surface
   floor shortcut; add the Y radius except for collision type 20.
   Clamp from above to the wrapped negative depth of **surface slot 1**,
   independent of the local terrain's surface flags. That lookup updates
   `floorSurfaceValue`. If this upper clamp fires, replace `winX` with the
   original saved angle plus a step toward `256 + RandomSigned(8)`, bounded
   by the wrapped `6*ticks`; do not use the loop-modified angle here.
5. Calculate X/Z displacement from heading and signed `field_28`, using the
   ordinary 65536-scale sine table, not the clamped table used by `Sine()`.
   Native `1E48:2E0A` keeps the signed product's high word for X and negates
   the cosine product's high word for Z.
6. Probe three units along each displacement's sign, separately for X and Z,
   before moving either coordinate. A probe succeeds iff `(mapByte & 39) == 1`.
   Failure toggles heading bit 8 and negates that axis's displacement; two
   failures toggle it twice. Finally, native `10AC:37E4` applies fixed-point-8
   X/Z movement, using `velocityX` and `velocityZ` as fractional accumulators.
   Horizontal movement is once per call and has no additional tick multiplier.

The floor implementation now exposes its terrain-only path, reusing the existing
stacked/repeated-floor rules without duplicating interpolation. Missing geometry,
surface-slot metadata, or map memory fail explicitly. There is no guessed water
height or renderer dependency. Full rendering/postprocessing remains outside
this recovered handler.

[FloatingDebrisTests.cpp](Scaffold/FloatingDebrisTests.cpp) compares all 54 bytes
and RNG state against an instruction-derived oracle over 65,536 seed fixtures
and all 65,536 drift-state words. It covers all word angles at boundary speeds,
zero/max ticks, player/object contact, scale-byte probability, all map-byte
values, independent axis reflections, floor/radius/type-20 rules, surface side
effects, collision gating, missing inputs, and shared lifetime expiration.
All **24 original debris records** are also checked with explicit dummy
terrain, surface metadata, and map, not a reconstruction of their native level.
This is statically recovered and host-tested, not DOS-runtime validated.

The synthetic `--behavior FloatingDebris --ticks 3` console fixture explicitly
supplies demo surface-slot metadata. Real-object input still requires the host
to supply level information; geometry streams alone do not contain that table.

### Weapon damage and shared activation

Evidence: [complete damage cascade](damage-activation-evidence.txt),
[direct helpers](damage-support-evidence.txt),
[projectile subtype 8](damage-projectile-evidence.txt) and
[projectile floor clamp](damage-projectile-tail-evidence.txt), together with
the [weapon-contact case](skeleton-hit-dispatch-evidence.txt).
All addresses below use the original executable hash recorded in those files.

**WeaponDamageCollider (2, `0000:1198`):** the author confirms this is a
generic weapon-spawned damage collider. Once per dispatch, even with zero
behavior ticks, it calls `FindCollidingObject`. No hit leaves the record alone;
the query excludes player slot 1 and obeys the global object-collision gate.
A hit first clears the contact's health, then selects the following path:

| Target collision type | In-place particle frame | Damage |
| --- | --- | --- |
| 30 Skeleton, 35 PushingEnemy | 2 | byte 52 |
| 36 SpiderLarge, 38 Devil | 4 | byte 52 |
| 26 DestroyableBarrier, 126 CeilingConcreteSlabStake | 3 | byte 52 |
| 60 Dragon | None | byte 52 divided by 2 |
| Any other collision selector | None | byte 52 |

Particle conversion calls `InitializeDriftingParticle` before reading the
damage byte and invoking `DamageObject`. It allocates nothing, preserves Y
and byte 52, restores health to 50, and installs behavior 18. Without a
conversion, the consumed contact remains at health zero unless the target's
own secondary allocation reuses its slot. Damage is not multiplied by ticks.

**Shared `DamageObject` (`10AC:10F5..1F49`):** the complete native selector
cascade is ported in [GameContext.cpp](Scaffold/GameContext.cpp). Amounts are
signed words: positive values are damage, zero is also used for activation,
and -1 selects special destruction for some types. It caches target-local
audio volume and the old strength/group word, performs the exact zero-amount
activation prefix, then subtracts the amount only from positive strength.

For amount zero and health strictly between 1000 and 2000, collision types
0, 103, 67, 48, 88, 6, 2, 76, 47, 21 and 9 normalize health to 1000.
Collision 125 instead changes display byte 4 to 120; other display values
survive. This is a fixed native list, **not** a blanket wake-up for all enemies.
In particular, this prefix does not normalize skeleton health.

The target-specific branches are:

| Collision selector | Recovered effect after the common prefix |
| --- | --- |
| 126 | Keep the strength subtraction; no additional effect |
| 59 | Add amount to Y with word wrapping |
| 93 | Set X angle 128 only when amount >100 |
| 91 | Play configured sound, restore original strength |
| 92 | Below 200 restore strength; otherwise break when depleted, stop sound channels, optionally copy the broken shell and initialize a key in the original slot |
| 20, 21 | Only amount -1 clears the mask and sets lifetime 10; otherwise restore strength |
| 138 | Copy the complete offset-46 word into behavior, clear collision type, set display 1 |
| 5 | Amount zero and lock <1 start/refresh ordinary-door opening |
| 8 | Clear health, masked wall collision pixels, and the exact wall-texture cell; retain the native conversion of object X/Z into one-based tile coordinates |
| 105 | Clear collision type and write the low item-value byte into the floor-object plane, **not** the floor-height plane |
| 35 | Native death animation setup or sound/knockback for a surviving target and amount >1 |
| 36 | Native spider death setup and audio cancellation; paired hit/death sounds |
| 37 | Install the harvestman's final frame, counters and zero blend |
| 38 | Install devil death frames/counters, or hit sound above amount 1 |
| 30 | Existing skeleton damage and eight-part/sword factory |
| 60 | Enter behavior 61 once on depletion; otherwise thousand-bucket roar test and vertical reaction |
| 31 | Cockroach/spider phase transitions, special mask 131 retention and hidden-object deletion |
| 65 | Shark death setup at health 1000 and a water impact on every call |
| 66 | Stop crab behavior and install its final animation on depletion |
| 4 | Damage-activated switch group dispatch, strength latch 1 and animation rate 10 |
| 14 | Bat falling/death state |
| 13 | Rat/snake death state at health 1000, with model-animation-specific sound |
| 41 | Clear health only when it is exactly 1000 |
| 22 | Back up twice the horizontal velocities, play effect 37/source 467, emit nine fragments/smoke particles and clear health |
| 10 | Clear collision fields and increment the frame byte with wrapping |
| 26 | Add low-word `byte52*amount` to offset 36; compare its unsigned high byte to offset 46 and install 32000 on destruction |
| 63 | Only amount 5 digs; add byte 52 to the progress word, set DS:DDBF=2 for the shovel, and play effect 54; otherwise restore strength |
| 68 | Dog death setup, or change live attacking phase 1 to -2 |
| 34 | Anaconda-head death behavior 87, or live hit counter 10 |
| 33 | Anaconda-tail death health/animation |
| 88, 89 | Attached-part/head death behavior 88; type 88 also subtracts offset 40 from its parent object's offset 40 |
| 61 | At health 1000, depleted strength and amount >=200, emit exactly twenty subtype-8 projectiles and set lifetime 35; weaker damage restores strength |
| 49 | Amount -1 installs native behavior value **49**, swaps frame bytes, sets display 80, and emits five smoke particles |
| All other byte selectors | Restore the original strength/group word; activation-prefix effects remain |

Collision selectors 22 and 138 now have numeric placeholder enum members
`Type_22` and `Type_138`; their visual identities remain unassigned. Behavior 1
has deliberately **not** been renamed. Original behavior values 49, 87 and 88
are retained even where their existing author-derived names do not describe
every use of the shared updater.

The small spider sound-pair table starts at DS:`0482`: pair 0 is death/hit
16/13, pair 1 is 26/26, and the remaining pairs through DS:`04C1` are zero.
Its file offset is `0x2D40 + 0x2E9E*16 + 0x482`. Pair indices outside 0..31
are rejected instead of reading neighboring globals as a table.
Native `1E48:D20C` clears the four ten-byte channel records, so
`StopAllSoundEffects` calls `AudioHandler::StopAllSamples` and clears source
tracking. An attached backend must implement cancellation; the default throws
explicitly rather than pretending playback stopped. No audio backend is required
for headless recovery. Configured sound zero remains silent.

`ActivateObjectGroup(group)` ports `10AC:2060`: compare health against wrapped
`group+1000` and call `DamageObject(target,0)` in slot order **2..140**.
Ordinary switch interaction and ordinary door activation now reuse these
shared paths. Damage-activated switch rate 10 is distinct from explicit
interaction's `field_24` rate; both native entry points are preserved.

**Direct dependencies:** `ApplyObjectKnockback` retains low-word products,
Manhattan normalization, a 30-unit amount cap and the original correction-only
wall test. Symmetric obstacles can cancel corrections; no extra center-pixel
gate is invented. Native division-by-zero/overflow becomes an explicit host
exception. Collision-22 breakup preserves its three initial RNG draws even
when the record is subsequently overwritten by the smoke initializer.
All allocation occurs in native order, including fallback 165 and source
self-aliasing. Twenty activation projectiles use the reclaiming allocator;
the nine breakup particles and five smoke particles use the first-free allocator.
The existing sword-fragment and water-impact factories also now retain their
literal native display byte **4**, rather than the enum's differently valued
`VectorUnlit` name.

FELD plane 1 is retained in `Level::floorObjects`. Wall/floor-object writes
increment `Level::geometryRevision`, providing a host-visible invalidation
token. The DX11 sample still caches its static meshes and does **not** yet
consume that revision; these changes implement core geometry state, not a
renderer mesh-rebuild feature.

**Boundary:** dagger/sword input and contact creation have subsequently been
implemented below. Other weapon families, automatic player-contact damage,
and all downstream behavior updaters are not claimed
implemented by this shared routine. Transitions into unrecovered behaviors
remain visible through their normal `UnimplementedHandler` status. This work
recovers every target branch of the shared helper, not the rest of every
enemy or weapon system.

[DamageTests.cpp](Scaffold/DamageTests.cpp) covers all 256 selector values,
full-record death fixtures, all 65,536 damage-word patterns for subtraction/
rope/barrier arithmetic, activation gates, particle frames, damage ordering,
zero-tick contacts, lifetime-to-zero dispatch, dragon half damage/roar RNG,
knockback quirks/faults, geometry planes and masked writes, mixed activation
groups, audio cancellation, secondary factory counts/RNG, and fallback/self-alias
allocation. Existing particle/skeleton/runtime suites exercise the reused paths.
These are static-instruction-derived host tests, not DOS-runtime comparisons.

### Player melee attack sequence

The dagger/sword sequence is recovered from
[weapon input and gameplay order](weapon-trigger-evidence.txt),
[animation and contact initialization](weapon-sequence-evidence.txt),
[selection and inventory registration](weapon-selection-evidence.txt),
[selection request helper](weapon-selection-tail-evidence.txt), and
[environment gates/defaults](weapon-environment-evidence.txt).
There are no unresolved globals blocking these two attacks.

| Native location | Recovered responsibility |
| --- | --- |
| `0000:A8F7` | Attack-button action |
| `0000:B5E6` | Weapon update/draw dispatcher; gameplay state is ported, DOS drawing is not |
| `0000:B57A` | Commit requested weapon/hand, start drawing |
| `0000:C91E` | Weapon-number selection, inventory swap into hand 2 |
| `0000:793E` | Refresh weapon inventory references; last eligible duplicate wins |
| `0000:CD57` | Prefer eligible hand 2, otherwise hand 1; request type change |
| `1E48:CDD2` | Set requested type and holster command if the requested type changed |
| `10AC:0384` | Initialize a supplied temporary weapon-damage collider |

#### Recovered shared state

| Original DS address | Context member | Meaning |
| --- | --- | --- |
| `DDB6` / `DDB7` | `selectedWeapon` / `requestedWeapon` | Current/requested weapon type |
| `DDB8` | `weaponSpriteFrame` | Weapon image selection; melee uses 1, selection commit clears to 0 |
| `DDB9` / `DDBA` | `weaponHand` / `requestedWeaponHand` | Original one-based inventory slot |
| `DDBB` | `weaponAnimationSpeed` | Historical API name: actually a signed phase/countdown, **not** a constant speed |
| `DDBD` | `weaponAnimationPosition` | Weapon pose/animation position, also read by skeleton combat |
| `DDBF` | `weaponAnimationRecoil` | Existing digging recoil; neither melee branch needs it |
| `DDC5` | `weaponReady` | Resource/strike latch; not sufficient alone to permit attack |
| `DD36` / `DD76` | `weaponAmounts` / `weaponInventorySlots` | Existing 32-entry resource and one-based inventory-reference tables |
| `D4A4` | `playerLookHeight` | Screen horizon, initially 110; **not** a degree-valued pitch |
| `D4FC` | `playerUnderwater` | Blocks sword/other types above dagger at attack input |
| `D500` | `field_D500` | Existing native body-mode gate; controls the unarmed updater |
| `D502` | `debugWeaponColliders` | Optional native contact visualization bytes; initially false |
| `D4FA` | Existing `positionalSoundsSuppressed` | Also gates attack input and automatic hand selection |
| `8294` | `worldObjects[1].itemValue` | Positive values skip the entire weapon-animation update |

Negative phase commands are -1 holster, -2 draw, and -3 attack.
The sword also accepts -4 as a reset reaction. The native new-game initializer
clears selected/requested type, hand, pose and phase, sets ready true, and
clears the sprite selector. Ordinary level transitions retain these globals
and inventory; the host does not reset them on `StartLevel`.

#### Attack input and contacts

`AttackWithSelectedWeapon()` ports the supported branches of `A8F7`.
It requires phase exactly zero, the **old** ready latch true, and DS:D4FA false.
It refreshes readiness from the resource word; underwater sword input returns
before spending or allocation. Otherwise it decrements the word with wrapping,
refreshes the latch, invokes the **reclaiming** allocator even for a sword,
and writes the attack sentinel -3. The dagger immediately initializes that
allocation; the sword leaves it cleared for its later strike. This seemingly
redundant allocation is retained because a full pool can reclaim a live record.
The final inventory-registry refresh also preserves native hand preference.
The native stale-latch case can spend resource zero into -1; no extra gate is invented.

`InitializeWeaponCollider` does not clear or allocate a record. Its callers do.
It installs mask 254, behavior 2, the given lifetime/radii/damage byte, and:

```text
X = playerEyeX + signedByteDivide(low16(playerDirectionX * reach), 100)
Z = playerEyeZ + signedByteDivide(low16(playerDirectionZ * reach), 100)
Y = playerEyeY + yOffset + (low16(playerLookHeight - 90) SAR 2)
```

Coordinates wrap as words. The native signed-byte division can fault even
when a full-width host multiplication would look reasonable; host code
throws on that overflow. Player direction is the existing 100-scale vector,
not the heading angle. Native debug mode additionally assigns model/animation/
display 1 and placement 64. Normal contacts are invisible until converted into
an impact particle.

| Attack | Contact moment | Reach | Y offset | Damage | Radius XZ/Y | Lifetime |
| --- | --- | --- | --- | --- | --- | --- |
| Dagger | Immediately at input | 40 | -12 | 8 | 18/8 | 10 |
| Sword | Pose reaches exactly 32 | 45 | -10 | 20 | 22/12 | 10 |
| Unarmed transition branch | Countdown crosses 10 | 30 | -10 | 4 | 10/10 | 3 |

These are native constants, not weapon-record damage parameters.
No new line-of-sight/wall test is inserted; the existing behavior-2 collision
query performs target selection and forwards to `DamageObject`.

#### Dagger sequence

The dagger updater runs **once per call**, even for zero ticks. It restores
amount 100 and readiness true, selects sprite 1, then maps the old phase:

| Old phase | Before the final countdown decay |
| --- | --- |
| -3 | Phase 40, preserve pose |
| -1 | Phase 20, preserve pose |
| -2 | Pose 1, phase 6 |
| 0 | Pose 10 |
| 1..5 | Pose `10 - phase*5` |
| 15..20 | Pose `(phase-15)*5` |
| 21..30 | Phase 0 |
| 31..40 | Pose `(phase-31)*6 + 10` |
| Other | Pose 1; commit a changed requested type immediately |

Finally a positive phase approaches zero by the entire supplied tick count.
The common tail commits selection at phase zero if hand or type differs.
With one tick per call, drawing completes in six calls, holstering changes
type on the seventh, and an attack becomes ready again after eleven calls.
Batched dagger frames deliberately do **not** behave like repeated one-tick
animation calls; the native branch mapping only happens once.

#### Sword sequence

The sword runs exactly `ticks` substeps; zero ticks skips branch-local updates.
Each substep restores amount 100/readiness true and sprite 1, then handles:

- Phase 0: pose 20.
- -3: pose 21, phase 20.
- -2: pose 10, phase 10.
- -1: pose 0, phase 1010.
- -4: pose 0, phase 0.
- Phase 900..1000: commit a different requested type and return immediately.

Then decrement a positive phase and increment pose unless it is exactly 20.
At pose exactly 32 with readiness true, play sound effect 19 at full volume,
source -3, limit 2, allocate a **first-free** contact and clear readiness.
The next substep restores that latch, as in the EXE. Pose >=40 enters phase -2
and pose 10. All increments and comparisons preserve signed-word behavior.

With one-tick calls, a fresh attack strikes on call 11, enters recovery on
call 19, and returns to idle on call 29. Drawing takes ten substeps;
holstering commits the changed type on substep 11. A large batch cannot
skip the exact strike pose or repeat the hit during recovery. Existing
skeleton reactions continue to read/write the same shared phase and pose.

#### Runtime and demo integration

`Tick` retains existing pickup interaction, then updates hand selection,
world objects, weapon animation, weapon-number input, and finally held attack
input. Thus a newly created contact first runs in the **next** world pass;
the sword cannot damage a target in the object pass that precedes its strike.
The late animation/input order follows `E463`, `E4A4`, `E4A7`.
Existing host pickup interaction remains before the object pass, rather than
being moved with the still-incomplete native input dispatcher.
Held attack repeats when phase zero is reached; no release-edge requirement
is added. Pause/loading/menu frames neither sample weapon input nor animate.

`SelectWeapon` preserves native busy/current/missing/depleted gates, swaps the
registered inventory slot into hand 2, refreshes the registry, and requests
holstering. Automatic selection requires health **exactly 1000** and prefers
hand 2 over hand 1. Numeric selectors 1/2 correspond to dagger/sword; the DX11
sample maps held Ctrl or left mouse to attack. Those physical attack bindings
are sample choices, not a claim about the original configurable keyboard default
(native attack binding byte is DS:323D).

`GameTickResult::weapon` exposes animation and attack statuses.
Other weapon families report `UnimplementedWeapon` without executing a
success-shaped partial trigger. Their amount/allocation prefix is not applied
until those complete attack branches are recovered. The renderer reports
an attempted unsupported attack in its window title.

**Presentation boundary:** DOS held-weapon bitmap loading/drawing and the
weapon HUD are not reproduced here. The timing/hand/pose fields are available
to a future presenter; DX11 currently shows world hits/particles, not a held
dagger/sword overlay. Swimming/look control also remains partial; the recovered
environment fields are explicit rather than silently inferred from incomplete
movement code.

[WeaponTests.cpp](Scaffold/WeaponTests.cpp) tests full contact records,
all 65,536 signed phase words for each weapon, dagger batching, sword strike/
recovery thresholds and maximum batches, direction/pitch arithmetic, sound
and RNG, native allocation side effects/fallbacks, inventory/hand priority,
hotkey swaps, blocked input, unsupported weapon status, and pickup-to-enemy-hit
sequences through `Tick`. These are static-instruction-derived host tests,
not DOS-runtime comparisons.

### Drifting particles and shared smoke creation

The complete wrappers and supplied-object initializers are preserved in
[particle-spawn-evidence.txt](particle-spawn-evidence.txt). This batch adds
behavior 18 and `SpawnInCrater` (139), plus `GameContext` member initializers
in [GameContext.cpp](Scaffold/GameContext.cpp). Other discovered behaviors,
other weapon families remain outside this implementation. Shared damage,
contact behavior and dagger/sword controls have subsequently been implemented above.

#### DriftingParticle (18): whole-unit drifting and fading

This replaces the tentative name `ExplosionSmokeCloud`. The same particle
serves weapon impacts and non-weapon emitters, so the name describes its
verified motion rather than assigning a smoke-only visual role.
Entry `0000:16E5` executes a loop
of exactly `tickCount` iterations; zero ticks make no object-local changes.
Each iteration:

1. Adds each velocity to its corresponding position, with signed-word wrap.
   Unlike `FireParticle` and `SmokeFading`, there is no fixed-point accumulator.
2. Decrements vertical velocity if bit zero of `itemValue` was set.
3. Subtracts five from the blend/fade word, wrapping to a signed word.
   Clears health only when that result is **negative**, not at exactly zero.
4. Toggles only bit zero of `itemValue`; the other bits are preserved.
5. Sets display mode to `clamp(37 - (fade SAR 3), 30, 36)`.

It does not stop the loop after clearing health. Its fade-based death works
even when the separate shared health countdown is disabled. It consumes no
random numbers and does not query collision, height, or audio.

`InitializeDriftingParticle(object, frame, x, z)` ports `10AC:07F2`
(`RETF 8`, native slot/frame/X/Z word arguments). It does **not** allocate or
clear the supplied record. It sets health 50, model 39, animation 3,
frame=`frame`, second frame 1, fade 100, display 30, behavior 18, collision
type zero, and vertical velocity 1.

Before replacing X/Z, it sets horizontal velocities to the sign of
`low16(oldX - x)` and `low16(oldZ - z)`. It then assigns X/Z and draws
`Random(512)` for each of the X and Y angles, in that order.
Y, the phase word, placement mode, scale, collision radii/mask, and all other
unwritten fields survive. In particular, callers can place this initializer
over an existing object, and can subsequently override behavior 18.

#### Supplied-object smoke initializer

`InitializeSmokeParticle(object, x, y, z, radiusXZ, riseFactor, placement)`
ports `10AC:044E` (`RETF 14`, native slot/X/Y/Z/radius/factor/placement words).
The renderer placement byte is configuration, not free particle scratch.
This initializer also neither allocates nor clears.

| Field | Assignment |
| --- | --- |
| Model / animation / frame | 1 / 2 / 3 |
| Behavior / display | `SmokeFading` (24) / `SpriteTransparent` (1) |
| Blend/opacity | 10 |
| Y angle | `Random(2)` |
| XZ / Y radii | supplied radius / 5 |
| Z velocity | 2, used by behavior 24 as scale growth rather than Z movement |
| Health/lifetime | signed low word of `riseFactor * 50 + Random(50)` |
| `spriteScale` byte | low byte of `Random(unsigned radiusXZ) + 10` |
| Y velocity | signed low word of `riseFactor * 200 + Random(130)` |
| Position / placement | supplied values |

The `behaviorParameter` byte and other unwritten fields, including the fractional
accumulator, second frame, X velocity and collision fields, survive.
The four draws are always ordered **2, 50, radius, 130**.
Finally it queries the existing floor-height helper and raises Y to that
signed floor value when Y is lower. This also preserves the floor helper's
surface-metadata side effects.

The radius is a word-sized unsigned RNG bound even though the object field
is signed; negative radius words are not silently clamped. Radius zero
advances the RNG seed and returns zero at that draw, matching the explicit
zero-bound branch in native `2CE4:170A`. Missing geometry, missing
required surface metadata and invalid floor coordinates also remain explicit
errors, not successful-looking fallback particles.

#### Complete smoke emitter: SpawnInCrater

Entry `0000:3F8A` draws `Random(1000)` and emits one particle if the signed
word at offset 46 is **strictly greater** than that result. There is no tick
multiplier or per-tick loop: zero ticks can still emit, and at most one
particle is emitted per update.

It calls the first-zero-health allocator, then reads the source's position
and Y radius, and initializes smoke at
`(sourceX, low16(sourceY + radiusY), sourceZ)` with XZ radius 5, factor 1,
and placement 1. The allocator clears the chosen slot, including fallback
165. Source reads occur **after allocation**; an expiring source can therefore
be cleared by its own allocation. In the scaffold, its resulting zero X/Z
are outside the supported heightfield and fail explicitly. Do not move the
coordinate reads before allocation to hide this edge case.

New particles in later ordinary slots update in the same world pass; earlier
slots wait, and fallback 165 is initialized but not normally updated.
The emitter itself does not otherwise change its source record.

All **28** initial `SpawnInCrater` records occur in `OBJPOS25.DAT`.
They have Y radius 17 and intensities 16, 36 or 46. They corroborate the
emission fields; tests use their original records with explicit dummy floor
geometry, not a claim to have loaded level 25's actual heightfield.

[ParticleTests.cpp](Scaffold/ParticleTests.cpp) checks complete 54-byte records,
all signed-word fade/phase patterns, maximum ticks and continued death-loop
processing, initializer retention, wrapped direction signs, RNG ordering,
scale-byte truncation, floor correction/surface metadata, explicit errors,
emission thresholds, zero ticks, allocation alias/fallback, same-pass
updates, and all 28 original emitter records.

Console examples:

```powershell
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior DriftingParticle --ticks 3
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior SpawnInCrater --ticks 3
```

The synthetic crater demo uses intensity 1000 to guarantee one particle;
object files keep their actual values. Both commands retain the intentional
partial-recovery exit code 2. These are static-instruction-derived host tests,
not DOS-runtime comparisons.

### SmokeFading: object-local rise, opacity, growth, and deceleration

Behavior 24 enters at module-relative `0000:1A9D`, calls two pure math
helpers, then jumps to common postprocessing at `0000:5110`. It has no direct
global accesses, and its helpers have no global accesses either.
[Function_SmokeFading](Scaffold/BehaviorHandlers.cpp) is implemented.
The exact case and all three math routines below are preserved in
[smoke-fading-evidence.txt](smoke-fading-evidence.txt); range headings provide
the `InspectCode.java` reproduction arguments.

The operation order is:

1. Multiply the **old** `velocityY` by ticks, keeping the low word.
   Add this to `itemValue`, which this behavior uses as an 8-bit fractional
   position accumulator. Add the signed high byte of the resulting word to
   `posY`, then keep only the accumulator's low byte.
2. Set `blendWeightOrSpriteOpacity` to `min(health SAR 2, 5)`.
   This is an arithmetic right shift, not truncating division for negative
   health. In normal dispatch health is positive.
3. If `spriteScale` is below 200, add the low byte of
   `velocityZ * ticks` to it. Preserve `behaviorParameter`.
   The test occurs **before** adding; the result is neither clamped to 200
   nor protected from byte wrap.
4. Add `GetStepToward(velocityY, 0, low16(2 * ticks))` to `velocityY`,
   keeping the low word. For ordinary values this decelerates toward zero.

Only Y, the fractional accumulator, opacity, the `spriteScale` byte, and vertical
velocity can change. Health itself, X/Z, horizontal velocity fields, animation
bytes, and all shared state remain untouched by this case.
The health-based opacity agrees with
`PropertyAccessorBase::GetSpriteTransparency` in
[GamePlay.cpp](../NewRenderer/SharedSource/base/GamePlay/GamePlay.cpp).
This does **not** identify where lifetime/health is decremented or expired;
that is outside this case, as are the unrecovered common pre/postprocessing.

Example with two ticks:

```text
posY 100, fraction 250, velocityY 300, health 40,
spriteScale C7h, behaviorParameter 34h, velocityZ 3
    -> posY 103, fraction 82, velocityY 296, opacity 5,
       spriteScale CDh, behaviorParameter 34h
```

#### Shared math helper names

[GameMath.h](Scaffold/GameMath.h) and [GameMath.cpp](Scaffold/GameMath.cpp)
provide reusable translations with the original explicit argument order:

| Module-relative entry | Name and parameters | Original ABI |
| --- | --- | --- |
| `10AC:384E` | `AddFixedPoint8(whole, fraction, increment)` | First two arguments by far reference, then one word; `RETF 10` |
| `1E48:0A6C` | `GetStepToward(current, target, step)` | Three words; returns a delta in AX; `RETF 6` |
| `1E48:0A25` | `SignWord(value)` | One signed word; returns -1/0/1 in AX; `RETF 2` |
| `1E48:0051` | `Sine(angle)` | One angle word; returns signed dword in DX:AX; `RETF 2` |
| `1E48:0078` | `Cosine(angle)` | One angle word; returns signed dword in DX:AX; `RETF 2` |

References become C++ `int16_t&`; word arguments remain 16-bit. The fixed-point
helper uses eight fraction bits and signed high-byte carry. The step helper
first wraps `target - current` and its absolute value to signed 16-bit words,
then compares that signed magnitude to the signed step. Preserve the original
edge behavior for `-32768` and wrapped negative steps instead of substituting
a modern clamp. Its sign helper is pure and shared. `WrapSignedWord` is a
host arithmetic utility, not a newly identified original function.

Tests verify the example, field-level isolation, health/dispatch gates,
fraction and coordinate wrapping, scale threshold/byte wrapping, signed
right shift, and deceleration extremes. They cover every vertical-velocity
word at representative tick counts with varied fraction, health, scale, and
growth values against a separate word/byte oracle. Shared-helper tests cover
every signed word, several target/step boundaries, and aliased fixed-point
arguments. These are host tests, not DOS runtime validation.

#### Rotation tables

The same math files expose two immutable `std::array<int32_t,513>` values:

- `SineTable`: original DS:`1B00`, amplitude 65536.
- `ClampedSineTable`: original DS:`2304`, amplitude 67000, clamped to
  -65536..65536 after integer rounding.

Each includes the original extra endpoint at index 512. Rotation is
512 units per turn. The original shared helpers `Sine(angle)` and
`Cosine(angle)` use **ClampedSineTable**, with indices `angle&511` and
`(angle+128)&511`. Use `SineTable` directly when the unboosted version is
needed; do not assume the two sets of coefficients are interchangeable.

```cpp
const auto ordinary = SineTable[static_cast<std::uint16_t>(angle) & 511u];
const auto gameplay = Sine(angle);
const auto perpendicular = Cosine(angle);
```

The original generates both tables at startup using its stored angular
constant and x87 round-to-nearest conversion. The port stores verified
integer quarter-turn values and expands them at compile time, avoiding
host rounding-mode or math-library dependencies. A native verification
program ported the original finite-input x87 sine path and compared all
513 entries of both tables before freezing them. Fingerprint regressions,
an independent formula check, cardinal directions, plateau boundaries,
and all word-angle lookups are covered by the portable test executable.
No extra game context or dynamic table allocation is needed.

See [rotation findings](Findings.md#rotation-lookup-tables-and-fixed-point-arithmetic),
[initialization/lookup evidence](rotation-evidence.txt), and
[runtime sine/rounding evidence](rotation-runtime-evidence.txt) for exact
constants, validation boundaries, and addresses. The clock-pendulum
behavior itself remains an unimplemented candidate.

### Level heights and WaterDrop

[GameContext.cpp](Scaffold/GameContext.cpp) implements the shared floor and
relative-room-height queries, using the native arithmetic rather than the
renderer's floating-point interpolation. Evidence is preserved in
[heightfield-evidence.txt](heightfield-evidence.txt).

`Level::LoadGeometry(data, size)` in [Heightfield.cpp](Scaffold/Heightfield.cpp)
reads an unpacked geometry stream prefix,
matching the renderer's `ReadLevelData(..., true)`: two little-endian words
containing width-minus-one and height-minus-one, then seven row-major byte
planes. Dimensions must fit **50 columns by 40 rows**; null pointers and
incomplete streams are errors. Each row is copied into the top-left of the
native stride-50 arrays, as at `1E48:1E12`. Outside-file defaults are floor
height 255, room height 50, and zero for the other retained planes.
The required prefix is `4 + 7*width*height` bytes; trailing data is not consumed. Headerless savegame blocks,
archive extraction, and other level records are not loaded by this method.

| Plane | Contents | Original first byte in DS | Retained here |
| --- | --- | --- | --- |
| 0 | Wall texture / exact player start marker | `32D0` | `wallTextures` |
| 1 | Floor texture/object | `3AA0` | `floorObjects` |
| 2 | Ceiling texture/object | `4270` | `ceilingObjects` |
| 3 | Floor height | `4A40` | `floorHeights` |
| 4 | Room height above floor | `5210` | `roomHeights` |
| 5 | Wall/surface flags | `59E0` | `surfaceFlags` |
| 6 | Material | `61B0` | `collisionMaterials` |

The two additional retained planes are needed for correct height queries:
zero ceiling-object bytes mean no ceiling, and the low five surface bits
select flat surfaces or stacked/repeated floors. Stream loading does not
modify the separate collision map or surface metadata.

### Static wall collision raster

`GameContext::LoadLevelGeometry` calls `Level::BuildCollisionMap` in
[CollisionMap.cpp](Scaffold/CollisionMap.cpp). Evidence:
[wall-collision-evidence.txt](wall-collision-evidence.txt), native builder
`1E48:594E`, icon loader `1E48:36DA`, transparent blitter `1E48:2877`,
and capture call `0000:07EB`. The builder:

- Fills inclusive 5x5 rectangles from plane 6, with a four-pixel tile stride,
  row first then column; later rectangles overwrite shared edges.
- Uses **plane 5 & 31**, not plane 0's visible texture/shape index.
  Selectors 9..27 choose AN2 frames `selector-8`.
- Draws in native priority order `16,17,18,19,27,9,10,11,12,13,14,15,20,21,22,23,24,25,26`.
  This is the table at DS:`0352` (EXE file offset `31A72`, main DS module `2E9E`).
  Zero pixels are transparent; nonzero pixels overwrite, not OR.
- Discards AN2 relative offsets, just like the native loader, and crops the
  screen rectangle `(4,3)..(4*(width-1)+5,4*(height-1)+5)`.
  Final dimensions are `(4*width-2)` by `(4*height-1)`, e.g. level 11 is 98x111.
  Zero-based tile `(column,row)` starts at map pixel `(4*column-1,4*row)`.

The immutable original wall AN2 frames 1..19 are embedded, including placeholder
patterns. Asset SHA-256:
`4186bf3a6bc137a4c003d82734e19bc3ec46ff1ddeab8ca91832e1b00d26290e`.
No runtime AN2 file service or Unicode filename is needed. Runtime tests independently
decode the original asset, verify every embedded icon through isolated tiles,
compare all complete original FELD maps against a screen-raster/crop oracle, exercise
priority/overlap/materials/borders/high bits, and walk the controller into a real
level-11 wall. The static map is not rebuilt during ticks, preserving door/pillar edits.
Original `FELD9.256` and `FELD29.256` are only 703 bytes, versus 732 required by
their 13x8 headers. They remain explicit loading errors rather than silently
defaulting the missing 29 material bytes.

The Pascal arrays are indexed `[1..40, 1..50]`. Instructions use bases biased
backwards by 51 bytes: for example `4A0D`, not the actual first floor byte
`4A40`. The one-based loops at `1E48:1CC6` and the seven 2,000-byte
initializers at `1E48:1D5B` establish that distinction.

Host integration:

```cpp
auto level = CreateDummyLevel();
level.LoadGeometry(bytes.data(), bytes.size());
level.BuildCollisionMap();
GameContext context;
context.level = &level;
context.collisionMap = level.collisionMap.get();
auto floorY = context.GetFloorHeight(object.posX, object.posY, object.posZ);
auto ceilingY = WrapSignedWord(floorY + context.GetRoomHeight(object.posX, object.posZ));
```

`WrapSignedWord` is declared in [GameMath.h](Scaffold/GameMath.h).
The context borrows the level; its owner must keep it alive.

**Native lookup contracts:**

- `GetFloorHeight(x,y,z)`, `1E48:1B45`, first reads the surface at native
  column `x/100`, row `41-z/100`, with signed truncating division. The DOS
  division has a signed-byte quotient; overflow is an explicit host error.
- Slots 1..4 use separately supplied `level.heightSurfaces[slot-1]`, each
  containing a signed `depth` word and a byte `value`. The negated depth is
  the flat floor unless it is below -900, which falls back to terrain.
  `context.floorSurfaceValue` models DS:`D4CC`: the configured byte, or -1
  for other flags. Missing required metadata throws; it is not silently
  treated as ordinary terrain.
- Those surface depths and bytes are **not in the seven geometry streams**.
  The renderer describes three water slots in `LevelInfo`, with depth
  `height*10`. The executable accepts four slots; the fourth slot and the
  full role of DS:`D4CC` still need confirmation. No level-info loader or
  speculative fourth water slot has been added.
- Terrain helper `1E48:18CF` reads four floor samples. In host coordinates,
  the upper-left sample is `[40-z/100][x/100-1]`; its neighbors are column+1
  and row+1. X/Z fractions are `(coordinate%100)*661`; inverse fractions
  are `65535-fraction`. Each interpolation extracts the high word of an
  unsigned weighted sum. The Z blend runs from the lower to upper row.
  Result Y is the wrapped negative double of that interpolated height.
  Even a constant raw height 100 produces -196, not -200.
- Surface flag 28 subtracts room heights from the four floor corners.
  Depending on `-SAR(y,1)` versus the maximum adjusted corner minus five,
  it chooses that upper floor or restores the lower one. The unsigned
  multiplication of negative adjusted corner words is preserved.
- A positive `context.floorRepeatHeight` models DS:`D4CE`. For nonzero
  surface flags it applies helper `1E48:188B`: signed remainder, repeated
  addition of that spacing relative to the query height, then signed
  clamping to 0..255. Its level-loading source and intended level-wide
  meaning are not established; default zero disables the branch.
- `GetRoomHeight(x,z)`, `1E48:1BA6`, returns **relative** height. No ceiling
  returns 20000 immediately; otherwise the same interpolation of room
  bytes is doubled. Absolute ceiling Y is effective floor plus room height.
- Queries outside the arrays or needing out-of-array neighboring samples
  throw, rather than reproducing DOS reads into adjacent arrays. A complete
  four-sample cell requires X=100..4999 and Z=200..4099. Flat-surface and
  no-ceiling branches do not unnecessarily require those neighbors.

`CreateDummyLevel()` now also creates flat raw floor 0, room height 50,
ceiling-object 1, and surface flags 0. Native interpolation therefore gives
floor Y=0 and room height=96. This is a usable **dummy room**, not the
original array initializer (floor 255, room 50, ceiling 0).

**WaterDrop (`BehaviorType::WaterDrop`, case `0000:2EBD`)** calls
`10AC:9A2C`. Its gameplay state changes are implemented:

1. Query effective floor using the **old** position.
2. Add `velocityY*ticks` to Y with word wrapping. Set transparent-sprite
   display and opacity 4.
3. If signed word `field_24` is positive, subtract ticks from that wait
   timer. Otherwise grow `spriteScale` until it exceeds signed word
   `doorLockOrGroupBitmask`; then subtract ticks from vertical velocity.
   Clamp velocity to -16 only in this non-waiting branch. `behaviorParameter`
   is preserved; growth and subtraction wrap exactly as in the executable.
4. Only `posY < floor` is impact. Clear `winX`, decrement `field_26` once
   **per update**, place Y at floor-1, stop velocity, and select frame 2.
5. Once signed `field_26 < 1`, select frame 1, restore that counter to 4,
   move to effective floor plus relative room height, set the wait timer
   from `itemValue`, clear `spriteScale`, and set `winX=1`.

Thus the splash lasts four impact updates after the counter is initialized
to four. An initial zero counter respawns immediately on first impact.
The native helper does not force an arbitrary freshly created object to
the ceiling: initial position/counters come from the object record, and
the ceiling placement occurs at reset. It never deletes the object or
changes its health.

The `winX==1` impact branch calls positional sound helpers `10AC:202A`,
`1E48:D001`, and `1E48:D15C`. It now requests `SoundEffect::Drop` through
the recovered sound API below; hardware playback requires an `AudioHandler`.
Shared object pre/postprocessing is still outside this recovered case.

Tests cover stream offsets and all retained cells, rejected input without
partial writes, all 10,000 fractional X/Z pairs, every constant byte
height, special-surface thresholds and metadata, stacked/repeated floors,
lookup bounds, no-ceiling behavior, byte/word wrapping, old-position floor
selection, zero-tick splash updates, and a complete 41-update drop cycle
with wait 20 and target scale 2. These remain host tests, not DOS-runtime
validation.

### Sound effects, attenuation, and four-channel admission

The author confirms **effect 5 is Drop**, and remembers random sample
variants, attenuation, and four playback channels. The corresponding
instructions are now preserved in [sound-evidence.txt](sound-evidence.txt).
[Sound.h](Scaffold/Sound.h) defines the manually maintained `SoundEffect`
enum, initially just `Drop=5`, and the virtual `AudioHandler` interface.
[GameContext.cpp](Scaffold/GameContext.cpp) implements table loading, sample selection,
attenuation, and channel admission.

**Correction to the earlier call interpretation:** `10AC:202A` takes
**two** words, X/Z, and returns with `RETF 4`. It does not consume the
earlier `PUSH 5`. That word remains on the stack as the first argument of
`1E48:D15C`, which returns with `RETF 8` and takes **four words**:

| Pascal push order | Meaning | WaterDrop argument |
| --- | --- | --- |
| 1 | Sound-effect ID | `Drop` (5) |
| 2 | Volume | Attenuation of wrapped distance times 3 |
| 3 | Source ID | Original object slot |
| 4 | Signed simultaneous-play limit per source | 1 |

This is **not a priority parameter**. At this entry point there is no
preemption: occupied channels are never selected or stopped. Other audio
entry points are not thereby ruled out.

`PlaySoundEffect(effect, volume, sourceId, instanceLimit)` implements that
contract, returning `SoundPlaybackStatus` for started, disabled/unavailable,
inaudible, missing sample, or channel/limit rejection. An overload takes a
world-object reference and derives its source ID from `worldObjects`.
With an attached backend, an object outside that array is an explicit error;
do not invent a shared source ID for unrelated temporary objects.

**The variant table is data, not embedded sample IDs in the executable.**
The initialization prefix at `1E48:D297` opens the Pascal string
`samp\s_tab.prb`, reads four header bytes, then reads 645 bytes into
DS:`ECEA`. The existing
[S_TAB.PRB](../../Data/SourceAssets/Original/SAMP/S_TAB.PRB) is exactly 649
bytes, with little-endian header words **129, 5**:

- 129 effect records, one-based effect IDs.
- Five bytes per record: count, then **four possible sample references**.
- The shipped table uses at most **three** variants. Drop has two:
  sample IDs **5 and 38**.
- References remain one-based, matching the original samples. Zero or a
  reference above the backend's available sample count produces `NoSample`.
- Zero-count effects are legitimate empty entries. Invalid table dimensions,
  incomplete/extra bytes, counts above four, and effect IDs outside 1..129
  are explicit errors. Loading validates the whole input before mutation.
- The loader loads all records; only the enum's semantic names await further
  identification. The enum is separate from generated renderer enums.

Selector `1E48:D086` calls Turbo Pascal `Random(count)` at `2CE4:170A`.
The seed-update helper `2CE4:1759` computes:

```text
RandSeed = (RandSeed * 08088405h + 1) modulo 2^32
variant = high16(RandSeed) modulo count
```

The low multiplier word at `CS:178F` is `8405h`; the helper's shifts/additions
provide the other multiplier terms. This implementation uses
`GameContext::randomSeed` (DS:`04C2`), not a separate sound-only generator.
Default seed is zero; timer-based `Randomize`/game startup seeding is not
ported. Nonempty effects consume a random draw before volume, mute, or
channel rejection. Without a backend and without a loaded catalog, requests
report `AudioUnavailable` without attempting selection.

**Four-channel policy (`1E48:D0EE`, `1E48:D15C`):**

1. Scan the four channels in order. Count free channels and active channels
   whose source word matches this request. This count includes **other
   effects and samples from the same source**, not just duplicates.
2. Remember the **last** free channel: native slot 4 is preferred when all
   are free. Host interface indices are 0..3.
3. A **negative** limit requires at least two free channels before starting,
   so this request leaves one free. Positive limits may use the last one.
4. Require an available channel and
   `nativeAbs(instanceLimit) > activeCountForSource`. A zero limit starts
   nothing; native `ABS(-32768)` remains negative and also starts nothing.
5. Ask the backend to start the selected sample/volume on that free channel
   and record the source ID. Backend exceptions propagate; source tracking
   is only published after successful startup.

The original ten-byte channel records begin at DS:`F37C`: sample at +0,
volume at +2, active word at +4, a reset-to-zero playback field at +6, and
source at +8. The host backend owns playback/completion state, exposed by
`IsChannelPlaying`; the gameplay context retains just the source IDs needed
for admission. It must own these four channels exclusively, report completion
accurately, and throw on playback failures rather than silently dropping them.

**Distance is separate from playback:**

- `10AC:202A(x,z)` returns wrapped Manhattan distance from DS:`D4D2/D4D6`,
  modeled as `soundListenerX/Z`. These globals' wider camera/player role
  still needs tracing; callers supply the listener explicitly.
- DS:`D4FA`, modeled as `positionalSoundsSuppressed`, forces distance
  10000. Its wider mode meaning is not assigned.
- `1E48:D001(distance)` returns
  `clamp(355 - (unsigned(nativeAbs(distance)) >> 1), 0, 255)`.
- `GetSoundVolumeAt(x,z,distanceScale)` combines these operations, preserving
  word wrapping and `ABS(-32768)`. The scale defaults to 1; WaterDrop uses 3.
  This is X/Z-only attenuation, not Euclidean distance or stereo panning.
- To bypass falloff, pass a fixed volume (typically 255) directly to
  `PlaySoundEffect`. There is no attenuation boolean in the four-word native
  playback call.

Library integration:

```cpp
context.LoadSoundEffects(tableBytes.data(), tableBytes.size());
context.audioHandler = &audio;
context.soundListenerX = listenerX;
context.soundListenerZ = listenerZ;
auto& object = context.worldObjects[2];
context.PlaySoundEffect(SoundEffect::Drop,
    context.GetSoundVolumeAt(object.posX, object.posZ, 3), object, 1);
```

`AudioHandler` supplies sample count, channel-playing queries, and
`PlaySample(channel, sampleReference, volume)`. It is borrowed, like level
state; the host loads/decodes the samples and must keep the handler alive.
No DOS DMA/mixer or renderer audio-device implementation has been invented.
`soundEnabled` models an explicit playback-disable gate. No backend normally
means audio is unavailable, not that gameplay must fail.

The console's optional `--sound-table` installs a **logging-only** backend:

```powershell
.\Source\Decompile\build\Debug\desperabis-recovery.exe --object drop54.bin --sound-table .\Data\SourceAssets\Original\SAMP\S_TAB.PRB
```

This reports channel/sample/volume requests, not audible output. For this
isolated console test its listener is at the object's X/Z, or at the supplied
player when using `--player`. Omitting the option leaves audio unavailable.

Tests use the real table, all four variant counts, all low seed words and
high-seed boundary cases, all 16 channel occupancy masks, signed instance
limits, cross-sample source counting, completion/reuse, muted/empty/unavailable
cases, backend failures, exhaustive X/Z attenuation arithmetic, and WaterDrop's
one-sound-per-impact latch. CTest supplies both original executable and table
paths. These are host tests, not recorded DOS playback comparisons.

## Build and run

From the repository root, using the installed Visual Studio 2026 toolchain:

```powershell
.\Source\Decompile\GenerateSolution.bat
cmake --build .\Source\Decompile\build --config Debug
ctest --test-dir .\Source\Decompile\build -C Debug --output-on-failure

.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior BatIndoor --ticks 3
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior IdleAnimation --ticks 4
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior SmokeFading --ticks 2
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior WaterDrop --ticks 2
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior 300
.\Source\Decompile\build\Debug\desperabis-recovery.exe --list
```

The generation script also works from other working directories. It creates
`build\DesperabisRecovery.slnx`; generated build files remain Git-ignored.

The console can also read exactly one 54-byte record:

```powershell
.\Source\Decompile\build\Debug\desperabis-recovery.exe --object object54.bin --ticks 3
```

The console creates the dummy level by default. To execute Pillar and save the
result without supplying a map:

```powershell
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior Pillar --collision-map-output dummy-map.bin
```

To replace it with a map-buffer file (four-byte header followed by pixel
bytes, already normalized to header offset zero), use:

```powershell
.\Source\Decompile\build\Debug\desperabis-recovery.exe --behavior Pillar --collision-map map.bin --collision-map-output updated-map.bin
```

For explicit coordinates, use `--object object54.bin` instead of `--behavior`.
The WaterDrop-only console demo starts at X=150/Z=4050 at the computed ceiling;
binary object input always retains its own position. Load actual geometry with:

```powershell
.\Source\Decompile\build\Debug\desperabis-recovery.exe --object drop54.bin --geometry FELD.256 --ticks 3
```

This loads the unpacked stream prefix described above, not a compressed
`GAME.DAT` archive entry. Special surface slots need separate level-info
metadata through the library API; the console reports an error if required
metadata is absent. Geometry loading does not generate collision-map pixels.

Collision-map input must contain 4..40004 bytes and include the entire raster described by
the header; declared dimensions must fit the dummy allocation. The input is
loaded into the existing 40004-byte dummy map, with any remaining bytes zeroed;
this optional buffer import is not a variable-sized level allocator.
Output saves the complete 40004-byte allocation, including any wrapped writes.
This is a scaffold memory dump, not a new on-disk level format.

Execution/selection commands return **exit code 2** to indicate that full
gameplay remains incomplete, even when an implemented case ran.
Invalid input/file/memory errors return 1. Help/list return 0.
Raw record input is not a savegame loader.

The tests check all catalog entries, signed health gating, disabled dispatch,
little-endian words, bounds, sentinel/high-byte handling, and unchanged object state.
They also independently interpret the original selector instructions for
**all 65,536 possible behavior words** and compare the C++ destination.
Routing tests alone do not establish behavior equivalence. Additional tests
verify Pillar's single-byte overwrite, value truncation, health/dispatch gates,
tick/Y independence, missing-state errors, and dummy-level dimensions/zeroing.
An independent modular
arithmetic oracle checks every signed X and every signed Z at four strides,
including coordinate subtraction, stride, and final address wrapping with
a nonzero original pointer offset normalized to header-relative storage.
These are host tests, not DOS runtime validation.

## Reproduction and safe refinement

1. Export the original object routine with `InspectCode.java`, read-only and
   without auto-analysis, using range `0000:1094:448B`. See the headless
   invocation pattern in [HighLevelStructure.md](HighLevelStructure.md).
2. Generate to a **new** directory:

```powershell
.\Source\Decompile\GenerateBehaviorScaffold.ps1 `
    -DisassemblyPath .\Source\Decompile\captures\object-behavior-full.txt `
    -OutputDirectory .\Source\Decompile\captures\fresh-scaffold
```

The generator checks the original hash and every exported instruction byte
against the relocated executable. It walks the selector's comparison spine
instead of guessing boundaries from adjacent text. Its JSON records the
renderer header hash too. It also emits `GameEnums.h`, copying the four
game-relevant enums and adding the unknown/sentinel behavior selectors found
in the executable. Numeric behavior assignments live there; catalog entries
and generated handler results refer to enum members.

It refuses to overwrite any generated output. **Edit recovered handler bodies
in place; do not regenerate over them.** Generate elsewhere and reconcile
catalog/evidence changes deliberately when the source enums or analysis change.
When refreshing enum definitions, reconcile `GameEnums.h` as well. Do not copy
fresh placeholder handlers over already implemented functions.
The original case bodies share a Pascal stack frame and common code; these
C++ functions are recovery boundaries, not claims of original function calls.
