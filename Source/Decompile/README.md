# Desperabis gameplay recovery

Recover the behavior of the original Turbo Pascal 7 DOS game and make it
available to the modern renderer. This directory holds reproducible analysis,
not original source code or an assertion that decompiled C is correct.

For the current executable observations, hypotheses, and open questions, see
[Executable findings](Findings.md).
For the first control-flow map and the connection to the existing DOSBox
bridge, see [High-level program structure](HighLevelStructure.md).
For object layout, the compilable named-handler scaffold, build commands,
and its explicit limitations, see [Object behavior recovery](BehaviorRecovery.md).
For the first explained case and shared collision-map pointer, see
[Pillar](Pillar.md) and its [instruction evidence](pillar-evidence.txt).

## Directory conventions

- `InspectExecutable.ps1`: read-only DOS MZ inspection and baseline generation.
- `InstallTools.ps1`: pinned official downloads, SHA-256 verification, and
  extraction. Resumes partial downloads; refuses existing installation
  directories rather than overwriting them. Do not run concurrent installers.
- `StartGhidra.ps1`: GUI/headless launcher using only the portable JDK.
- `ImportOriginal.ps1`: first import, with automatic analysis disabled.
- `ghidra_scripts/VerifyDosImport.java`: verifies the original binary,
  entry-point mapping, relocation, and initial instruction decoding.
- `ghidra_scripts/InspectCode.java`: bounded read-only decoding with raw bytes,
  encoded far-call targets, and logical relative-branch targets.
- `ghidra_scripts/DiscoverObjectSpawns.java`: read-only allocator-call discovery,
  bounded object provenance, and one-level initializer tracing.
- `SpawnDiscovery.md`, `spawn-discovery.txt`, `spawn-initializer-evidence.txt`:
  discovery report, 25-site index, and selected initializer instruction evidence.
  Includes 14 linked behavior IDs, 12 absent from the initial object tables;
  this is not a claim to cover every spawn mechanism.
- `TestSpawnDiscovery.ps1`: independent relocation enumeration, regression
  controls, and original-object-table cross-checks for the discovery report.
- `original-executable.json`: generated identity and header baseline for the
  original executable. No timestamps or machine-specific paths.
