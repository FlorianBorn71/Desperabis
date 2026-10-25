# High-level program structure

Applies to the original executable hash recorded in [Findings.md](Findings.md).
All code addresses below are **module-relative segment:offset**, not DOS runtime
addresses. In the current Ghidra import, add `1000` to the segment.

Evidence: [focused instruction export](high-level-evidence.txt), produced with
[InspectCode.java](ghidra_scripts/InspectCode.java). No automatic analysis or
runtime experiments were performed. Candidate names below are explanatory
aliases, not names recovered from Pascal source or installed in Ghidra.

## Author's remembered flow

Recorded from the author's description:

1. Initialization, probably including bulk resource storage in XMS memory.
2. Virtual resource access through `.dat` archives with a leading directory
   of filenames and offsets.
3. Splash screen, then main menu.
4. Main-menu options and settings/configuration submenus.
5. New-game chapter selection.
6. Level introduction through an animated book and text parsed from a file.
7. Gameplay frames: vertical synchronization, timer-derived simulation work,
   input/sound, object-behavior dispatch, rendering. Exact ordering uncertain.

This outline is a search guide; it is not all independently verified yet.

## Current match to the executable

| Component | Address / data | Evidence and interpretation |
| --- | --- | --- |
| Runtime/segment initialization | `2CE4:0000` | Loads DS with relocated segment `3E9E`, corresponding to module-relative `2E9E`. Observed. |
| Initial call sequence | `0000:ED60` | Eight far calls precede the main body. Several initialization roles remain unresolved. |
| Resource initialization | `0000:ED8C` onward | Passes Pascal strings `Patch`, then `Extrn` with a numeric suffix loop, then `Game`, `Music`, `Sounds` to `1E48:0483`. Observed names/calls; archive semantics still need tracing. |
| XMS interface detection | `1E48:044D` | `INT 2Fh` with AX=`4300`, checks AL=`80`, then AX=`4310`, saves ES:BX at DS:`ECB0`, calls the returned interface, compares AX with `0200`. Strong evidence for XMS support; bulk-upload behavior not yet traced. |
| Application state dispatcher | `0000:E8C4` | Repeated dispatch on DS:`DC5E`; returns when byte DS:`D4F9` becomes nonzero. Observed control flow. |
| Main menu | `0000:A625` | Handles selection and writes dispatcher states; calls drawing helper `0000:A568`. Main-menu identification is strongly supported by the referenced German menu strings. |
| New-game/chapter selection candidate | `0000:8299` | Reached through the New Game menu item; selector calls `0000:0D30` on acceptance. Exact chapter metadata is not decoded. |
| Settings / configuration candidates | `0000:851D`, `0000:92C0` | Reached through the corresponding main-menu choices and states. Their individual controls are not yet mapped. |
| Level-start candidate | `0000:0D30` | Called after the new-game selector and during startup/other transitions. Full resource-loading and story-book boundaries are unresolved. |
| Normal gameplay-loop candidate | `0000:E2A3` | State `0` handler, with timer-counter waiting, repeated updates, object-behavior call, and frame work. Strong structural match. |
| Alternate gameplay/presentation loop | `0000:E589` | Used by several special states; shares timing and object-behavior calls with the normal loop. Its exact demo/intro/replay roles remain unresolved. |
| Object-behavior routine | `0000:1094` | Matches the author's DOSBox hook after address conversion; iterates 54-byte records and dispatches on record offset `+08`. Author-supplied alias: `DESP_GamePlay`. |
| Vertical-retrace wait | `1E48:019D` | Polls bit 3 of the VGA status port derived from BIOS CRTC base at `0000:0463` plus 6; waits for the bit to clear then set. Observed hardware polling. |
| Tick interrupt handler | `1E48:C5B1` | Saves registers, establishes DS, increments DS:`DC52` only while below 15, restores registers, executes `IRET`. Installed for interrupt `1C` by calls at `0000:00B1` onward. |

The splash screen and animated story book have **not** been assigned verified
routine addresses. Finding a level-start candidate is not proof that the book
animation or its parser has been identified.

## Main-menu state transitions

The menu drawing helper references length-prefixed strings at code offsets
`A4F3`, `A50A`, `A516`, `A527`, `A53C`, `A54A`, and `A558`.
The first is the conditional Return to Game item.

Observed selection assignments and outer dispatcher destinations:

| Choice | DS:`DC5E` after selection | Destination |
| --- | --- | --- |
| Return to Game | `0` | `0000:E2A3` |
| New Game | `4` | `0000:8299` |
| Load Game | `6` | `0000:9F6B` |
| Save Game | `7`, only if enabled | `0000:A184` |
| Settings | `2` | `0000:851D` |
| Configuration | `3` | `0000:92C0` |
| Return to DOS | `FFFF` | Dispatch default sets byte DS:`D4F9` to 1 |

States `1` and `21` decimal both call the main menu. There are other states
for special sequences and transitions; this is not the complete state enum.

## Gameplay timing and ordering

For the normal-loop candidate, the observed core sequence is:

```text
0000:E343  frame-loop body begins
0000:E34E  call 1E48:403C
             -> 1E48:0150: set display start address
             -> 1E48:019D: wait for vertical retrace
0000:E353  wait until signed word DS:DC52 >= 1
0000:E35C  copy DS:DC52 to DS:D4CA, then clear DS:DC52
           repeat call 0000:D060(6) DS:D4CA times, on one branch
           additional conditional work
0000:E440  call 0000:CCD6(DS:D4CA)
0000:E45A  call 1E48:3351 with position/view-like arguments
0000:E463  call 0000:1094(DS:D4CA) -- object behavior
           further scene/frame work, including call 0000:60F7
0000:E504  loop back to E343 while exit byte is zero and state is 0
```

