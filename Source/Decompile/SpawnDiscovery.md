# Object-allocation discovery

This is an executable-wide **discovery index**, not a claim that every spawn,
weapon, or child behavior is recovered. No gameplay code or enum names were
changed by this investigation.

**Subsequent implementation:** behavior 18 and its supplied-object initializer,
the shared smoke initializer, and the complete `SpawnInCrater` emitter are now
implemented and host-tested. See [particle recovery](BehaviorRecovery.md#drifting-particles-and-shared-smoke-creation).
The discovery evidence remains unchanged; the table below uses current recovered names.

**Subsequent sword-hit identification:** the sword driver creates behavior-2
contact objects; on a Skeleton hit, `0000:11E3..11F8` converts that same object
through `10AC:07F2` into **behavior 18**, frame 2, before applying damage.
This identifies the separate nonfatal hit particle, not behavior-1 death parts.
The initializer/update and behavior-2 contact dispatch are implemented.
The player dagger/sword input/animation driver is now implemented; see
[melee sequences](BehaviorRecovery.md#player-melee-attack-sequence).
See [sword contact evidence](skeleton-sword-contact-evidence.txt),
[hit dispatch evidence](skeleton-hit-dispatch-evidence.txt), and
[the skeleton contract](BehaviorRecovery.md#skeleton-movement-combat-and-death).

## Results

The original executable contains **25 direct allocation-call matches**:

- All 25 have MZ relocation entries at their far-call segment operands.
- 24 have a bounded decoded path from a local `ENTER` prologue to the call.
- `0000:B070` remains a candidate: the analyzer cannot establish that local
  path without crossing an unsupported boundary.
- No additional same-segment near-call matches were found.
- Conditional object provenance links **19 distinct constant behavior-word
  stores** to allocated slots, including one level of helper initialization.
- Those stores assign **14 distinct behavior IDs**.
- **12 of those IDs are absent** from all 38 initial object-position files
  (2,920 records). This corroborates runtime use, not exclusive runtime use.

The durable machine-generated evidence is [spawn-discovery.txt](spawn-discovery.txt).
Complete bounded exports for selected initializers and individual store
anchors are in [spawn-initializer-evidence.txt](spawn-initializer-evidence.txt).
The already recovered fire branch also has [fireplace evidence](fireplace-evidence.txt).

### Child behavior IDs

These are observed assignments, not newly invented names. Numeric placeholders
remain until the update logic and selecting conditions justify a better name.
The following initial-record counts include every record, regardless of health.

| ID | Current name | Creation/initialization evidence | Initial records |
| --- | --- | --- | ---: |
| 1 | `Behavior_1` | Multiple branches in `10AC:091C`; also `0000:A8F7` | 0 |
| 2 | `WeaponDamageCollider` | Shared initializer `10AC:0384`, store `10AC:03D2` | 0 |
| 3 | `Behavior_3` | `0000:A8F7`, store `0000:AA0A` | 0 |
| 4 | `Behavior_4` | `0000:A8F7`, store `0000:AB65` | 0 |
| 18 | `DriftingParticle` | Initializer `10AC:07F2` and allocation inside `10AC:B33C` | 0 |
| 21 | `FireParticle` | Fire branch, store `10AC:09AC` | 0 |
| 24 | `SmokeFading` | Shared initializer `10AC:044E`, store `10AC:047A`; other direct creation | 0 |
| 33 | `WaterImpact` | Factory `10AC:0629`, store `10AC:0729` | 0 |
| 41 | `FlyingArrow` | Factory `10AC:4106`, store `10AC:4145` | 21 |
| 42 | `MercuryLamp` | Another branch of `10AC:4106`, store `10AC:4653` | 1 |
| 103 | `Behavior_103` | Factory `10AC:056C`, store `10AC:061F` | 0 |
| 106 | `Behavior_106` | Allocation inside `10AC:A467`, store `10AC:A862` | 0 |
| 114 | `RainDrop` | Shared initializer `10AC:D682`, store `10AC:D6F4` | 0 |
| 143 | `Behavior_143` | `0000:A8F7`, store `0000:B154` | 0 |

This supports investigating the low-numbered unknowns as runtime object
behaviors, but does **not** establish that they all mean projectiles.
In particular, behavior 1 is assigned in both fragment and other creation
paths. Behavior 42's existing name is not proof that every instance is an
ordinary lamp, or that its branch is selected by every caller.

## Allocation-site catalog

`Z` calls `1E48:2049` (first exactly-zero-health slot, otherwise cleared slot
165). `L` calls `1E48:2085` (free or lowest-health selection).
IDs include assignments found inside one directly called initializer; they
do not imply all branches execute together.

Behavior names in the context column come from the existing case catalog.
Names of callers of a shared routine indicate a connection, not which
internal branch each caller selects. The object dispatcher's six inline
allocation sites are identified by their catalog case intervals.

| Allocation site | Kind | Enclosing routine / context | Linked IDs or unresolved work |
| --- | --- | --- | --- |
| `0000:2456` | Z | `0000:1094`, `MetalPole` case | 24 via `10AC:044E` |
| `0000:3527` | Z | `0000:1094`, `RainEmitter` case | Clears a slot; result unused and no object reserved |
| `0000:3562` | Z | `0000:1094`, `RainEmitter` case | 114 via `10AC:D682`; usually selects that same still-free slot |
| `0000:3CC7` | Z | `0000:1094`, `Meteor` case | 24 via `10AC:044E` |
| `0000:3F0A` | Z | `0000:1094`, behavior 138 case | 24 via `10AC:044E` |
| `0000:3FAA` | Z | `0000:1094`, `SpawnInCrater` case | 24 via `10AC:044E` |
| `0000:A96D` | L | `0000:A8F7`, selected-resource/weapon-use cluster | 1, 2, 3, 4, 143 |
| `0000:B070` | Z | Candidate inside that cluster | No promoted stores: local CFG path unresolved |
| `0000:B720` | Z | `0000:B5E6` | 2 via `10AC:0384` |
| `0000:B9C9` | Z | `0000:B5E6` | 2 via `10AC:0384` |
| `0000:C2DC` | Z | `0000:B5E6` | 2 via `10AC:0384` |
| `10AC:0570` | Z | `10AC:056C` | 103 |
| `10AC:062D` | L | `10AC:0629` | 33 |
| `10AC:0782` | Z | `10AC:0767`, called by behavior 11 | 24 via `10AC:044E` |
| `10AC:0920` | Z | `10AC:091C`, also called by `BurningFireplace` | 1, 21, 24 |
| `10AC:128C` | L | `10AC:10F5`, called by behavior 2 and `MetalPole` | Unresolved |
| `10AC:1EE0` | Z | Same `10AC:10F5` routine | 24 via `10AC:044E` |
| `10AC:410A` | L | `10AC:4106`, called by `FireExhauster` and `KnightsCrossbow` | 41, 42 |
| `10AC:651A` | Z | `10AC:5FE0`, called by `Mummie` and `Gorilla` | 24 via `10AC:044E` |
| `10AC:A2C5` | Z | `10AC:9F31`, called by `Ghost` | 24 |
| `10AC:A7B5` | Z | `10AC:A467` | 18 via `10AC:07F2`, then a direct assignment of 106 |
| `10AC:B4D9` | Z | `10AC:B33C`, called by `FloodingWater` | 18 |
| `10AC:C4DF` | Z | `10AC:C432`, called by `MercuryLamp` | Passes slot to `10AC:C1EC`; no constant child behavior established |
| `10AC:CC86` | Z | `10AC:CC18`, called by behavior 97 | 24 via `10AC:044E` |
| `10AC:D540` | Z | `10AC:D45F`, called by `DestroyableRoof` | 24 via `10AC:044E` |

The 18-to-106 initialization path is particularly useful: the analyzer records
both writes to the **same allocated object**. These are not necessarily two
separate particles. Similarly, a subsequent initializer can replace a
behavior previously assigned in a factory branch. Treat this as an assignment
index, not a list of guaranteed final behaviors.

## Most useful next investigations

1. **Player attack/resource-use cluster `0000:A8F7` and `0000:B5E6`.**
   The former reads the selected index at DS:`DDB6`, reads/decrements the
   known weapon/resource amounts at DS:`DD36`, allocates, and initializes
   several low-numbered behaviors. `10AC:0384` is shared across this cluster;
   it assigns behavior 2, radii, lifetime, mask `FE`, and player-relative
   coordinates. This is a strong lead for temporary weapon contact/damage
   objects. Exact weapon-to-child mapping and damage rules still need tracing.
2. **Other callers of shared `SmokeFading` initializer `10AC:044E`.**
   The initializer and `SpawnInCrater` are now implemented; other emitters
   still need their complete update paths traced before wiring.
3. **Other particle initializers:** 106, 33, 103, and 114. Behavior 18 and its
   initializer are now implemented, but the subsequent 106 override is not.
   Initializers establish starting values; update cases establish their roles.
4. **Shared behavior 1:** distinguish its fragment/projectile uses before
   choosing a name that is too specific.
5. **Unresolved sites:** `0000:B070`, `0000:3527`, `10AC:128C`,
   and `10AC:C4DF`. The last forwards the slot to a routine with a nested
   static-link call, so the conservative analyzer drops frame-local facts.

## How the analyzer works and what the evidence means

[DiscoverObjectSpawns.java](ghidra_scripts/DiscoverObjectSpawns.java) reads
the hash-checked original MZ relocation table and imported initialized code
blocks. It scans for direct far allocator calls and same-segment near calls.
Matching bytes are initially **candidates**, not disassembled code.

For relocation-backed far calls it searches up to 16 preceding `ENTER`
patterns and attempts bounded control-flow decoding to the candidate.
This establishes a local instruction path, **not** whole-program reachability,
a verified function boundary, or branch feasibility. The same distinction
applies to the report's relocation-backed callers of an enclosing routine.

After a locally established allocation call, it tracks the returned AX slot
through register moves, BP locals, multiplication by 54, addition of world
array base `823C`, and saved/reloaded far pointers. A write to offset 8
without this identity and data-segment provenance is **not** promoted.
Both branch arms are explored and facts at joins are intersected.

Unknown register writes invalidate facts. Calls invalidate volatile register
facts. Explicit stack-frame address arguments invalidate BP facts; other BP
facts rely on no hidden alias mutation by callees. This is not a full alias
analysis, ABI proof, or instruction-semantics implementation.

The operand-stack suffix tracks pushes/pops and parameter removal proven by
bounded `RETF` paths. That avoids mistaking a slot pending below `Random`'s
arguments for an argument consumed by `Random`. Unknown cleanup drops stack
facts. A proven slot argument can seed one helper's BP parameter slots.
There is no arbitrary recursive interprocedural analysis or callee return-
value expression recovery.

Floating-point-emulator interrupts `34h..3Fh`, indirect/far jumps, another
prologue, undecodable bytes, further allocations, and analysis budgets bound
the traces. Unsupported paths and missing constants remain explicit.
The script does not create Ghidra instructions, references, functions,
labels, or data types; reports are created only at a new output path.

### What is not covered

- Indirect allocation calls and unrecognized aliases/wrappers.
- Creation by record copying, reserved-slot initialization, or reactivation
  without calling either allocator.
- An executable-wide inventory of behavior changes on existing objects.
- Full initializer expressions, selecting conditions, or final values on
  every feasible path.
- Runtime reachability, timing, collision/damage semantics, or visual meaning.

Consequently, **25 is an allocation-call-site count, not a total spawn count
or proof that all spawn mechanisms have been found**.

## Reproduction and validation

From the repository root, choose a **new** report path:

```powershell
$root = Join-Path $PWD 'Source\Decompile'
& (Join-Path $root 'StartGhidra.ps1') -Headless -HeadlessArguments @(
    (Join-Path $root 'projects'), 'Desperabis',
    '-process', 'DESPERAB.EXE', '-readOnly', '-noanalysis',
    '-scriptPath', (Join-Path $root 'ghidra_scripts'),
    '-postScript', 'DiscoverObjectSpawns.java',
    (Join-Path $PWD 'Game\Original\DESPERAB.EXE'),
    (Join-Path $root 'captures\new-spawn-discovery.txt'),
    '-log', (Join-Path $root 'projects\new-spawn-discovery.log')
)
.\Source\Decompile\TestSpawnDiscovery.ps1 `
    -ReportPath .\Source\Decompile\captures\new-spawn-discovery.txt
```

Check the Ghidra log for `Exported read-only allocation discovery`, not just
the process exit code. The script's built-in positive/negative tests cover
object/segment provenance, call clobbers, branch joins, partial local writes,
word constants, and operand-stack ordering.

[TestSpawnDiscovery.ps1](TestSpawnDiscovery.ps1) independently enumerates the
on-disk relocation-backed allocator calls, checks the site set and unresolved
candidate, known fireplace assignment, helper edges, unique stores/IDs, and
all initial-object asset counts. These checks pass on the preserved report.
This is static, host-side validation; no DOS execution was performed.