- `crab-oil-dispatch-evidence.txt`, `crab-oil-helper-evidence.txt`,
  `crab-oil-support-evidence.txt`: native push-player radial separation,
  crab wandering/material checks and burnable-oil aging, light and spreading.
  `Scaffold/ThirdPartBehaviorTests.cpp` covers these mechanics and their
  original third-part level objects. Destructible walls are extracted into
  independently hideable ranges sharing the static island GPU buffers; the host
  synchronizes their visibility from GameCore's authoritative wall tiles.
  See [destructible wall presentation](BehaviorRecovery.md#destructible-wall-presentation).
- `map-presentation-evidence.txt`: native map ownership gate, corner-map
  heading, PictureMap's page-11 transition and modal map dispatch.
  The corner map uses a lazily uploaded, palette-converted collision texture;
  see [map presentation](BehaviorRecovery.md#owned-and-wall-map-presentation).
- `map-modal-geometry-evidence.txt`: complete native modal-map routine and its
  coordinate, parchment-remap and line-drawing helpers. Confirms separate
  geometry-based tile shading and wall outlines, now generated into one separate
  level texture shared by the modal and the wall-map model's preview material.
- `water-surface-generation-evidence.txt`: four native surface definitions,
  five-bit selectors and intentional suppression of zero-depth/mode-255 flat
  quads. Level 17 uses modelled shoreline water in those suppressed tiles;
  see [native special-surface geometry](BehaviorRecovery.md#native-special-surface-geometry).
- `level-fifteen-traps-evidence.txt`, `level-fifteen-trap-spawn-evidence.txt`,
  `level-fifteen-trap-helpers-evidence.txt`: arrow emitters, floor spears and
  their shared projectile/contact helpers; implemented with original-fixture
  regression tests in `Scaffold/LevelFifteenTrapTests.cpp`.
- `level-sixteen-seventeen-evidence.txt`,
  `level-sixteen-seventeen-support-evidence.txt`,
  `level-sixteen-seventeen-lifecycle-evidence.txt`: recovered frog, outdoor
  bat, lava smoke, sloped rolling-rock and shark paths.
- `forest-flooding-evidence.txt`: native `ExtinguishLava` morph/stepping,
  floor-frame replacement and conditional collision-map rectangle. Implemented
  with switchable renderer floor variants and original/saved-state regressions;
  see [forest flooding](BehaviorRecovery.md#forest-river-flooding-and-lava-extinguishing).
- `initial-import.txt`: generated memory map and initial instruction report.
- `high-level-evidence.txt`: focused instruction evidence for the control-flow map.
- `ui-evidence.txt`: UI page dispatch, F2 inventory navigation, shared input/text
  helpers, overlay/meter registration, and per-buffer element redraw lists.
  See [UI architecture](HighLevelStructure.md#ui-pages-shared-helpers-and-element-lists).
- `ui-chapter-evidence.txt`: chapter-selection cancellation and acceptance,
  including the original new-game initialization call.
- `new-game-initialization-evidence.txt`: recovered fresh-game resets and
  their relationship to the functional part selector and normal level loading.
- `Scaffold/GameContext.h/.cpp`: context class with all member definitions in
  one implementation file: application flow/page hooks, object allocation,
  sound/random numbers, collision/height queries, and prompts.
- `Scaffold/ApplicationFlow.h`: native-valued menu enums; outer page dispatch
  is implemented in `GameContext.cpp`. Page loops and loading remain
  explicit throwing hooks, not rendering or a playable application.
- `Scaffold/ApplicationFlowTests.cpp`: menu gates, full scripted page flow,
  startup failure handling, and routing coverage for every state word.
- `Scaffold/Cutscene.cpp`, `Scaffold/CutsceneTests.cpp`: native timed-cutscene
  playback, stationary windmill camera with meteor-driven rotation, and the
  post-swamp 24 -> 97 -> 25 transition after 700 ticks, including authored
  impact debris and the thirty radial fire-wave triggers. Moving fire changes
  surface selectors to 29; the renderer updates static-model fire presentation
  and grid brightness without replacing meshes.
  See [meteor cutscene recovery](BehaviorRecovery.md#timed-meteor-cutscene-after-the-swamp).
- `player-movement-evidence.txt`: keyboard/mouse controls, movement/turning,
  terrain support, collision-map correction, and player-object synchronization.
  The controller is identified but not yet implemented.
- `object-update-evidence.txt`: shared lifetime/burning prefix, scene-list
  suffix, iteration bounds, and both clearing/free-slot allocators.
- `lighting-evidence.txt`: packed static brightness, dynamic 21x21
  tile-corner cache, LightSource flicker/writes, frame clearing, and readers.
- `pulsating-light-evidence.txt`: complete behavior-23 pulse update, randomized
  target speed, clamped-sine outputs, and one-corner light contribution.
- `fireplace-evidence.txt`: fireplace sound, weighted flickering light,
  emission, fire-particle initialization/update, and the signed random helper.
  See [fireplace recovery](BehaviorRecovery.md#burningfireplace-and-fireparticle).
- `Scaffold/FireplaceTests.cpp`: exact particle records, arithmetic/RNG/audio/
  lighting checks, allocation/lifetime edges, and six original asset fixtures.
- `particle-spawn-evidence.txt`: behavior 18, its supplied-object initializer,
  the shared smoke initializer, and the complete `SpawnInCrater` emitter.
- `Scaffold/ParticleTests.cpp`: drifting/fading arithmetic, initializer field
  retention/RNG, floor correction, crater emission, lifecycle/allocation
  edges, and 28 original level-25 emitter records. See
  [particle creation](BehaviorRecovery.md#drifting-particles-and-shared-smoke-creation).
- `floating-debris-movement-evidence.txt`: complete floating-debris update and
  its player-overlap, direction, map-predicate, and fixed-point movement helpers.
- `Scaffold/FloatingDebrisTests.cpp`: full-record/RNG oracle, signed state words,
  axis reflections, vertical bounds, and all 24 original debris records.
  See [floating debris](BehaviorRecovery.md#floatingdebris-floating-underwater-parts).
- `rat-snake-evidence.txt`, `rat-snake-support-evidence.txt`: complete rat/snake
  movement, shared math/collision helpers, and separate player-contact damage.
- `Scaffold/RatSnakeTests.cpp`: movement/RNG, health gates, steering, neighborhood
  retries, sound and all 40 native rat/snake records. See
  [rat/snake recovery](BehaviorRecovery.md#ratandsnake-movement-and-separate-contact-damage).
- `skeleton-*-evidence.txt`: skeleton movement, attacks/parrying, fixed-point
  player life, death fragmentation and fragment/water lifecycles.
- `Scaffold/SkeletonTests.cpp`: native arithmetic/RNG oracle, wall navigation,
  strike/parry gates, life/death state, eight-part creation/settling, and all
  15 original skeleton records. See
  [skeleton recovery](BehaviorRecovery.md#skeleton-movement-combat-and-death)
  for player-weapon, remaining activation-driver and death-presentation boundaries.
- `cockroach-dispatch-evidence.txt`, `cockroach-movement-evidence.txt`,
  `cockroach-hit-evidence.txt`: shared cockroach/brown-spider phases, movement,
  obstacle avoidance, death and speed-dependent player contact.
- `Scaffold/CockroachTests.cpp`: seeded phase/RNG oracles, health gates,
  terrain/wake-up/death/contact checks and all 70 original records. See
  [cockroach recovery](BehaviorRecovery.md#cockroach-shared-cockroachbrown-spider-behavior).
- `damage-activation-evidence.txt`, `damage-support-evidence.txt`,
  `damage-projectile-evidence.txt`, `damage-projectile-tail-evidence.txt`:
  complete shared damage/activation cascade and its direct dependencies.
- `Scaffold/DamageTests.cpp`: weapon contacts, all collision selectors, full-record
  death transitions, signed-word arithmetic, activation groups, geometry writes,
  secondary spawning, fallback/alias allocation, and audio cancellation.
  See [weapon damage and shared activation](BehaviorRecovery.md#weapon-damage-and-shared-activation).
- `weapon-trigger-evidence.txt`, `weapon-sequence-evidence.txt`,
  `weapon-selection-evidence.txt`, `weapon-selection-tail-evidence.txt`,
  `weapon-environment-evidence.txt`: dagger/sword attack input, shared state,
  equip/holster selection, exact contacts and gameplay update order.
- `Scaffold/WeaponPresentation.cpp`: read-only `GetWeaponView()` recipes for all
  named weapons, native walking bob, mirrored bitmap offsets, interpolated model
  poses, casting/reload frames and draw/holster motion. DX11 executes these after
  the world pass using cached art and private model depth, independently of the
  pickup preview. See [first-person weapon presentation](BehaviorRecovery.md#first-person-weapon-presentation).
- `Scaffold/WeaponTests.cpp`: exhaustive phase-word tests, swing/recovery
  timelines, contact records, hand/registry selection, audio and live `Tick`
  pickup-to-hit integration. See [melee attack sequence](BehaviorRecovery.md#player-melee-attack-sequence).
- `ranged-weapon-physics-evidence.txt`, `ranged-weapon-support-evidence.txt`,
  `ranged-weapon-support-tail-evidence.txt`, `weapon-projectile-routes-evidence.txt`,
  `weapon-resting-evidence.txt`, `weapon-effect-initialization-evidence.txt`,
  `weapon-effect-loader-evidence.txt`, `weapon-square-root-evidence.txt`:
  crossbow/fireball physics, stone contact, placement and effect-resource contracts.
- `fireball-breakup-evidence.txt`: `BouncingObject` (1) flame submode, shared
  per-substep dispatch and complete `FadingParticle` (5) opacity update.
- `Scaffold/RangedWeaponTests.cpp`: crossbow, fireball, stone, electricity,
  shovel/scythe, Lentils bridge reveal, hedge destruction, bear-trap/metal-pole
  placement, fireball breakup movement/flicker/light/fading, exhaustive flame
  arithmetic/RNG, and original OBJPOS/AN2 regressions.
  See [ranged weapons and placement](BehaviorRecovery.md#ranged-weapons-shovel-and-placement)
  for unimplemented extra tools and explicit unsupported placement boundaries.
- `scythe-lentils-physics-evidence.txt`, `scythe-hedge-map-evidence.txt`,
  `scythe-lentils-support-evidence.txt`, `scythe-resource-evidence.txt`,
  `scythe-resource-init-evidence.txt`, `lentils-hit-clamp-evidence.txt`:
  hedge animation/map removal, native tool counters, Lentils overlap masks,
  bridge frame progression and remaining platform-support boundaries.
- `item-label-loader-evidence.txt`, `item-label-selection-evidence.txt`,
  `item-label-consumers-evidence.txt`, `item-price-message-tail-evidence.txt`:
  indexed `BEZEICH.TXT` loading, item-name subtype selection, the leading
  parenthesis display flag and shop-price substitution.
  See [item-name findings](Findings.md#indexed-item-names-and-shop-text-bezeichtxt).
- `lighting-shade-evidence.txt`: timer-driven shade noise, its quantization
  consumer, a separate global shading term, and per-frame noise-range resets.
- `object-render-list-evidence.txt`: render-sort-key assignment and signed
  sorted insertion into tile lists, with external per-object next links.
- `door-unlock-evidence.txt`: inventory key matching/consumption, lock-state
  handling, feedback branches, and activation after successful unlocking.
- `door-message-evidence.txt`: prompt storage/countdown, consumed-hand icon
  redraw scheduling, activation preconditions, and ordinary-door entry/interaction.
- `Scaffold/Lighting.cpp`: gameplay-only dynamic lighting cache, explicit
  per-frame reset, native world-coordinate accumulation, and window culling.
  LightSource objects now add flicker and request their configured sounds;
  the unidentified DS:DC34 accumulator is preserved. No rendering is included.
- `Scaffold/`: compilable selector, implemented Pillar/LightSource/PulsatingLight/Door/Flies/RatAndSnake/Skeleton/FloatingDebris/WeaponDamageCollider/DriftingParticle/WaterImpact/SinkingObject/FireParticle/BurningFireplace/IdleAnimation/SmokeFading/SpawnInCrater/switch/WaterDrop/LevelStartOverride cases,
  placeholder handlers, and tests.
- `Scaffold/CollisionMap.cpp`: explicit 200x200 dummy-level creation until proper
  level loading is recovered. Collision-map queries/writers are in `GameContext.cpp`.
- `Scaffold/GameMath.h/.cpp`: shared fixed-point, signed-step, and sign helpers,
  plus both original integer sine tables and the sine/cosine lookup helpers.
- `rotation-evidence.txt`, `rotation-runtime-evidence.txt`: table initialization,
  masked lookups, and the original x87 sine/rounding path.
- `Scaffold/Sound.h`: named sound effects and virtual audio interface.
  Table loading, attenuation, random selection, and channel admission are in
  `GameContext.cpp`.
- `sound-evidence.txt`: exact distance, volume, sample selection, channel
  policy, table-loading prefix, and random-number instruction ranges.
- `ambient-mixing-evidence.txt`: level-selected ambient recipes, base-track
  construction, random weighted blending, XMS transfers, and loop streaming.
  See [generated ambient sound](BehaviorRecovery.md#generated-ambient-sound-loop).
- `Scaffold/AmbientTests.cpp`: original-asset recipe/RNG checks and ambient
  level-load metadata, disabled-sound, failure propagation and retry tests.
- `Scaffold/Heightfield.cpp`: unpacked geometry stream loading for `Level`.
  Native floor/room queries and special surfaces are in `GameContext.cpp`.
- `heightfield-evidence.txt`: exact WaterDrop, height lookup, and Pascal
  array-layout instruction ranges.
- `renderer-grid-boundary-evidence.txt`, `renderer-grid-tiles-evidence.txt`,
  `renderer-grid-placement-evidence.txt`: fixed 50x40 loading, native render
  tile/vertex bounds, outside-grid floor 0/ceiling 80, and static-model
  corner-height selection. Rendering does not use gameplay's cross-plane
  height aliases. See [native padded rendering grid](BehaviorRecovery.md#native-padded-rendering-grid).
- `Scaffold/ObjectInteraction.cpp`: explicit-input pickup/inventory gameplay,
  grouped-switch arming, and known object-local interactions.
- `Scaffold/PlayerInteraction.cpp`: per-update collision-selection/input-interface/
  interaction path. World/player overlap queries are in `GameContext.cpp`.
- `inventory-evidence.txt`: inventory/counter initialization, cleanup,
  input-gated calls, and the original copy-helper contract.
- `Scaffold/BehaviorMap.json`: case addresses and direct global/call dependencies.
- `Scaffold/GameEnums.h`: renderer-derived behavior/collision/display/weapon
  enums with original storage widths and additional binary behavior selectors.
- `GenerateBehaviorScaffold.ps1`: reproducible extraction with overwrite guards.
- `build/` (ignored): local CMake build products.
- `tools/` (ignored): optional workspace-local tool installations.
- `projects/` (ignored): local Ghidra projects. Back these up separately;
  Git does not preserve them.
- `captures/` (ignored): runtime dumps and debugger logs. Promote small,
  relevant evidence into tracked investigation notes when needed.

Do not modify the [original executable](../../Game/Original/DESPERAB.EXE).
Do not use the localized executable interchangeably: patching can change bytes
and invalidate evidence tied to a hash.

## Continuing on another machine

Commit and push the documentation, scripts, CMake definition, and `Scaffold/`
sources before moving to another checkout. Local files, including untracked
files, are not transferred by Git until committed. Chat history is not required
to understand the recorded findings.

On the other machine:

1. Check out the branch containing that work and retrieve Git LFS content
   (`git lfs pull`), including the original executable.
2. Run `InspectExecutable.ps1` and compare its SHA-256 with
   [the baseline](original-executable.json).
3. Install CMake and Visual Studio 2026 with the C++ desktop tools, then run
   `GenerateSolution.bat`. Open `build\DesperabisRecovery.slnx`.
4. Build and test using the commands in
   [Object behavior recovery](BehaviorRecovery.md). Use `desperabis-recovery`
   as the startup project; example arguments: `--behavior BatIndoor --ticks 3`.
5. For further binary analysis, run `InstallTools.ps1` to obtain the pinned
   portable Ghidra/JDK installations, or configure equivalent tools explicitly.

The generated solution keeps the gameplay library and recovery console at the
top level. Test projects are grouped under `Tests`, and generated build helpers
(such as `ALL_BUILD`, `ZERO_CHECK`, and `RUN_TESTS`) under `CMake`.
Regenerate with `GenerateSolution.bat` to update an existing solution.
Tests remain enabled and build normally; configure with `-DBUILD_TESTING=OFF`
if you want to omit test projects entirely.

The ignored `projects/`, `tools/`, `captures/`, and `build/` directories are
not included in a checkout. At this stage the Ghidra project contains the
initial import only; subsequent inspections were read-only. No recovered
names/types or edited functions exist solely in that database.

To recreate that import without overwriting the checked-in `initial-import.txt`,
run this from the repository root on a machine without the local project:

```powershell
$root = Join-Path $PWD 'Source\Decompile'
New-Item -ItemType Directory -Path (Join-Path $root 'projects') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $root 'captures') -Force | Out-Null
.\Source\Decompile\StartGhidra.ps1 -Headless -HeadlessArguments @(
    (Join-Path $root 'projects'), 'Desperabis',
    '-import', (Join-Path $root '..\..\Game\Original\DESPERAB.EXE'),
    '-noanalysis',
    '-scriptPath', (Join-Path $root 'ghidra_scripts'),
    '-postScript', 'VerifyDosImport.java',
    (Join-Path $root 'captures\recreated-import.txt')
)
```

Check that the new report ends with `Verification: PASS`. Do not rerun an
import over an existing annotated project. `ImportOriginal.ps1` intentionally
refuses an existing report, so it cannot be used unchanged when the tracked
initial report is already present.

The external DOSBox modification in the sibling `SheetMusicOverlay` repository
is not part of this checkout. Its relevant hook, address mapping, record-copy
layout, and source hash are documented in
[High-level program structure](HighLevelStructure.md). Obtain that external
source separately if you want to rebuild the bridge; a working emulator
integration is not currently reproduced by this repository.

### Host-driven renderer integration

Cave levels 17/18 now cover all authored live behavior handlers and their
required fire-jet/death dependencies. This includes lift riding, electricity
recharge/beam feedback, anaconda chain motion/death, and the missing shark,
bat and anaconda player-contact responses, gorilla repulsion and pedestal
touch damage. See
[cave recovery](BehaviorRecovery.md#cave-levels-17-and-18),
[BehaviorHandlers.cpp](Scaffold/BehaviorHandlers.cpp) and the original-asset
`cave-levels` regression suite. Native object/save layouts are unchanged.

Level 15's dragon is implemented in
[DragonBehaviors.cpp](Scaffold/DragonBehaviors.cpp), including pursuit, animated
walking, six-direction head/mouth frames, close breath, distant three-fireball
volleys, death fragments/clouds and the dropped key. `LoadLevelGeometry` resolves
the `LEVELS.INF` model-slot mapping; `LoadLevelObjects` loads the relevant original
3DA anchor geometry. GameCore positions the existing linked `DragonHead` world
record before contact processing and after object updates. The host just
synchronizes that sprite normally: no dragon-specific renderable fields or
host-owned attachment callbacks are required. Invisible point-color-254 markers
must not be drawn, and bank-5 fireballs resolve ordinary `ANIM5` frames even in
torchlight display mode; only sprite bank 6 uses reserved `FACKEL` texture slots.
See [dragon recovery](BehaviorRecovery.md#level-15-dragon-boss) and the
`level-fifteen-dragon` core/WARP regressions.

The renderer solution now contains the independent **Desperabis-GameCore** static
library project at
[Desperabis-GameCore.vcxproj](../Desperabis-GameCore/Desperabis-GameCore.vcxproj).
It builds the same sources as the headless CMake library; it has no renderer,
DirectX, OpenXR, or window-system dependency. Add a project reference and the
`Decompile/Scaffold` include directory to a host project. With CMake, add this
directory as a subdirectory and link `Desperabis::GameCore`. The existing
`behavior-recovery` target remains available; the output library is named
`Desperabis-GameCore`.

Subclass `GameContext` and override only `LoadLevel(const LevelLoadRequest&)`
to start loading host assets. Call `StartLevel(number)` to enter loading,
then `CompleteLevelLoad(request.id, std::move(data))` with owned
`LevelLoadData`, or `FailLevelLoad(request.id, message)` on failure.
GameCore is authoritative for subsequent level selection: automatic
`CollisionType::LevelEnd` contact calls `EndLevel()`, which requests the next
level or unloads the finished game part and opens the main menu. The host must
build its renderer scene **inside the requested load**, not select/load a scene
first and then tell the core which level to install. Effect channels and the
core-owned ambient loop stop when loading begins, including deferred loads.
`GameTickStatus::LevelEnded` has no object/weapon update; application world delta
and tick timing are zero for that transition frame. Synchronize new-level
presentation after completion, and do not carry the old frame's simulation
into the new scene. See [progression and exit recovery](BehaviorRecovery.md#core-owned-level-progression-and-levelend).
Interactive hosts call `UpdateApplication(elapsedSeconds, input)` every frame,
including paused, loading and zero-simulation-tick frames. GameCore checks
F1/F2 input, dispatches gameplay/main-menu/part-selection/inventory/map updates, and owns the
33 Hz accumulator. Inspect the optional `result.tick`, synchronize render
instances from `GetWorldObjects()`, and use `result.worldDeltaSeconds` for
world presentation clocks. Draw the core-authored `GetApplicationPresentation()`
recipe on its original 360x240 logical canvas; the host only resolves assets and scales
that canvas. No modal UI override is required.
F1 exposes Resume, New Game, Load Game, Save Game and Quit. New Game reads
part names and complete level sequences from `GRAFIK/HERZ.256`, built from
the localization tool's `MissionsMenu.txt`; it does not display level indices.
With the original data, playable starts are 50, 30 and 10. Up/Down
selects, Enter starts and Escape returns without altering the current game.
All three parts are available for reconstruction/testing; native unlock state
and the animated DOS selector artwork are not yet modeled. Acceptance clears
carried inventory/resources and restores the native vital/weapon defaults
before requesting the existing host level loader. Host cheats/preferences
remain intact. Part 2 followed by six Tab advances reaches forest level 16.
Startup remains the training level. See the
[new-game contract](BehaviorRecovery.md#game-context-and-application-flow).
Unported introductions are skipped using `LEVELS.INF` presentation metadata,
but remain in the native saved sequence, including their ordinal positions.
Load/Save offers ten slots, `STAND1.SPL` through `STAND10.SPL`, in the shared
repository `Bin/SaveGames` directory for both Debug and Release. Copy original
saves there to import them; files written there can also be copied back to DOS.
Enter selects a save name, typing/backspace edits its original 20-byte DOS
string, and Enter again saves/overwrites. Escape cancels editing before leaving
the slot screen. Unused rows are blank; editing replaces the selected row and
shows a blinking inline caret, without a separate overwrite prompt. All
main-menu-family pages use the original `BACKGR2` image/palette and rotating
`CURSOR.AN2` sword in the font palette. Their 360x480 art/text aspect is mapped
to the common 360x240 logical canvas. Empty/corrupt slots and file errors are
explicitly reported.
The host supplies CP437 text through `QueueTextInput`, implements
`ReadSaveFile`/`WriteSaveFile`, and installs `GetPendingSaveGame()` into an owned
renderer `Level`/`LevelScene` before completing the core level load. Loaded
snapshots retain their objects, seven geometry planes, collision pixels and
embedded asset metadata; they must not be replaced by cached original levels
or a cheat loadout. See the [native SPL contract](BehaviorRecovery.md#native-savegames-and-herz-part-data).
Inventory recipes reference the original frame/action/resource art and palette
shade states rather than replacement widgets. See the
[native layout recovery](HighLevelStructure.md#original-inventory-artwork-and-placement).
Gameplay recipes may contain a clipped `UiMap` overlay while still requiring
the world to render; use the presentation's page, not an empty command list,
to distinguish gameplay from modal screens. `ApplicationUi::UploadCollisionMap`
consumes `GameContext::collisionmapUploadDirty` only after a successful upload.
Runtime collision edits must use the context's pixel/span helpers, which route
through `WriteCollisionMapByte`; do not bypass dirty tracking with raw writes.
`SyncGameObjects` also passes the full native floor plane to
`Level::SynchronizeFloorTiles` for the prebuilt `ExtinguishLava` river variants;
the core remains responsible for morph progress, collision and floor writes.
Level installation and floor-footprint changes also invalidate map presentation.
`ApplicationUi::UploadGeometryMap` independently caches the modal/wall-preview
texture by loaded level and `geometryRevision`. `ApplyMapMaterial` changes only
the map panel's per-instance material and shares that GPU texture; do not modify
the original model atlas or upload separate preview pixels. World instances
must be cleared before the UI's level texture cache.
`Tick({behaviorTicks, frameTicks}, input)` remains the low-level recovery API;
do not mix it with the application's time accumulator. See the
[runtime integration contract](BehaviorRecovery.md#host-driven-runtime-integration)
for lifetime, timing, pause, error, and object-copy details.

Tutorial prompts use `CollisionType::TriggerTextPrompt` (210) on contact and
`BehaviorType::TutorialTrigger` (210) after other overlapping objects disappear.
`LoadLevelGeometry` resolves level texture slots from `LEVELS.INF`;
`LoadLevelObjects` decodes referenced `TUR<n>.256` message tables. Each texture
row contains a length-prefixed DOS string, not rendered text pixels. Object
`generalPurpose3` selects the texture slot, `itemValue` the zero-based row, and
`generalPurpose1` the signed prompt duration. Tutorial level 50 uses slot 10,
`TUR39.256`, with fourteen rows. Negative durations request bottom placement;
backslashes separate lines. `HandleTextPromptContact` sends the original bytes
through `PromptUserMessage` and uses `SetObjectHealth` for one-shot removal when
`behaviorParameter` and the native DS:04CD gate are nonzero.
See `tutorial-*-evidence.txt` for native dispatch, contact and string-transfer code.

Player contact also handles `DestroyOnCollide` (10), including the tutorial's
spider web: native `DamageObject(object, 0)` disables collision and advances to
the destroyed model frame rather than deleting the record. `DamageObj` (49) and
burning oil puddles apply `-29491 * behaviorParameter` to signed 16.16
`playerLife` on each selected-contact update, through `ApplyPlayerLifeDelta`.
They need no interaction key and preserve the native suppression/depletion rules.
See `player-collision-evidence.txt` at `0000:DB42` and `0000:DBF1`.

`OnObjectCreated(int slot)` and `OnObjectDestroyed(int slot)` are default-empty
virtual host hooks, delivered immediately on the game thread, including factories
and interactions called outside `Tick`. Active means `health > 0`; slots 1..165
include the player and allocator fallback, but not reserved slot 0, inventory or
scratch records. Destruction observes the old record before deactivation or
allocator clearing; creation observes the initialized record. Reusing an active
slot emits destruction followed by creation, even within one update. Level
completion destroys the old active slots before replacing geometry, then announces
new active slots after installing the complete ready world. Failed/rejected loads
do not replace the world. Countdown expiry still dispatches the native handler at
zero health; a subsequent revival is a new creation.

For external changes, use `ReplaceWorldObject(slot, initializedRecord)` (an empty
record removes it) or `SetObjectHealth(record, health)` for active/inactive health
transitions; when activating through the latter, initialize other fields first.
Direct writes to public `worldObjects` cannot be intercepted and emit no hooks.
Native allocators announce destruction before clearing but do not reserve or
announce an empty slot: finish creation with `ReplaceWorldObject` or a supplied
particle initializer. Reinitializing an already active particle preserves its
lifetime unless its health becomes nonpositive. Hooks must not throw, mutate the
world or reenter gameplay. Factory errors propagate and can retain native partial
writes without announcing successful creation; repair or replace that world
before resuming a failed update. Construction, destruction and moving a `GameContext`
do not emit hooks; hosts must clean up their own resources when retiring a context.
There is no end-of-tick lifecycle scan. Continue using `GetWorldObjects()` for
ordinary per-frame changes such as position and animation.

The DX11 host wires these hooks to renderer-only extras owned by `LevelScene`,
not by generic `Scene` or GameCore. Its 166-entry shared-pointer array uses
native slots 1..165 (slot 0 remains reserved). The polymorphic
`ObjectPresentationContext` factory currently owns torch, pulsating-item,
fireplace, fireball-projectile/impact, dragon-fireball and fire-emitter lights,
plus breakup-spark coronas. Creation attaches
the light; destruction, live-slot replacement, scene clearing and scene
destruction detach it. Presentation synchronization follows current resolved
object positions, updates light bounds, and replaces contexts on in-place
behavior changes without allocating a new light on every movement update.
The normal rendered-object range remains native slots 2..164.

Fireball projectile/impact contexts also apply additive material overrides.
Their presentation update reapplies the override after sprite/frame material
refreshes and recomputes material hashes and transparency classification,
without changing native display flags, textures or opacity. The nine breakup
sparks use additive materials and small corona-only sources (no dynamic lights), identified by their
native flame sprite (bank 1, animation 5, frames 1..2), selector word 36 = 1
and BouncingObject/FadingParticle behavior. Additive blending persists through
their bounce-to-fade transition; unrelated debris and smoke retain their
existing blending. Fireplace fire particles, pipe flames and dragon breath
also use additive overrides, without allocating per-particle dynamic lights.

`GameContext::FlashLight(position, rgb, fadeInSeconds, fadeOutSeconds, radius)`
forwards a renderer-independent `LightFlash` request to the default-empty
`OnLightFlash` hook. Position/radius use native world units; RGB uses byte
channels and durations use seconds. Fade times must be finite and nonnegative
with a positive total duration; radius must be finite and positive (default
200). Invalid requests throw. The DX11 host converts units and lets `LevelScene`
own a linearly fading light, automatically removing it after fade-out.
Weapon-fireball breakup and the dragon's final explosion trigger warm flashes
without changing native damage, fragment counts or RNG.

`OnFireEmitted(slot, position)` reports actual pipe/dragon-breath emission at
the source position or resolved muzzle. Detached recovery records report slot
-1; the DX11 host ignores those. Each emitter owns one retriggerable orange
light fading over 0.2 seconds after its last emission, rather than a light
per flame. The dragon's three volley fireballs each retain a moving light.
Spark coronas use eight times their initial radius for better visibility,
follow native size/opacity through bouncing and fading, and
their `FillLightParams` never contributes to the dynamic-light budget.

Dormant ghosts (health other than 1000) and active ghosts at native opacity
zero are completely invisible, rather than inheriting the generic sprite
mapping's one-sixth alpha. Other sprites retain that mapping. Each ghost owns
one dark-blue light, enabled only while active and within 220 native X/Z units
of the camera, fading in over the outer 120 units. The existing death phases
brighten this light toward vivid blue, then fade it during the final dissolve.
No dormant ghost contributes a light, and removal clears its context.
The live light uses RGB (15,30,220), intensity 2.25 and radius 180 native units
so its blue tint remains visible against the normal white player light; death
increases intensity toward 6.75 with radius 195. Both intensity and radius
are 75% of the previous tuning. This illuminates nearby surfaces,
not a corona/halo around the sprite. Pixel regressions check visible blue
illumination at 100/150 native units with the normal player light enabled.

DX11 dynamic-light specularity defaults to amount 2.0 and exponent 16.
The experimental F6-F8 controls have been removed from both clients.
A zero amount disables the specular contribution.

These are per-view `RenderingSettings`, shared by all lit materials. The
pixel shader adds Blinn-Phong half-vector highlights from each dynamic
light, scaled by its color, radial attenuation and `dot(albedo, albedo)`
of the sampled texture RGB. This unnormalized squared mask ranges from
0 to 3: half-gray contributes one quarter as much as white, exaggerating
bright surfaces. Ambient/baked light, unlit materials and coronas receive no
specular term. `LightSource::SetSpecularAmount` sets a finite, nonnegative
per-light multiplier, defaulting to 1; zero disables highlights. It multiplies
the global amount, allowing fractional or boosted specularity per source.
`PlayerLight` uses 1 in blue mode and 0 in normal/off modes. This parameter
changes only the specular term, not diffuse
illumination, fading or coronas. Highlights follow the active camera (including each
stereo eye and reflection views) and are fogged with the lit surface.
BurningFireplace lights use intensity 1.5 instead of 3.0 to reduce the
overlapping illumination in the mine; their color, radius and flicker
remain unchanged.
After HLSL edits, rebuild `ShaderCompiler-DX11.vcxproj` in Release/x64 and
run `GenerateBuiltInCpps.exe` with its project directory as the working
directory before rebuilding clients: DX11 loads the embedded bytecode in
`SharedSource/DX11/BuiltInShaders_Release.cpp`, not the HLSL source at runtime.

While a charged electricity weapon is firing in gameplay, the DX11 host
maintains one cyan light and small corona at its bolt origin. Native weapon
model vertex 3 is unchanged between the two firing frames. The application
projects it using the first-person weapon viewport, then unprojects through
the world camera so both hands, animated poses and aspect ratios stay aligned.
Stopping, weapon/menu changes, free-camera mode and scene clearing remove the
electricity light. Transient/emission hooks apply to the GameCore-driven host;
the remote DOS scene shares particle materials/projectile contexts but does not
synthesize these new events from snapshots.

`Level::PopulateScene` requires a presentation callback when creating objects;
`LevelScene` supplies it and translates the loader's zero-based object ordinal
to native slot `ordinal + 2`. The remote DOS scene uses the same slot-owned
factory. Existing texture-animation markers remain scene-wide, independent
of these per-object light contexts. Regression coverage in
[ApplicationUiTests.cpp](../NewRenderer/DesperabisGame-DX11/ApplicationUiTests.cpp)
checks original level-23 lights and firing/expiry of an actual GameCore fireball,
along with slot reuse, behavior changes, reactivation and scene cleanup.
It also checks flash timing/validation, emitter reuse, dragon-volley light
counts, corona-only fading/bounds, and electricity alignment against actual GPU
weapon matrices within 0.25 pixels in both chapter banks and three aspect ratios.

`OnPlayerHealthChanged(previousLife, currentLife)` is a separate default-empty
virtual hook for the authoritative signed 16.16 `playerLife` meter, not slot 1's
object-health word. `ApplyPlayerLifeDelta` calls it immediately after suppression,
clamping and depletion processing, only if the final value changed. Fractional
damage/healing notifies too; unchanged/suppressed deltas do not. Use that helper
for player-life updates: direct field writes bypass notification. Hooks must not
mutate gameplay or reenter updates; callback exceptions propagate after life is
committed. Construction and ordinary level changes do not emit this hook; the
host should build its initial UI from current life/max and rebuild when replacing
its scene. Native initial/max health is **360**, even though the gradient has
**90** texels. The `player-health` suite covers immediate ordering, fractional
changes, clamps, suppression, depletion, gameplay damage and level transitions.

`LoadLevelGeometry` requires both the requested FELD file and `LEVEL\LEVELS.INF`.
It installs the four native surface depth/value pairs and repeated-floor height
alongside geometry and collision data, so water contact does not encounter
missing metadata partway through a tick. Malformed level information fails loading.

`Tick` now includes a **basic normal walking/posture controller**: forward/back,
strafe, turn, normal acceleration/damping, collision-map correction, gravity
and interpolated terrain support, jumping, crouching and low-room clearance. It uses
`frameTicks` for player substeps and updates the camera/listener before contacts
and object behaviors. Set `basicPlayerMovementEnabled = false` if the host owns
movement. `result.playerMovement` reports simulated and blocked substeps.
The host cheat flag `DEBUG_noPlayerCollision` defaults to false and persists
across level changes. It bypasses player wall correction and steep-slope
sliding/look adjustment, keeps `playerLife` at 100% of its current maximum,
and blocks all damage, including fire, lava, contact and drowning. Restoration
uses `ApplyPlayerLifeDelta`, including health notification, on enable, ticks
and level/save installation; it also clears death-camera and hit state.
Disabling the cheat immediately restores ordinary damage and slope behavior,
without disabling object interactions.
The normal standing eye height is 45 native units above terrain; crouched height
is 28, with the original gradual transitions and changing collision radius.
`JumpKey` raises a crouched player before jumping; releasing `CrouchKey` does not
automatically stand. Water contact forces crouching; underwater play is bottom
walking/crouching, not free swimming.
`GetPlayerCamera()` returns eye position, not object-center Y; do not add another
host eye offset. `LookUpKey`, `LookDownKey` and `CenterLookKey` control the
native horizon with terrain-ahead adjustment and per-frame smoothing.
`PlayerCamera::lookHeight` exposes the applied horizon independently of direction;
weapon launch elevation uses that same state. The `player-look` suite checks
input timing/limits, all 65,536 applied horizon words, slopes, arrow launch and
controller gates. See [native evidence](player-look-evidence.txt).
Steep slopes now apply the native 3x3 terrain correction. Collision-47 bridge
pieces supply smooth support, suppress terrain look/pushing, and choose their
own footstep effect; plank and activated-drawbridge behaviors are implemented.
Walking includes native bobbing, surface-dependent footsteps, landing sounds
and two-instance channel admission. The `player-support` suite covers these
paths, including original level 23/34 bridge records. See
[support evidence](player-support-footsteps-evidence.txt) and the
[support contract](BehaviorRecovery.md#terrain-slopes-bridge-support-and-footsteps).
Teleporters now support manual unlocking/opening, native automatic contact,
destination heading/eye-height modes and cooldown. Activated ladders provide
native vertical control; collision-48 ladders suppress terrain sliding/look.
Behavior 184 supplies the native wall-collision bypass gate. See the
[special movement contract](BehaviorRecovery.md#teleporters-ladders-and-collision-bypass-volumes)
and `player-special-*-evidence.txt`; `player-support` also tests original
level 10/23/36 teleporter records. Vehicle attachment/controllers, several
contact hazards remain deferred. Oxygen, drowning, looping underwater audio and
exhaled bubbles are implemented; see the
[underwater contract](BehaviorRecovery.md#underwater-oxygen-audio-and-bubbles).
Teleport
render-table and lighting flags are recorded, not yet rendered; behavior 178's
possible foil/viewport relationship remains unproven.
Supply a valid starting
position, a horizontal view vector, and real map data for walls. See the
[basic controller contract](BehaviorRecovery.md#basic-horizontal-player-controller).

Build just this project from the solution:

```powershell
msbuild .\Source\Desperabis.sln '/t:libraries\Desperabis-GameCore' /p:Configuration=Debug /p:Platform=x64
```

The `game-runtime` CTest suite covers loading, ownership, separate tick clocks,
input/update ordering, pause, explicit failures, raw-object round trips, and
renderer/game enum compatibility. This provides the integration API, **not an
already connected renderer scene**. Existing scene-side behaviors must be
disabled for game-controlled objects when the renderer bridge is added.

### Recovery status

The [Pillar](Pillar.md) collision-map writer and corresponding reader/player-blocking
rules are implemented. `BatIndoor`'s helper at module-relative `10AC:3391` is now
recovered too: flying, wall avoidance, ceiling roosting/waking, sound and falling
after a lethal hit. The `bat-indoor-behavior` tests include all 50 original bat
records on their real collision maps; see [behavior details](BehaviorRecovery.md#batindoor-indoor-flight-roosting-and-death).
Ordinary `Mummie` and final-level `MummieGuard` are implemented too, including
alternating-tick movement, attacks, encoded animation banks, staged death/smoke
and guard opening. Tests exercise all nine original mummies and both guards.
The four records with health/state above 1000 retain their native paused gate.
Automatic activation volumes now wake matching groups: level 12's invisible
slot-22 trigger wakes its three paused mummies without an interaction key. See
[mummy behavior](BehaviorRecovery.md#mummie-and-mummieguard) for exact semantics.
The shared renderer now darkens model display modes 30..36
(`VectorBrightness0..VectorBrightness6`); WARP verifies the real mummy's fade, smoke and
removal. RGB modulation approximates the original indexed palette maps;
mode 37 remains separate. Native selection is preserved in
[model-fade-display-evidence.txt](model-fade-display-evidence.txt).
The separately recovered [special model-colour tables](model-colour-remap-evidence.txt)
show that modes 20..25 select water, orange/fire, dark/oil, identity,
blue-white glass/fog and red-lava remaps, not an ascending brightness ramp.
The cathedral light rays and museum aquarium encode triangle selector 4,
currently called `WhiteFog`, independently of their whole-object display mode.
The export records the exact `OPGSET.256` row ordering and its seven-row
payload despite a six-row header. This recovery does not change the current
native palette shader. Modes 20..25 are named `VectorWaterTint`,
`VectorLavaTint`, `VectorOilTint`, `VectorFullbright`, `VectorBlueTint` and
`VectorRedLavaTint`; the renderer reuses the corresponding material RGB
approximations with opaque alpha. WARP verifies all six tints on the same
real model. Modes 26/27 retain their prior approximations as `Type26`/`Type27`,
and fire-overlay tint/intensity remain unchanged.
`SpiderLarge`, `Harvestman`, `Devil` and `Gorilla` are also implemented from
their native helpers, including white-spider/harvestman ceiling release,
contact damage, devil leap attacks, death poses and the spider's ballistic
slime-to-puddle animation. Enemy state remains object-local, with context-owned
RNG/player-life/collision services and no new mutable globals. All 64 original
enemy records are tested; the shared mummy regression remains intact. See
[four enemy behaviors](BehaviorRecovery.md#spiderlarge-harvestman-devil-and-gorilla).
`FlyingArrow` is currently recovered only for slime variants 3/103.
Pillar's case and point writer, plus the object-local IdleAnimation and
SmokeFading cases, Flies, two switch behaviors, WaterDrop, LightSource, PulsatingLight, ordinary
doors, RatAndSnake, FloatingDebris, LevelStartOverride, behavior 18, and the fireplace/crater particle paths are implemented
and host-tested.
Flies preserves randomized frame blending, per-update orientation changes, and
the fixed buzzing effect with doubled-distance attenuation; it does not move
the object's position. Use `--behavior Flies --ticks 3` to inspect its state.
See [Flies](BehaviorRecovery.md#flies-randomized-frame-blending-and-buzzing)
for exact thresholds, RNG order, and removal after burning.
LightSource adds flickering contributions to the gameplay lighting cache and
requests its configured sound; no rendering is involved. Reset the cache
once per frame before object updates. The console does this automatically:
`--behavior LightSource --ticks 100` runs a dummy light and reports its
contribution. See [gameplay lighting](BehaviorRecovery.md#gameplay-lighting-cache)
for frame setup and the still-unidentified DS:DC34 accumulator.
PulsatingLight varies its pulse speed, updates `generalPurpose3`, and adds a clamped-sine
contribution of 5..55 to one lighting corner. Use
`--behavior PulsatingLight --ticks 3` for its state and contribution.
See [PulsatingLight](BehaviorRecovery.md#pulsatinglight-randomized-pulse-speed-and-one-corner-lighting).
`Level::LoadGeometry` accepts the unpacked 50x40 seven-plane geometry prefix;
the context borrows the level for floor and relative-room-height queries.
Use `--behavior WaterDrop` for a dummy-room update, or
`--object drop54.bin --geometry FELD.256` to supply geometry and object state.
WaterDrop splash requests now use `SoundEffect::Drop` and a virtual
`AudioHandler`; variant selection, attenuation, and four-channel admission
are recovered. Add `--sound-table .\Data\SourceAssets\Original\SAMP\S_TAB.PRB`
to log selected samples/channels (not audible playback). A real host audio
backend and loading separate special-surface metadata remain outside this
port; missing required geometry metadata is an explicit error. See
[Level heights and WaterDrop](BehaviorRecovery.md#level-heights-and-waterdrop).
See [Sound effects](BehaviorRecovery.md#sound-effects-attenuation-and-four-channel-admission)
for the actual four-argument playback contract and integration.
`AmbientSoundPosition` (190) now implements occasional positional effects:
one enabled-update probability draw, byte-5 effect ID, X/Z attenuation and
field-46 instance limit. Tutorial slots 100/101 use IDs 87/88 and probability
9/1000. This does not alter the separate environmental ambient loop.
`UnderwaterColorAnim` (104) is recovered as moving underwater brightness patches,
but deliberately remains unimplemented until a renderer lighting path is agreed.
See [tutorial marker recovery](BehaviorRecovery.md#tutorial-positional-sound-and-underwater-lighting-markers).
Explicit pickup interactions now populate fourteen inventory objects and
update the known counters in the game context; grouped-switch interaction
arms its return timer. Doors support automatic closing, obstruction reopening,
the native 3x3 collision footprint, grouped activation, and consuming matching
inventory keys. The overridable `GameContext::PromptUserMessage` hook sends original DOS-encoded
feedback to the app, which owns its storage, lifetime and presentation. `IsUserMessageActive`
lets doors preserve native feedback gating without core-owned message state.
`IsBottomUserMessageActive` similarly gates the witch's mouth animation.
The [level-90 witch shop](BehaviorRecovery.md#witch-shop-level-90) implements
linked-stock purchases with native gold/stacking/ownership rules, localized
spoken prices, head/mouth/dog-petting animations and both proximity curtains.
`witch-level` tests exercise original assets and native save continuation;
WARP tests verify the non-additive, core-controlled mouth material.
See [doors and unlocking](BehaviorRecovery.md#ordinary-doors-and-key-unlocking)
and [gameplay messages](BehaviorRecovery.md#gameplay-user-messages).
Use `--behavior Door --ticks 3` for an opening-door fixture, or
`--object object54.bin --interact` to request one
ENTER-style action instead of a behavior update. Add `--player player54.bin`
to select that target through the original overlap query first; omit
`--interact` to check overlap without pressing the key. The library exposes
`UpdatePlayerInteraction(context, input)` and a virtual `InputHandler`
interface for game integration. Input uses abstract forward/backward, strafe,
turn, and interaction actions through `IsKeyPressed(InputKey)`.
`UpdateObjects(context)` now runs the recovered lifetime/burning prefix and
case dispatch for slots 2..164; the console uses this path. Both native
free-slot allocation methods are available on the context. There is no
universal velocity or angular-speed integration in the original shared loop.
Level-10 weather is implemented in `Scaffold/WeatherBehaviors.cpp`: context-owned
wind, airborne/ground leaves, individual raindrops and splashes, and native
lightning/thunder timing. `GetLightningFlashes()` exposes complete, unclipped
360x240 screen-space line lists with palette index 250 and render-table request
16. DX11 now renders tapered textured strips and a distant dynamic light as
an intentional non-native enhancement; the native flash palette is not applied. Snapshots
last until the next object-update batch, remain frozen while paused, and clear
on loading/unload. `Scaffold/WeatherTests.cpp` checks native RNG/geometry,
pooling, original fixtures and the existing key-99 building teleporter.
The host can override `OnLightningShown(const LightningFlash&)` and
`OnLightningHidden()` instead of polling: show fires after each complete flash,
hide once when the active batch clears. The callback reference is borrowed;
copy it to retain geometry. Flash metadata includes the generator's native
slot and optional XYZ position, not an inferred 3D strike point.
Each line also exposes full `thickness1/thickness2` in native screen pixels:
2 at the trunk, progressively thinner along the bolt and side branches, exactly
zero at terminal tips. These deterministic fields consume no additional RNG.
Rain's native display mode 13 is recognized as an unlit sprite; the DX11 WARP
tests render both original rain/splash frames, including in zero ambient light.
See [weather recovery](BehaviorRecovery.md#level-10-wind-leaves-rain-and-lightning).
Player movement and selected contact/interaction paths run through `Tick`,
not this low-level loop. Vehicle controllers, several contact hazards and
unsupported interactions remain incomplete; rendering belongs to the host. Follow
[Object behavior recovery](BehaviorRecovery.md) for scope, regeneration
precautions, and unresolved globals. Before future Ghidra-only edits become
essential, add tested annotation exports/restoration or transfer a project
archive separately; the present scripts do not back up arbitrary annotations.

## Reproduce the baseline

From the repository root in PowerShell:

```powershell
.\Source\Decompile\InspectExecutable.ps1
```

To deliberately regenerate the tracked baseline:

```powershell
.\Source\Decompile\InspectExecutable.ps1 -OutputPath .\Source\Decompile\original-executable.json
```

The script checks MZ bounds, relocation targets, and the entry point. These
checks do not establish that the executable is unpacked or overlay-free.
Likewise, an overlay number of zero does not rule out an overlay system.

## Tools and import procedure

1. Obtain an official prebuilt [Ghidra release](https://github.com/NationalSecurityAgency/ghidra/releases).
   Follow the JDK requirement shipped with that release; do not assume the
   development branch requirements match a release. Use a separate local JDK
   rather than replacing the machine-wide Java installation.
2. Create a non-shared Ghidra project under `projects/`.
3. Import the original executable using the DOS MZ loader. Verify a 16-bit
   little-endian real-mode x86 language, relocated memory layout, entry bytes,
   and the mapping between file offsets and analysis addresses.
4. Record the exact Ghidra/JDK versions, executable hash, loader options,
   language/compiler IDs, analysis options, and address mapping here before
   treating analysis addresses as durable references.
5. Initially inspect the entry point and segment setup before broad automatic
   analysis. Verify function boundaries and near/far returns. Pascal runtime
   code and inline assembly may require manual corrections.
6. Add a debugger-enabled DOSBox build for runtime experiments. Record its
   exact build and configuration. Work on a separate game copy under
   `captures/`, not the original game directory, to isolate saves and writes.
   Verify breakpoints work before using it as evidence.

### Installed and verified

- Ghidra **12.2**, official `ghidra_12.2_PUBLIC_20261016.zip`.
  Archive SHA-256:
  `2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67`.
- Eclipse Temurin **25.0.4.1+1**, Windows x64 HotSpot JDK.
  Archive SHA-256:
  `00c847d804f4a78e9f04f2683faf14fed898535b177b7fc704486cb0284e9283`.
- Existing Java 11 installation and system environment were not changed.
- DOSBox was not found on PATH; no emulator setup or runtime debugging yet.

Both archives were checksum-verified before extraction. Downloads are from
the NSA Ghidra and Adoptium Temurin GitHub releases, respectively.

From the repository root:

```powershell
# Fresh installation only; not needed on the already configured machine.
.\Source\Decompile\InstallTools.ps1

# First import only; refuses an existing project or report.
.\Source\Decompile\ImportOriginal.ps1

# Open the GUI, then select Source\Decompile\projects\Desperabis.gpr.
.\Source\Decompile\StartGhidra.ps1
```

The first import succeeded with the DOS MZ loader
(`Old-style DOS Executable (MZ)`), language `x86:LE:16:Real Mode`,
compiler spec `default`, and `-noanalysis`. No custom loader options were
specified. The default compiler spec is not a verified Pascal calling
convention. Loader-created names are not recovered semantic names.

The loader uses analysis load segment `1000`. For this import's loaded image,
physical analysis offset = module linear offset + `0x10000`. This is a static
mapping, not a measured DOS runtime load address.

Verified entry mapping:

- Module-relative `0000:ED60`, module linear offset `0xED60`.
- File offset `0x11AA0`, after the `0x2D40`-byte MZ header.
- Ghidra address `1000:ed60`.
- First instruction's encoded far-call operand was relocated from
  `2CE4:0000` on disk to `3CE4:0000` in analysis memory.

See [the generated report](initial-import.txt) for memory blocks and the first
12 decoded instructions. The pseudo-disassembler may display far-call targets
using equivalent segment:offset aliases: its `3000:ce40` denotes the same
physical address as the encoded `3CE4:0000`. Do not mistake the displayed alias
for the actual segment operand or disregard logical CS when analyzing a callee.
These instructions were decoded without creating function boundaries.

The entry begins with eight far calls, followed by `ENTER 0x200,0x0`.
**Observed:** this instruction sequence. **Inferred:** the calls may include
Pascal unit initialization. Later tracing established DS setup and found
the outer dispatcher and gameplay-loop candidates; see
[High-level program structure](HighLevelStructure.md). Several initialization
callees still lack semantic identification.

Import and script logs are under `projects/`. The project database has been
saved there; the reports/scripts are tracked candidates, but nothing has been
committed automatically.

## Address and naming conventions

The executable SHA-256 identifies the binary to which addresses apply.
Keep these address spaces distinct:

- **File offset:** byte index in the executable, including the MZ header.
- **Module linear offset:** byte index from the start of the DOS load module.
  For file-backed bytes, file offset = header size + module linear offset.
- **Module-relative segment:offset:** a segmented address before adding the
  DOS load segment. Linear offset = segment * 16 + offset.
- **Runtime segment:offset:** includes the actual DOS load segment for
  module-relative segment values. Heap and other runtime allocations need
  separate identification.
- **Ghidra address:** determined by the verified import mapping; do not
  silently equate it to a file or runtime address.

Use a stable identity such as `original:module:0x0000ED60` for a function
anchored at that module linear offset. Module-relative `0000:ED60` is an
address representation, not a guarantee of the runtime segment value.
Aliases can represent the same linear address; preserve meaningful segment
context in addition to the stable identity. Overlay code, if discovered,
will need an overlay identity too.

Start with names such as `sub_0000_ED60`, then rename as understanding grows.
Preserve the stable identity and previous names in exports. Do not assign
the sample name to an actual Ghidra function until its boundary is verified.
Use `field_0A` for an unknown field, and `candidate_...` for speculative names.

### Function parameter fidelity

Keep recovered function interfaces as close to the original as the evidence
allows. Preserve parameter count and order, observed widths and signedness,
pointer/reference representation, return values, and calling convention.
Record stack offsets, push order, and caller/callee cleanup where known.
Do not replace separate coordinates with a vector, hide arguments in a new
context object, or narrow a parameter merely because only part is consumed.

Distinguish ABI storage from source-language types: a word-sized stack slot
does not prove a Pascal `Word` parameter, and reading only its low byte does
not prove a Pascal `Byte` parameter. Mark unresolved types explicitly.
Conceptual C++ signatures are translations, not proof of original Pascal
declarations. Preserve original-width arithmetic and truncation separately
from any modern host ABI; identify necessary renderer adapters explicitly.

For the immutable 54-byte object layout, pass `RawGameObject&` directly to
recovered handlers and use named members such as `object.posX`. Do not add
wrappers or field-copy indirection around this known layout. Layout immutability
does not mean the gameplay values are const.

Use production game terminology for runtime symbols, such as `Desperabis`,
`Level`, and `GameContext`, rather than recovery-stage prefixes or suffixes.
Track implementation completeness in status values, tests, and analysis
documentation, not in the names of gameplay types.

Use enum members for gameplay identities, object fields, dispatch, and results.
Keep numeric conversions at binary/console boundaries; derive display names
from enum catalog entries rather than duplicating string identities in handlers.

## Evidence and persistent findings

Ghidra is the working source of truth for names, types, comments, and function
boundaries. Once it is set up, add reproducible exports of those annotations
to Git; do not independently maintain a competing editable symbol registry.
Keep the database backed up until export and restoration coverage is tested.
Documentation describes behavior, rationale, and open questions.

Classify claims as:

- **Remembered:** recollection by the original author.
- **Observed:** an instruction, byte sequence, or runtime value.
- **Inferred:** interpretation consistent with evidence, not yet confirmed.
- **Validated:** checked by a documented experiment.

For each investigation, record the question, binary hash, stable symbol IDs,
relevant instructions or field offsets, interpretation, evidence category,
reproduction steps, and remaining questions. Runtime evidence also needs
the savegame identity/hash, inputs, and timing/configuration where relevant.
Record field widths, signedness, packing, pointer representation, and units.
Distinguish serialized asset layouts from in-memory layouts until verified.

Keep notes focused on a behavior or structure rather than chat sessions.
Do not invent function boundaries or known fields to fill gaps.

## First investigation

The first investigation connected the author's menu/game-loop outline and
external DOSBox bridge to static code addresses. Results and open questions
are in [High-level program structure](HighLevelStructure.md).

Next, use the identified object-behavior entry and remembered object layout
to investigate a small behavior. Compare the instructions with the author's
memory, test the hypothesis in the original, and persist the result before
expanding the scope.

Existing leads:

- [Renderer documentation](../../Docs/NewRenderer.md): existing asset visualization.
- [Localization documentation](../../Docs/Localization.md): executable string
  patching and asset formats.
- [Executable patch table](../PackingTool/PatchedEXE.cpp): existing named file
  offsets, useful as candidate anchors after verifying the binary version.

Recover original behavior before translating it to C++. Preserve arithmetic,
update order, timing, and quirks until understood; controlled scenarios can
later become regression tests for the replacement gameplay system.
