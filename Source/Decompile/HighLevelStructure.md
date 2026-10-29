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

## UI pages, shared helpers, and element lists

Evidence: [bounded UI instruction export](ui-evidence.txt). This is static
recovery, not a running UI implementation. Names below are descriptive aliases.

**The architecture is a hybrid:** separate procedural page loops share input,
text painting, and presentation helpers. There are also genuinely data-driven
overlay/icon records, resource-meter descriptors, and configuration tables.
These should not be mistaken for a single universal page/widget interpreter.

### Outer page dispatch and F2

The application dispatcher at `0000:E8C4` selects a page using word DS:`DC5E`.
The page routines handle their own input and change that word to leave the page.

| State, decimal | Page | Entry |
| --- | --- | --- |
| `0` | Gameplay | `0000:E2A3` |
| `1`, `21` | Main menu | `0000:A625` |
| `2` | Settings | `0000:851D` |
| `3` | Configuration | `0000:92C0` |
| `4` | New-game/chapter selector | `0000:8299` |
| `6` | Load | `0000:9F6B` |
| `7` | Save, subject to the existing enable gate | `0000:A184` |
| `10` | Inventory | `0000:7C44` |

This is not the complete state enum. Other states invoke dialogs and
presentation routines whose exact roles remain partly unresolved.

The F2 route is explicit: the input wrapper reads an extended-key prefix,
then scan code `3C` at `0000:CA68`. If byte DS:`D4FA` is zero, it writes
state `10` at `CA73`. F1, scan code `3B`, writes state `1`.
Inventory is therefore a modal application page, not an object behavior.

### Shared UI services, not a universal page runner

| Suggested alias | Entry | Observed role |
| --- | --- | --- |
| `UiInputAvailable` | `0000:69AA` | Polls keyboard and an optional additional input device. |
| `ReadUiInput` | `0000:6A36` | Normalizes keyboard/device input to common command bytes. Up/down/left/right become `AC/B4/AF/B1`; Enter and Escape are `0D/1B`. |
| `DrawUiText` | `0000:7F3C` | Measures and paints Pascal text with position, style, and alignment arguments; also uses rectangle-copy helpers. No widget registration is observed here. |
| `WaitUiRetrace` | `0000:7F10` | Calls the common vertical-retrace wait. |
| `ResetUiPages` | `0000:7F1A` | Uses the page presentation helper to initialize/reset drawing pages. |
| `PresentPage` | `1E48:5BE8` | Presents/toggles drawing pages and updates framebuffer pointers. |

For example, main-menu drawing helper `0000:A568` makes explicit calls to
`DrawUiText` for its individual labels. Its owner, `A625`, contains selection,
animation, key handling, and explicit page-state assignments.
Configuration has its own loop at `92C0`, with a nested drawing helper at
`91F4`. The latter does use tables: it loops over eight rows, reading Y
coordinates at DS:`01DE + row*2`, captions at DS:`01EE + row*26`,
choice counts at DS:`02BE + row`, and selected choice bytes at
DS:`3128 + row`. Thus even within the procedural pages, repeated controls
can be table-driven.

### Registered overlay/icon records: 24 bytes

`0EF0:0542` appends an element, increments byte count DS:`D4A0`, and returns
the assigned byte index through a caller-supplied far pointer. Element `i`
is at **DS:`2AFC + i*24`**, with registration starting at index 1.
The same list supplies gameplay HUD elements and inventory cells.

| Record offset | Observed contents |
| --- | --- |
| `+0` | Word visual/asset selector; also passed to asset lookup `1E48:CDEF`. |
| `+2`, `+4` | Word screen X/Y supplied by the caller. |
| `+6`, `+8` | Zero initially; for positive asset selectors, filled from negated asset metadata words. Exact rendering meaning remains open. |
| `+10`, `+12` | Initialized to `10`, `25`; exact field names remain open. |
| `+14` | Caller-supplied word; setup uses `32` for several HUD entries and `2` for inventory entries. Exact mode semantics remain open. |
| `+16` | Caller-supplied word payload; inventory uses an item/action identifier. |
| `+18` | Another caller-supplied word selector; exact meaning remains open. |
| `+20` | State word initialized to zero and cleared on inventory exit; complete animation semantics remain open. |
| `+22` | Frame lookup-table state: focused `0`, ordinary `2`, selected-source pulse `0..4`. Setup also uses other values outside this page. |

