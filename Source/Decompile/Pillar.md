# Pillar: collision-map point writer

Binary identity: the original hash in [Findings.md](Findings.md).
Evidence: [pillar-evidence.txt](pillar-evidence.txt), exported read-only with
[InspectCode.java](ghidra_scripts/InspectCode.java). Each range heading records
the module-relative segment, offset, and byte count for reproduction.

## Result

**Observed:** behavior 8 (`Pillar`) writes a single byte, value `38h` (56), to
a 2D byte grid using the object's X and Z. The grid is reached through a
global far pointer at **DS:69C4**. Its coordinate resolution is **25 raw units
per sample**, or four sample intervals per 100-unit tile.

This agrees with the author's description of Pillar marking the collision
map. The exact effects of the marker bits on player movement have not yet
been followed through the player collision path. No runtime test has been
performed against the DOS game. The case is now implemented in
[Function_Pillar](Scaffold/BehaviorHandlers.cpp), calling the recovered helper
in [CollisionMap.cpp](Scaffold/CollisionMap.cpp). Host tests verify the point
write and original-width arithmetic; shared preprocessing/postprocessing and
full-game integration remain unrecovered.

**Author-provided interpretation (provisional):** treat `38h` as the hardcoded
blocking-wall value, named `CollisionMapValue_BlockingWall`. This names the
whole byte, not an individual flag. The author recalls surface flags such as
water and grass; their masks and the meanings of bits within `38h` remain
unresolved.

## Shared helper name

Canonical recovery name: **`SetCollisionMapPixelAtWorldXZ`**.

| Identity | Value |
| --- | --- |
| Module-relative entry | `10AC:387B` |
| Stable identity | `original:module:0x0001433B` |
| Current Ghidra entry | `20AC:387B` (physical alias `2000:433B`) |
| ABI-shaped conceptual signature | `void SetCollisionMapPixelAtWorldXZ(int16_t x, int16_t z, uint16_t value)` |
| Shared state | `collisionMapPointer`, far pointer at DS:`69C4` |

This name is used in the recovery notes and C++ implementation; the original
executable is unchanged.
Future references to this address should use the same name rather than a
Pillar-specific name: the helper accepts an arbitrary byte value.
The far-call ABI uses three word-sized stack arguments: BP+`0A` is X,
BP+`08` is Z, and BP+`06` holds the value argument. Only its low byte is
read. The original Pascal type and signedness of that third parameter are
unresolved: the slot could also hold a padded byte argument. The `uint16_t`
above represents the observed slot width, not a verified Pascal `Word` type.
The conceptual C++ declaration does not reproduce the real-mode far-call ABI.
No return value is established; `void` describes the observed side-effect
contract, not a recovered Pascal declaration.

The C++ helper is a `GameState` member with these three explicit
parameters. The instance supplies shared global state in place of DOS DS;
it does not add an argument to the original parameter list.
`collisionMap` directly references the inline `CollisionMap` struct.
The host normalizes the original far pointer to header offset zero.
The 16-bit address arithmetic is equivalent to rotating a captured DOS segment
so its map header starts at zero; importing a nonzero-offset segment must
perform that rotation first. The dummy map itself is allocated with exactly
`width * height + 4` bytes, not a full DOS segment. A computed address beyond
that allocation throws explicitly rather than reproducing a DOS memory
overwrite. Valid wrapped writes to the header remain modeled. Missing map
state still throws explicitly.

The confirmed caller is Pillar at `0000:147B`. Other callers have not been
exhaustively searched; the current behavior-case dependency catalog lists this
target only for Pillar.

## Call site and coordinate transform

At module-relative `0000:1466`, the dispatcher compares the behavior word to 8.
The case at `0000:146B` pushes, in order:

1. Object signed word `+0C`: X.
2. Object signed word `+10`: Z.
3. Constant `0038h`.

It calls `SetCollisionMapPixelAtWorldXZ` at `10AC:387B`, then continues at
shared postprocessing `0000:5110`. In conceptual source:

```cpp
SetCollisionMapPixelAtWorldXZ(object.posX, object.posZ, CollisionMapValue_BlockingWall);
```

The helper returns with `RETF 6`, consuming the three word-sized parameters.

The helper at `10AC:387B` computes:

```text
column = signed_divide(wrap16(X - 112), 25)
row    = wrap16(162 - signed_divide(wrap16(Z - 38), 25))
stride = wrap16(map.widthMinusOne + 1)

address = mapSegment : wrap16(mapOffset + low16(row * stride + column) + 4)
byte_at(address) = low8(value)
```

Signed division is x86 `IDIV`, truncating toward zero, not floor division.
The subtractions and final addressing use 16-bit arithmetic. The multiply
helper at `2CE4:0E1C` computes a 32-bit product; the final writer uses the
low word when adding to DI and does not normalize the segment on overflow.

