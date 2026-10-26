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
- `initial-import.txt`: generated memory map and initial instruction report.
- `high-level-evidence.txt`: focused instruction evidence for the control-flow map.
- `ui-evidence.txt`: UI page dispatch, F2 inventory navigation, shared input/text
  helpers, overlay/meter registration, and per-buffer element redraw lists.
  See [UI architecture](HighLevelStructure.md#ui-pages-shared-helpers-and-element-lists).
- `ui-chapter-evidence.txt`: chapter-selection cancellation and acceptance,
  including the original new-game initialization call.
- `Scaffold/GameContext.h/.cpp`: context class with all member definitions in
  one implementation file: application flow/page hooks, object allocation,
  sound/random numbers, collision/height queries, and prompts.
- `Scaffold/ApplicationFlow.h`: native-valued menu enums; outer page dispatch
  is implemented in `GameContext.cpp`. Page loops and loading remain
  explicit throwing hooks, not rendering or a playable application.
- `Scaffold/ApplicationFlowTests.cpp`: menu gates, full scripted page flow,
  startup failure handling, and routing coverage for every state word.
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
- `Scaffold/`: compilable selector, implemented Pillar/LightSource/PulsatingLight/Door/Flies/RatAndSnake/FloatingDebris/behavior-18/FireParticle/BurningFireplace/IdleAnimation/SmokeFading/SpawnInCrater/switch/WaterDrop cases,
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
- `Scaffold/Heightfield.cpp`: unpacked geometry stream loading for `Level`.
  Native floor/room queries and special surfaces are in `GameContext.cpp`.
- `heightfield-evidence.txt`: exact WaterDrop, height lookup, and Pascal
  array-layout instruction ranges.
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

The current investigation is [Pillar](Pillar.md), its collision-map writer,
and the corresponding reader/player-blocking rules. `BatIndoor`'s helper at
module-relative `10AC:3391` remains a later target.
Pillar's case and point writer, plus the object-local IdleAnimation and
SmokeFading cases, Flies, two switch behaviors, WaterDrop, LightSource, PulsatingLight, ordinary
doors, RatAndSnake, FloatingDebris, behavior 18, and the fireplace/crater particle paths are implemented
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
PulsatingLight varies its pulse speed, updates `field_28`, and adds a clamped-sine
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
Explicit pickup interactions now populate fourteen inventory objects and
update the known counters in the game context; grouped-switch interaction
arms its return timer. Doors support automatic closing, obstruction reopening,
the native 3x3 collision footprint, grouped activation, and consuming matching
inventory keys. `GameContext::PromptUserMessage` stores UTF-8 feedback and its
lifetime without drawing; `UpdateObjects` advances that lifetime once per
frame. See [doors and unlocking](BehaviorRecovery.md#ordinary-doors-and-key-unlocking)
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
Player movement, automatic contact responses other than the door occupancy
marker, inventory use, platform input, message rendering, and unsupported interactions remain
explicitly unimplemented. Follow
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