The constructor and renderer prefix `1E48:4368` establish that these are
renderable overlay records, not 54-byte gameplay objects. There is no callback
or parent/child pointer in the recovered 24-byte layout.

Setup routine `0EF0:0648` first registers HUD elements, then saves the first
inventory element index in byte DS:`2DB1`. It creates twelve inventory cells
with nested loops over three rows and four columns:

```text
x = 10 + column * 50
y = 10 + row * 46
payload = 14 - (row * 4 + column)
```

The bottom row is registered in this exact left-to-right order:

| X/Y | Raw visual selector | Payload | Meaning |
| --- | --- | --- | --- |
| `10,182` | `4` | `100` | Arrow: return to gameplay. |
| `60,182` | `5` | `101` | Fork/knife: use the selected medicine item. |
| `110,182` | `2` | `1` | Visually left-hand inventory slot. |
| `160,182` | `130` (`82h`) | `2` | Visually right-hand inventory slot. |

These are raw selector words, not a claim that every value names a separate
unmodified image asset. The setup tail `0EF0:07A5..0895` is preserved in
[inventory-hands-render-evidence.txt](inventory-hands-render-evidence.txt).
The input loop navigates all sixteen entries as a 4-by-4 logical grid;
arrows stop at the edges rather than wrapping. This does not imply that
every entry has uniform visual spacing.

#### Author-provided inventory screenshot

The original-game screenshot confirms twelve backpack frames in three rows
of four, followed by a separated bottom row: return arrow, fork/knife, and
two hand slots. A key is visible in the left of the two hand slots.
The right side contains the health label/bar/percentage and resource icons.
The author confirms that these right-side icons render fullbright when their
corresponding inventory resource is present (for example, a nonzero health
potion amount). This brightness indicates availability, not cursor selection.
The fork/knife consumes health potion; the arrow closes inventory and resumes
gameplay. Exact consumption amount, input semantics and native brightness
calculation are not established by the screenshot alone; the author's subsequent
functional clarification is recorded below.

The author's second screenshot, with dagger and sword present, shows health
at 40%, a bright right-side medicine icon with quantity `25`, and a bright
arrow icon with count `1`; the other resource symbols remain dark. This
provides a concrete example of availability-dependent brightness independent
of focus. Items appear as bitmap icons inside the backpack and hand frames;
no separate 3D item preview is visible. A still screenshot does not establish
whether any of those bitmap icons animate or whether their pixels were
produced by the model renderer. The native branches below distinguish these
cases; the weapon pictures must not be assumed to be stored bitmap assets.

#### Item pictures: stored frames or rendered models

The inventory renderer reads **object byte 50**, currently provisionally named
`RawGameObject::pickupType`. It is not just a Boolean pickup flag, nor a
universal bitmap ID. The object also retains its ordinary model and frame
fields; no weapon-ID-to-icon switch is involved in these drawing branches.

- For model IDs `1..6`, byte 50 selects an image entry within the sprite asset,
  alongside `modelAnim`. The entry contains offsets and a bitmap pointer,
  which are passed to palette-aware blitter `1E48:2787`.
- For model IDs `31..50`, byte-50 values `1..14` select an associated stored
  bitmap pointer at model-descriptor offset `16Ah + value*4`, again blitted
  through `2787`.
- For those same model IDs, byte-50 values `15..220` instead take
  `1E48:45DD..4673`: the renderer passes the model descriptor, `modelAnim`,
  `animFrame`, `modelFrame2`, and blend weight to model-render routine
  `1E48:9C62`. It supplies fixed pose values `30,-60,30`, screen position from
  the inventory overlay, and depth parameter **`2*byte50 + 50`**.
  `1E48:A660` maps the object's display mode for this call; it is not the
  model-projection routine.

Across all original object tables, dagger and sword records have:

| Weapon | `modelId` | `modelAnim` | `animFrame` | Byte 50 | Preview depth |
| --- | --- | --- | --- | --- | --- |
| Dagger (`itemValue == 1`) | `39` | `2` | `1` | `22` | `94` |
| Sword (`itemValue == 2`) | `39` | `2` | `2` | `40` | `130` |