Input, sound, and individual rendering subroutines are not fully classified.
Independent tracing now confirms that `D060` contains player update/contact
handling: it calls the player overlap selector `0000:CE9E` at `D8D7`, then
dispatches on the selected object's collision byte at `D8FC`. This is not a
classification based only on loop ordering. Its complete movement/input
semantics and `CCD6` remain unresolved. The view-like arguments are a
hypothesis pending field identification.

The separate object-interaction dispatcher is `0EF0:09F8`, with ordinary and
grouped switch cases; it must not be conflated with the player-contact block
or the general target-interaction helper `10AC:10F5`. See
[player collision details](BehaviorRecovery.md#player-collision-and-object-interaction)
and [instruction evidence](player-collision-evidence.txt).

The alternate loop at `E589` has analogous counter handling at `E6F9`, repeated
calls to `D060(2)`, and calls the same object routine at `E774`. It loops back
to `E6E9` while its original dispatcher state is unchanged and the exit byte
is clear. Distinguishing these two loops matters when choosing runtime hooks.

**Static conclusion:** the remembered timer-derived workload is a strong match.
The interrupt caps the pending counter at 15; the loop snapshots it, clears it,
and passes the snapshot to object behavior. This is not evidence that the
entire object routine is called once per tick: it is called once at that frame
site with a tick-count argument, and at least some cases contain inner loops.
The exact interrupt cadence has not been recovered.

## Connecting the existing DOSBox bridge

The author supplied an external local source file:

```text
D:\privat\repos\SheetMusicOverlay\Desperabis\DosBox\core_normal.cpp
SHA-256: 0e44267eee54f7f7255a0fa6068c59fa18dc7d3af9d470f915d1adab773d0f83
```

At inspection time:

- Around line 422, the hook tests `currentCs == 0x92C4` and calls this the
  first instruction of `DESP_GamePlay`.
- `currentCs` is a physical instruction address (`core.cseip`), despite
  its name; `LOADIP` computes it from segment base plus instruction offset.
- `OnGameObjectsUpdated` hard-codes data-segment physical base `0x36C10`.
- Its record representation is 54 bytes. It copies the player from
  DS:`8272` and 140 subsequent records from DS:`82A8`.

Assuming those hard-coded addresses describe one load of this same binary:

```text
module-relative DS base = 0x2E9E * 16 = 0x2E9E0
runtime load base       = 0x36C10 - 0x2E9E0 = 0x8230
hook module offset      = 0x92C4 - 0x8230 = 0x1094
Ghidra address          = 1000:1094
```

The binary at that location independently supports the match:

- Begins `ENTER 0x42, 0`.
- Starts the record index at 2.
- Computes record address as DS:`823C + index * 0x36`; index 2 is `82A8`.
- Tests signed word at record `+0A` for positivity before processing.
- Uses word at `+08` as a comparison-cascade discriminator.

Subsequent author context and the existing renderer accessors identify `+0A`
as health and byte `+08` as behavior. The executable uses the full word at
`+08`, including a case for 300; preserve byte `+09` as well. See
[Object behavior recovery](BehaviorRecovery.md) for the connected layout
and the named-handler scaffold.

### Important record-count discrepancy

The bridge copies **140 non-player records**, but the executable loop's tail
at `0000:5511` compares the index against **`00A4` (164)** and loops back to
`0000:10A4` until that value is reached. Combined with the starting index 2,
this is **163 record slots**, not 140.

The tail bytes are:

```text
81 7E FC A4 00 74 03 E9 89 BB C9 C2 02 00
```

It returns with `RET 2` at `0000:551C`. These bytes can be reproduced with
an inspection range of `0000:5511:000E`.

The extra slots may have a special purpose, or the bridge may intentionally
copy only a subset. That interpretation needs the author's context. Do not
use the bridge's buffer length as the original array's total capacity.

The bridge's separate return-address comment (`0x363EE`) is not consistent
with the load-base conversion above for `0000:551C`; treat it as unresolved
historical information rather than a verified return anchor.

## Remaining work

1. Explain the 140-versus-163 record-slot difference.
2. Identify splash and book/parser boundaries inside the transition paths.
3. Trace `.dat` directory parsing and XMS resource transfers, not just detection.
4. Classify the frame subroutines and recover interrupt frequency.
5. Add validated names/types to Ghidra, then export those annotations.

## Reproducing instruction evidence

From the repository root, this read-only example exports the gameplay entry
and loop tail to a new ignored capture:

```powershell
$root = Join-Path $PWD 'Source\Decompile'
.\Source\Decompile\StartGhidra.ps1 -Headless -HeadlessArguments @(
    (Join-Path $root 'projects'), 'Desperabis',
    '-process', 'DESPERAB.EXE', '-readOnly', '-noanalysis',
    '-scriptPath', (Join-Path $root 'ghidra_scripts'),
    '-postScript', 'InspectCode.java',
    (Join-Path $root 'captures\object-entry-and-tail.txt'),
    '0000:1094:002E', '0000:5511:000E'
)
```

Create `captures` first if absent. The script refuses to overwrite an existing
output. Each `Range` heading in the focused export records the corresponding
hexadecimal segment, offset, and byte count for reproducing that section.

Linear decoding is not reachability analysis. For supported unprefixed relative
branches, the script appends a `logical module target` calculated from encoded
displacement and 16-bit IP wrapping. Prefer it to Ghidra's displayed target:
address normalization can otherwise lose a whole `0x10000` when a near branch
wraps within its logical code segment. Other normalized operands still require
careful checking.
