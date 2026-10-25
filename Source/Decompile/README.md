# Desperabis gameplay recovery

Recover the behavior of the original Turbo Pascal 7 DOS game and make it
available to the modern renderer. This directory holds reproducible analysis,
not original source code or an assertion that decompiled C is correct.

## Directory conventions

- `InspectExecutable.ps1`: read-only DOS MZ inspection and baseline generation.
- `InstallTools.ps1`: pinned official downloads, SHA-256 verification, and
  extraction. Resumes partial downloads; refuses existing installation
  directories rather than overwriting them. Do not run concurrent installers.
- `StartGhidra.ps1`: GUI/headless launcher using only the portable JDK.
- `ImportOriginal.ps1`: first import, with automatic analysis disabled.
- `ghidra_scripts/VerifyDosImport.java`: verifies the original binary,
  entry-point mapping, relocation, and initial instruction decoding.
- `original-executable.json`: generated identity and header baseline for the
  original executable. No timestamps or machine-specific paths.
- `initial-import.txt`: generated memory map and initial instruction report.
- `tools/` (ignored): optional workspace-local tool installations.
- `projects/` (ignored): local Ghidra projects. Back these up separately;
  Git does not preserve them.
- `captures/` (ignored): runtime dumps and debugger logs. Promote small,
  relevant evidence into tracked investigation notes when needed.

Do not modify the [original executable](../../Game/Original/DESPERAB.EXE).
Do not use the localized executable interchangeably: patching can change bytes
and invalidate evidence tied to a hash.

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
Pascal unit initialization. Their purposes and the main game loop are not yet
identified. We must inspect the callees and segment setup before assigning
semantic names.

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

The author remembers the in-memory game object layout and menu/game-loop
structure; details have not yet been recorded here.

After verifying the import, use one known object layout or recognizable
behavior as an anchor. Trace a small routine, annotate it, compare it with
the author's memory, test the hypothesis in the original, and persist the
result before expanding the scope.

Existing leads:

- [Renderer documentation](../../Docs/NewRenderer.md): existing asset visualization.
- [Localization documentation](../../Docs/Localization.md): executable string
  patching and asset formats.
- [Executable patch table](../PackingTool/PatchedEXE.cpp): existing named file
  offsets, useful as candidate anchors after verifying the binary version.

Recover original behavior before translating it to C++. Preserve arithmetic,
update order, timing, and quirks until understood; controlled scenarios can
later become regression tests for the replacement gameplay system.