Both therefore use the **model-rendered inventory picture**, not the
associated-bitmap branch. Their different frame values select their shapes;
byte 50 adjusts the preview distance/size rather than identifying a separate
dagger or sword bitmap. This is compatible with a static icon-like appearance
in a screenshot and does not imply a separate enlarged or rotating preview.
The existing field/enum names remain unchanged pending a naming decision.
See [inventory-actions-evidence.txt](inventory-actions-evidence.txt),
[inventory-icon-evidence.txt](inventory-icon-evidence.txt), and
[inventory-model-preview-evidence.txt](inventory-model-preview-evidence.txt).

#### Author-confirmed inventory interactions

- Enter selects an item; Enter at another slot moves it or swaps it with the
  occupied destination. A selected item's frame oscillates from dark to
  fullbright. The author implemented this by blitting the frame icon with
  different lookup tables; the recovered sequence is documented below.
- Escape immediately returns to gameplay, including while an item is selected;
  it is not a separate first-stage selection-cancel command.
- The fork/knife consumes only enough health potion to restore full health,
  rather than exhausting the stored amount. The native loop broadly matches
  that intent, but has the refund/off-by-one behavior documented below.
- Weapons are placed in hand slots; left versus right does not inherently change
  their use. If both hands contain weapons, only one is used. The author
  tentatively recalls right-hand priority; the native slot-2-first scan now
  verifies this. The in-game "Haende tauschen" action swaps complete records
  in slots 1 and 2, using the configurable binding described below.
  This is not evidence that non-weapons are
  forbidden in hand slots: the screenshot shows a key in one.
- There is no general drop or examine action. A few puzzle-specific exceptions
  allow placing objects such as the mirror or vase; these require their own
  recovered gameplay branches rather than a universal inventory drop feature.
- Right-side symbols are quantity indicators, not interactive cells: for example,
  health-potion percentage and arrow count.

#### Verified selection and action contract

The modal loop at `0000:7C44` keeps **focus and selected source separately**.
Its selected-source grid index, local `BP-0E`, starts at `-1`; selecting an
item does not remove it from its slot. At `7D76`, Enter on a live item
(`health > 0`, payload below 28) with no selected source records that source
and resets its pulse phase. Otherwise Enter invokes nested helper `7B1E`.

The helper implements these actions:

| Focused payload | Action |
| --- | --- |
| `100` | Clear selection and return directly to gameplay, whether or not an item was selected. |
| `101` | With a source selected, invoke `7A33` on that item. No automatic search for another potion occurs. |
| `0..27` | With source payload `1..27`, swap the complete 54-byte source/destination records, queue both icons for redraw, and clear selection. |

The visible page only supplies inventory payloads `1..14`. The wider helper
range is not evidence for extra visible cells. Swapping the selected slot
with itself is allowed and clears selection. Neither this dispatcher nor
the swap helper `0000:C4E0` checks collision type to restrict hand slots to
weapons: keys and other items can be moved there. Escape immediately exits,
without first requiring selection cancellation.

The fork action handles **only collision type 2 (`Medicine`)**. A selected
nonmedicine item leaves the selection unchanged; pressing the fork with no
selection does nothing. Handled medicine clears selection, refreshes the
resource display in both pages, and clears the entire item record if its
remaining value is below 1. See
[inventory-actions-evidence.txt](inventory-actions-evidence.txt).

#### Exact native potion arithmetic

`0000:7A33` uses signed word `itemValue` as the stored quantity. For positive
quantity it clears auxiliary dword DS:`2DD8`, then runs:

```text
repeat:
    quantity -= 1
    upperClamped = AdjustMeter(playerLife, 0x00007333)
until upperClamped or quantity < 0
quantity += 1
```

The dose is the initialized dword at DS:`02F6`: **29491 raw 16.16 units**,
approximately `0.4499969482421875` life units, not one whole life point.
`1E48:D579` adds at value-record offset `+4`, then clamps to
`maximum << 16` and returns true when the signed value high word is
**greater than or equal to** the maximum at offset `+2`. It also retains
the native player-meter suppression gate DS:`2DDD`; the examples below
assume unsuppressed updates.

The final increment is unconditional after entering the loop. Consequently:

- Already full health consumes no quantity.
- Filling a gap of at most one dose also consumes no quantity.
- Exactly two doses of missing life consumes one quantity.
- If health cannot fill, starting with quantity `q > 0` permits `q+1`
  adjustment calls and finishes at quantity zero.
