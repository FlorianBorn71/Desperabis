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
| `34` | unsigned word | Scale, accessor divides by `64 * 256` |

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

Pillar, LightSource, Door, Flies, IdleAnimation, SmokeFading, Switch1,
SwitchGrouped, WaterDrop, and DisabledSentinel return `ExecutedCase`. The other
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
are included. Other light-emitting behavior cases remain unrecovered.

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
in `GameContext`. Activation is separate from per-frame behavior and remains
unimplemented, as do the other switch variants below.

**Ordinary switch (`Switch1`, 50, `0000:1FDD`):** its per-frame case is
object-local. Byte offset 42 is an animation phase, offset 38 its signed rate,
and offset 36 the return speed (initialized to 10 if zero). It adds
`low16(rate * ticks)` to the phase, preserving word wrapping and signed tests:

- Below zero: stop the rate and clamp phase to zero.
- Above 100: clamp phase to 100 and set rate to the negation of offset 36.
- At phase 100, a nonzero byte at offset 52 stops the rate, latching the switch.

Thus ordinary auto-return is in this case, but there is no separate countdown
here. Offset 52 is the low byte of `spriteScale` in the immutable layout;
for this behavior it is a latch flag, not a change to the record layout.

**Index-based target activation:** the renderer's switch index at offset 34
is confirmed by the interaction paths `10AC:194C` and `10AC:2094`, which
call `10AC:2060`. That helper takes one word index (`RETF 2`), adds 1000
with word wrapping, scans internal object slots **2..140 inclusive**, and
calls `10AC:10F5(slot, 0)` for each object whose signed health word equals
that encoded value. It does not operate on a global switch-bit mask.
This includes doors but is not limited to them. The 2..140 bound is separate
from the main update's previously observed 2..164 bound.

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
and shared masks. The future index-based activation helper will need the
shared object collection to scan slots 2..140. Its target selection is known,
but its callee `10AC:10F5` handles many collision/interaction types and effects;
only selected paths have been traced. No object collection or success-shaped
partial activation helper has been added solely to accommodate that future
code. Ordinary doors are now implemented below; game-window input polling and
general target interaction remain unimplemented. Explicit grouped-switch interaction and
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
`--interact`, and reports ordinary switch target dispatch as unimplemented.

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
level exits, teleporters, sarcophagi, ordinary switches, picture maps,
and lorries explicitly return `UnimplementedHandler` without mutating the
object or inventory. Other collision values follow the original generic
pickup tail. Door prompts and sound requests are implemented; message rendering,
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
| `spriteScale` low byte | Previous contact/overlap slot; high byte is preserved |
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
original German wrong-key text, encoded as UTF-8. A selected key ID of zero
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

`GameContext::PromptUserMessage(text, duration)` is the gameplay counterpart of
`1E48:2459`: it owns the text and replaces the current prompt without drawing.
`userMessage` holds at most 150 UTF-8 bytes, shortened at a character boundary;
the original stored at most 150 DOS-encoded bytes. Empty input clears the text
and lifetime. `userMessageTicks` receives signed-word absolute duration,
including the original -32768 overflow edge case.

`UpdateUserMessage(ticks)` follows the countdown part of `1E48:3F2A`: only a
positive lifetime decrements, without clamping or clearing expired text.
The host treats positive ticks as active. `UpdateObjects` calls it once after
all object updates using `frameTickCount`; standalone object dispatch does
not advance it. Hosts using their own scheduling should call it once per
frame instead, not once per object or additionally to `UpdateObjects`.
Negative update ticks are invalid.

Negative prompt durations originally selected alternate screen placement.
Their magnitude remains supported, but the screen-layout globals, calculated
pixel position, and text drawing are deliberately left to the host.
All duration words, overwrite/expiry rules, independent contexts, UTF-8
truncation, and once-per-frame timing are tested.

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
3. If the high byte of `spriteScale` is below 200, add the low byte of
   `velocityZ * ticks` to that high byte. Preserve the low scale byte.
   The test occurs **before** adding; the result is neither clamped to 200
   nor protected from byte wrap.
4. Add `GetStepToward(velocityY, 0, low16(2 * ticks))` to `velocityY`,
   keeping the low word. For ordinary values this decelerates toward zero.

Only Y, the fractional accumulator, opacity, the high scale byte, and vertical
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
scale C734h, velocityZ 3
    -> posY 103, fraction 82, velocityY 296, opacity 5, scale CD34h
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
planes. Native gameplay requires **50 columns by 40 rows**; other dimensions,
null pointers, and incomplete streams are errors. The required prefix is
14,004 bytes; trailing data is not consumed. Headerless savegame blocks,
archive extraction, and other level records are not loaded by this method.

| Plane | Contents | Original first byte in DS | Retained here |
| --- | --- | --- | --- |
| 0 | Wall texture | `32D0` | No |
| 1 | Floor texture/object | `3AA0` | No |
| 2 | Ceiling texture/object | `4270` | `ceilingObjects` |
| 3 | Floor height | `4A40` | `floorHeights` |
| 4 | Room height above floor | `5210` | `roomHeights` |
| 5 | Wall/surface flags | `59E0` | `surfaceFlags` |
| 6 | Material | `61B0` | No |

The two additional retained planes are needed for correct height queries:
zero ceiling-object bytes mean no ceiling, and the low five surface bits
select flat surfaces or stacked/repeated floors. Stream loading does not
modify the separate collision map or surface metadata.

The Pascal arrays are indexed `[1..40, 1..50]`. Instructions use bases biased
backwards by 51 bytes: for example `4A0D`, not the actual first floor byte
`4A40`. The one-based loops at `1E48:1CC6` and the seven 2,000-byte
initializers at `1E48:1D5B` establish that distinction.

Host integration:

```cpp
auto level = CreateDummyLevel();
level.LoadGeometry(bytes.data(), bytes.size());
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
   timer. Otherwise grow the high scale byte until it exceeds signed word
   `doorLockOrGroupBitmask`; then subtract ticks from vertical velocity.
   Clamp velocity to -16 only in this non-waiting branch. Low scale byte
   is preserved; growth and subtraction wrap exactly as in the executable.
4. Only `posY < floor` is impact. Clear `winX`, decrement `field_26` once
   **per update**, place Y at floor-1, stop velocity, and select frame 2.
5. Once signed `field_26 < 1`, select frame 1, restore that counter to 4,
   move to effective floor plus relative room height, set the wait timer
   from `itemValue`, clear the high scale byte, and set `winX=1`.

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
