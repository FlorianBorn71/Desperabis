# Generate built-in C++ files

This tool converts all used game assets into C++ files that can be linked into the C++ project.
The files are written to folder `SharedSource\IO\builtin`.

Class `FileManagerBuiltIn` can read the asset content from them as if they were physical files on disk.

The big advantage is that the executable can be shipped as one self-contained file with practically zero asset loading time overhead.

## Usage

No command line arguments are used. Just re-run this tool whenever assets have changed (mostly shaders of course).
Run it with `Source\NewRenderer\GenerateBuiltInCpps` as the working directory.

DX11 shaders, including `Text_VS.hlsl` and `Text_PS.hlsl`, are precompiled by `ShaderCompiler-DX11`
into `AdditionalAssets\shaderbins`. Build that project before running this tool. The generator embeds
the `.cso` files in `SharedSource\DX11\BuiltInShaders_Release.cpp`; rebuild the player afterward.
Text shaders are loaded from these built-in assets, without runtime HLSL compilation.

Additional assets are split by category:

- `SharedSource\DX11\BuiltInShaders_Release.cpp`: DX11 shader bytecode only.
- `SharedSource\IO\builtin\BuiltInTextures.cpp`: modern water-normal and lens/flare textures.
- `SharedSource\IO\builtin\BuiltInStrings.cpp`: `AdditionalAssets\Strings.json`.
- `SharedSource\IO\builtin\BuiltInMapIcons.cpp`: the existing map icon bundle.

The strings file contains the `en-us` and `de-de` message
catalogs; the game defaults to `en-us`. After editing translations, run this
generator and rebuild the game/tests. No loose JSON deployment is required.

## Original assets

The original DOS-format asset bytes are already embedded in generated
`BuiltInANIMS.cpp`, `BuiltInGRAFIK.cpp`, `BuiltInLEVEL.cpp` and `BuiltInVEKT.cpp`
under `SharedSource\IO\builtin`. `BuiltInSAMP.cpp` supplies audio when
`D_BUILTIN_SAMPLES` is enabled. These remain separate from the modern additional
textures and strings. The disabled "Shared Desp files" generation block retains
the recipe for rebuilding the original bundles from `Data\SourceAssets\Original`;
normal generator runs do not rewrite them.

To regenerate only the audio bundle, pass
`--samples <original-SAMP-directory> <output-BuiltInSAMP.cpp>`.
This includes S_TAB.PRB, SOUND1..129.SMP and all shipped MUSIC tracks
(1, 6, 10, 11, 14 and 20), including the menu and castle cues.

`FileManagerBuiltIn::Initialize` registers all enabled bundles under their
original virtual filenames. Reads first use that in-memory registry and only
then search configured external data directories for files not embedded.
Registered files therefore take precedence over external files. Asset loaders
decode the original formats from those bytes; the original DOS executable is
not needed at runtime.