- Starting at quantity zero performs no adjustment; common medicine cleanup
  still removes the empty record and clears selection.

Thus a simple "ceil missing health / dose" subtraction would **not** reproduce
the original. Preserve this distinction when deciding the port's compatibility
policy; no corrected consumption rule has been implemented.
Raw constants and checked boundary examples are preserved in
[inventory-pulse-potion-probe.txt](inventory-pulse-potion-probe.txt).

#### Hand priority and gameplay hand swap

`0000:793E` checks native slot **2 before 1**, requiring collision type 21
(`Weapon`) and health exactly 1000. Together with the setup coordinates this
verifies **visually right-hand priority**. It then rebuilds the weapon-to-slot
registry by scanning slots `1..14`; positive weapon IDs are registered and a
later duplicate replaces the earlier registry entry. Priority of a held
weapon and duplicate-ID registry precedence are separate rules.

Gameplay keyboard controls at `0000:C717` read binding byte DS:`323F`,
index key state at DS:`3132 + binding`, and swap slots 1 and 2 only when
weapon-animation state DS:`DDBB == 0`. Both held-item overlays are queued for
redraw. The default initializer at `1E48:D8AC` assigns scan code **`0C`**
(the US-layout minus key; the printed legend depends on keyboard layout).
Configuration can override it; this is not a hard-coded H action.
The swap exchanges all 54 bytes. For selected weapon ID 6, `C4E0` also
updates the active hand reference so it follows the moved object.
See [inventory-hands-render-evidence.txt](inventory-hands-render-evidence.txt)
and the existing [keyboard export](player-movement-evidence.txt).

#### Selected-source brightness, not item movement

Local phase `BP-10` advances by **17 modulo 512 per inventory-loop iteration**.
For a selected source, `0000:7E54..7E9E` computes:

```text
shade = roundToNearestEven(sin(phase * 0.012271846484375) * 1.9 + 2.0)
sourceOverlay.frameLookupState = shade
queueRedraw(sourceOverlay)
```

The phase factor and amplitude are CS-relative 80-bit constants at
`7C2C` and `7C36`; the bias at `7C40` is single-precision `2.0`. This is the
**selected source frame's lookup-table pulse**, not a separate item-movement
or 3D-preview transform. It covers integer states `0..4`. The current focus
normally uses state `0` and the previous focus state `2`; the selected-source
pulse overrides its frame state independently of the current destination.

