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