For ordinary in-range coordinates without 16-bit wrap, readable pseudocode is:

```cpp
column = (x - 112) / 25;
row = 162 - (z - 38) / 25;
mapPixels[row * stride + column] = static_cast<uint8_t>(value);
```

Pillar supplies `CollisionMapValue_BlockingWall` (`0x38`) as `value`.
This is a straight byte assignment, **not** an OR operation. `38h` sets bits
3, 4, and 5 and clears the other bits in that byte. Their individual meanings
are unresolved. The helper contains no bounds check, radius calculation,
rectangle fill, or Y-coordinate test.

The stored origin offsets (112, 38, and inverted row base 162) are established
by instructions; why these particular constants were chosen is not yet known.

## First identified shared global

Provisional name: `collisionMapPointer`.

| Location | Width | Interpretation |
| --- | --- | --- |
| DS:`69C4` | 16 bits | Offset half of far pointer |
| DS:`69C6` | 16 bits | Segment half of far pointer |
| Pointee `+0` | unsigned 16 bits | `widthMinusOne`: maximum column index; row stride is this plus 1 |
| Pointee `+2` | unsigned 16 bits | `heightMinusOne`: maximum row index |
| Pointee `+4` onward | bytes | Row-major map pixels |

The author's recollection that both dimensions are decremented by one agrees
with the allocator's stored differences and its `(difference + 1)` allocation
factors. [BehaviorDispatch.h](Scaffold/BehaviorDispatch.h) expresses the
entire layout in one struct, without a separate header type:

```cpp
struct CollisionMap
{
    static constexpr std::size_t VARIABLE_SIZE = 200 * 200;

    std::uint16_t widthMinusOne;
    std::uint16_t heightMinusOne;
    std::uint8_t data[VARIABLE_SIZE];
};
```

`data` is a plain built-in array at offset 4, not a container, pointer, or
copied view. Offsets and the total size are compile-time checked. The provisional
capacity is 40000 pixels, matching the current 200x200 dummy. `Level` owns
a heap pointer (`std::unique_ptr<CollisionMap>`) rather than embedding the map.
`CreateDummyLevel()` allocates exactly `width * height + 4` bytes using
`::operator new`, initializes the object there, and transfers ownership to
that pointer. Compile-time checks ensure this allocation fits the complete
declared type. Pointer ownership releases it automatically when the level
is destroyed. Variable dimensions remain deferred until proper level loading.

`CreateDummyLevel()` supplies a zero-filled 200x200 map with header words
`199, 199`: 40000 zero-filled pixel bytes plus the four dimension bytes.
The console uses it by default, so Pillar can execute without an input map.
It supplies no recovered geometry, objects, or real level-loading logic.

For normal dimensions, actual width/height are the stored values plus one.
The recovered point writer still uses 16-bit stride arithmetic, including
wrapping `FFFFh + 1` to zero. The writer handles header-relative offset
wrapping explicitly; ordinary array indexing alone does not reproduce
those edge cases.

Startup establishes the main DS as module-relative segment `2E9E`.
Thus the pointer storage has module-linear location `353A4h` and current
Ghidra address `3E9E:69C4`. This is writable runtime memory beyond the
file-backed load module, not an executable file offset. The map buffer itself
is dynamically allocated; its runtime location is not established statically.

### Writer

`10AC:387B`: loads the pointer with `LES DI,[69C4]`, obtains the stride from
the pointee header, converts X/Z, and stores the byte at pointee `+4+index`.

### Reader

Canonical recovery name: **`GetCollisionMapPixelAtWorldXZ`**, at `1E48:CF49`.
Its ABI-shaped conceptual signature is:

```cpp
std::uint8_t GetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z);
```

X and Z are pushed in that order (BP+08 and BP+06). The result is returned
in AL, with `RETF 4` consuming the two word arguments. It reads the shared
map pointer at DS:`69C4`, converts coordinates using the writer's same
wrapped subtractions and signed divisions, and calls `1E48:0E8F`.
This is an original helper, not an abstraction added to the C++ scaffold.
`GameState::GetCollisionMapPixelAtWorldXZ(x, z) const` is implemented in
[CollisionMap.cpp](Scaffold/CollisionMap.cpp), including the lower-level
reader's bounds and byte-addressing behavior in the same method.

The lower-level reader at `1E48:0E8F` receives column, row, then a far map
pointer (BP+0C, BP+0A, BP+06 respectively; `RETF 8`). It returns zero if
column is outside **0..319** or row outside **0..239**, using signed inclusive
range checks through `1E48:00E4`. These are hardcoded limits, **not checks
against the map's stored dimensions**. Otherwise it returns the byte at:

```text
mapSegment : wrap16(mapOffset + row * wrap16(widthMinusOne + 1) + column + 4)
```

It reads the stored width to calculate stride, but does not read the stored
height. As with the writer, the offset wraps without advancing the segment.
The hardcoded limits alone do not make access safe for the smaller host dummy
allocation. The C++ reader throws explicitly if the resulting byte address
falls outside that allocation, or if no map is supplied. Original coordinate
bounds failures still return zero. Like the writer, it assumes a map pointer
normalized to header offset zero.

Host tests check all 65536 word values independently for X and Z, and every
accepted grid coordinate plus its immediate out-of-bounds border, across six
stride headers. They cover signed truncation and wrapping, zero/wrapped stride,
header reads after offset wrapping, ignored stored-height limits, read/write
agreement, non-mutation, and explicit allocation/missing-map errors. This is
static-instruction-derived host validation, not a DOS runtime comparison.

**Confirmed behavior caller:** `Behavior_11` (11), case `0000:1515`, calls
the world-coordinate reader at `0000:1580` with its updated X/Z, then masks
the byte with `07h`. If that low-three-bit value is not 1, it sets animation
frame byte +3 to 1, second-frame byte +2 to 2, and changes behavior to 12.
If the value is 3, it additionally calls `10AC:0767` with the object slot.
The latter helper's effect and the meanings of values 1 and 3 remain
unresolved; they are not yet named water/grass/blocking flags.

**Separate neighborhood reader:** `1E48:2A64` samples a 3x3 neighborhood
around converted world X/Z through `1E48:0E8F`. It masks each sample with a
caller-supplied byte, stores the masked center sample through an output
reference, and adjusts two referenced words based on nonzero neighboring
samples before applying sign/step normalization. Its callers and full
gameplay role remain to trace. This is a **reader**, not evidence for the
remembered closed-door 3x3 writer.

All five complete instruction ranges (range check, pixel reader, world
wrapper, behavior 11, and neighborhood reader) are preserved in
[collision-map-reader-evidence.txt](collision-map-reader-evidence.txt).
This does not yet establish the player-blocking predicate or an exhaustive
list of direct/indirect map-reading behaviors.

### Allocation/setup

The level-start path at `0000:07F4` passes a rectangle with starting coordinates
4 and 3, two upper coordinates calculated as `local * 4 + 5`, and the address
of DS:`69C4` to `1E48:0FD5`.

That helper derives absolute coordinate differences, allocates:

```text
4 + (columnDifference + 1) * (rowDifference + 1) bytes
```

It stores the returned far pointer through its output parameter and writes
the two differences into the four-byte header. The upper-coordinate locals'
exact meanings and resulting per-level dimensions are not yet established.
The grid is not claimed to have a fixed width or height.

This explains the possible 4-versus-5 recollection: **sampling pitch is four
per tile**, while inclusive endpoints, margins, and the `+5` setup constants
also appear. This code does not write a 4x4 or 5x5 pillar footprint; it writes
one sample.

## Closed-door 3x3 writer

The author's remembered door footprint is now confirmed separately from the
point writer: helper `10AC:3EE0`, called by the door helper at `10AC:40F0`,
writes three rows of three bytes each. Its three word arguments are X, Z,
and value in that push order (`RETF 6`); only the value's low byte is used.
It starts at column `wrap16(X - 137) / 25` and visits row offsets -1, 0, 1
around `wrap16(162 - wrap16(Z - 38) / 25)`, using the map's stored stride.
Each row uses the byte-fill helper `2CE4:1B8C` with a count of three.

At animation phase 100 the door supplies its word at byte 20 as the value;
otherwise it supplies `38h`. This identifies the source of the open-state
marker without yet assigning meanings to its individual bits. Both the
door helper and the 3x3 writer are preserved in
[switch-evidence.txt](switch-evidence.txt). Neither is implemented in C++ yet.
The 3x3 neighborhood reader at `1E48:2A64` is a different function.

## Example and next steps

Without overflow, X=137 and Z=63 give column 1 and row 161. The write is at
pixel offset `161 * stride + 1`, after the four-byte header.

Next:

1. Trace the map reader's callers into player movement and identify which
   marker bits block which actions.
2. Resolve per-level grid dimensions, map initialization, and reset order.
3. Model the map once in shared C++ game state, preserving the header/stride
   contract and explicit behavior for out-of-range coordinates.
4. Validate the implemented Pillar case against DOS runtime captures without
   claiming shared preprocessing or the remainder of the update is complete.

The writer is implemented and host-tested, but not yet validated against a DOS
runtime capture. Keep the raw address identity even if the provisional global
name is refined.