`1E48:4317` selects lookup address `DS:DC4A + state*256` in rendering mode 10.
Outside that mode, it instead selects `1900h` for state 0 and `1300h` otherwise.
This confirms a palette-remapped blit, not modern alpha fading. The source
loop establishes no milliseconds-based pulse period: exact phase recurrence
is 512 iterations, with approximately 30 iterations per sine cycle.
The raw floating-emulator bytes and a 512-phase host-math check are retained
in [inventory-pulse-potion-probe.txt](inventory-pulse-potion-probe.txt);
the sine/rounding runtime identification is established separately in
[Findings.md](Findings.md#rotation-lookup-tables-and-fixed-point-arithmetic).

#### Mirror and vase placement belongs to gameplay

The original object tables contain one mirror and one vase, both in
`OBJPOS34.DAT`: zero-based records **56** and **62**, corresponding to slots
58 and 64 under the current record-to-slot mapping. Both have weapon
`itemValue == 6` and behavior 165; their pickup collision types are 93
(`MirrorToGrab`) and 92 (`VaseToGrab`). The vase also carries group 30 in
word 36; the mirror carries zero.

At `0EF0:0CB4`, these collision cases change the object to collision 21
(`Weapon`), optionally activate the positive word-36 group, and re-enter
normal weapon pickup. The existing weapon-use branch for ID 6 at
`0000:ADB8..AE76` copies the held 54-byte record into a world slot, clears
held health, places it at the camera/player coordinates, sets vertical
velocity `-10` and negative player heading, and resets words 28, 32, 36 and 42.
It refuses this operation when the tested map bits satisfy `(pixel & 7) == 1`.
It does not add a generic inventory drop command or a special modal action.
Subsequent behavior-165 puzzle physics is outside this inventory analysis.
See [player-collision-evidence.txt](player-collision-evidence.txt),
[weapon-trigger-evidence.txt](weapon-trigger-evidence.txt), and the original
record checks in [inventory-pulse-potion-probe.txt](inventory-pulse-potion-probe.txt).

### Per-buffer redraw lists

Two Pascal `string[25]` records at DS:`2D7C` and DS:`2D96` are **lists of
element-index bytes, not visible text labels**. They are selected using
the current drawing-page word DS:`2B08`.

`1E48:CAFE` appends an element index to both lists, subject to its state
and nonzero-index gates. `0000:65B8` walks the current page's list and calls
`1E48:4368(index)`, then replaces that list with the short string at
DS:`D748`. This is strong evidence for per-buffer redraw scheduling; the
contents/purpose of the replacement list have not been fully traced.
The constructor setup also builds index strings for frequently updated HUD
elements.

This resolves the earlier consumed-key helper: the DS:`2DB8` / DS:`2DB9`
arguments identify held-item overlay elements. Consuming a key schedules
both buffers to refresh that icon; it does not perform extra gameplay
inventory deletion or edit a human-readable label.

### Resource/meter descriptors: 31 bytes

There is a second registration helper at `0EF0:04C8`, with word count
DS:`D510`. Descriptor `i` is at **DS:`D7EF + i*31`**, starting at index 1.

| Record offset | Observed contents |
| --- | --- |
| `+0` | Caller-supplied byte style/type; byte `+1` is not established. |
| `+2`, `+4` | Caller-supplied word layout coordinates. Drawing applies additional fixed offsets. |
| `+6` | Inline Pascal `string[20]`: one length byte and up to twenty label bytes. |
| `+27` | Far pointer to live resource/value data. |

Setup clears the count and registers two descriptors bound to DS:`2DBA`
and DS:`2DC4`. Inventory drawing routine `0000:7743` loops through this
list, draws labels, and reads the pointed-to words at `+2` and `+6` for
resource/meter calculations. The complete value-record layout, style enum,
and floating-point drawing calculations remain open.

### Inventory page lifecycle and remaining boundaries

The recovered inventory skeleton is:

1. Restore focus column/row from DS:`05B4` / DS:`05B6`.
2. Draw static/resource content into both drawing pages.
3. Run a dedicated loop while application state remains `10`.
4. Read normalized input, navigate the grid, and handle item selection/actions.
5. Change overlay selection states and queue old/new elements for redraw.
6. Update the selected-item preview, draw its description through `0000:79C9`,
   present a page, and run common overlay drawing through `0000:65B8`.
7. On Escape, set application state to `0`; on exit, save focus, reset
   selection/animation state, and call `0000:793E` to rebuild held-weapon indices.

The pulse span beginning at `0000:7E5A`, and part of the resource-meter
drawing, contain Turbo Pascal floating-point emulator sequences. The pulse
has now been decoded separately above; its raw bytes are retained rather than
misleading ordinary-x86 mnemonics. The exports still contain only useful
prefixes of some larger routines; an exported range is not a complete
function claim.

**Porting implication:** recover page-state/input/action logic separately from
overlay descriptions and drawing. The functional modal contract is recovered:
navigation, selection, swapping, medicine use, exit, hand priority and
gameplay placement do not need renderer-owned decisions. Remaining visual
recovery includes full per-item preview animation, the resource percentage/
text formatting, and the complete right-side availability-to-palette recipe.
The author's fullbright-when-available rule is confirmed context, not a claim
that every indicator's native rendering branch has already been decoded.
Exact DOS loop-to-wall-clock pulse timing also remains unmeasured.
No inventory implementation or new rendering API has been added.
The [outer application-flow skeleton](Scaffold/GameContext.cpp) is now
implemented as `GameContext` members, with native-valued
[menu enums](Scaffold/ApplicationFlow.h). Page bodies and complete game
initialization remain explicit throwing hooks; no UI rendering has been added.
The [chapter selector export](ui-chapter-evidence.txt) confirms that Escape
returns to the main menu and acceptance invokes `0000:0D30(1, chapter)`.
See [the scaffold contract](BehaviorRecovery.md#game-context-and-application-flow).

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
semantics were subsequently traced further: `D060` is the player movement
and contact update, ending at `E066`. Keyboard controls are built separately
by `0000:C603`, called from the input wrapper `0000:C9E0`; mouse input is
handled by `0000:C777`. See [player movement](Findings.md#player-movement-and-input)
and the exact [instruction evidence](player-movement-evidence.txt).
`CCD6` remains only partly classified.

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
