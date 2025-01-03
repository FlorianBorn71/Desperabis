# Packing Tool

This command line tool can be used to generate new .DAT files for the original game from modified source files. 
The main use case is **localization** of the game.

For detailed information, refer to [the Desperabis localization chapter on github](https://github.com/FlorianBorn71/Desperabis/blob/main/Docs/Localization.md#how-to-start).

## Sample Usage

`PackingTool.exe -s ..\..\Data\SourceAssets\Original -s ..\..\Data\SourceAssets\Localization\English -t ..\..\Data\SourceAssets\Localization\English\_temp -o ..\..\Game\English`
