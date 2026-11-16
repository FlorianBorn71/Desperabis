# Localization

This repo comes with a tool to create custom localizations for the game. There are the following components that require language specific assets:

- All the texts (obviously), including texts that are compiled into the .EXE,
- some 2d graphics like the "press any key" text overlay and - less important - the intro credits,
- the sound files related to the voice of the witch.

To make the process easy, all relevant assets have been extracted and converted to standard formats and put into the respective localization folder. The conversion tool takes it from there, converts back to Desperabis proprietary binary formats and packages everything up again.

## Modern game string resources

The DX11 game uses [AdditionalAssets/Strings.json](../Source/NewRenderer/AdditionalAssets/Strings.json)
for menus, chapter names, inventory/HUD labels, door/shop messages, item
descriptions, tutorial instructions and the final-level recognition message.
Book/story text remains in the original text assets. Developer UI, technical
diagnostics, user-entered save names, speech audio and text baked into artwork
are not part of this catalog.

One UTF-8 JSON file groups message catalogs by locale (`en-us` and `de-de`).
Within each locale, stable IDs map to `message` and translator `description`:

```json
{
  "en-us": {
    "hud_gold": {
      "message": "Gold: {amount}",
      "description": "Gameplay gold display. Keep the {amount} placeholder."
    }
  },
  "de-de": {
    "hud_gold": {
      "message": "Gold: {amount}",
      "description": "Gameplay gold display. Keep the {amount} placeholder."
    }
  }
}
```

Translate `message`, not the IDs. Keep `description` as translator context;
JSON does not permit comments, so this field records where/how a string is used.
The file is **UTF-8**, unlike the legacy DOS localization files below.
Use `\n` for line breaks and `<highlight>...</highlight>` for highlighted
key names. Preserve named placeholders such as `{amount}` and `{price}`;
their positions may change and repeated placeholders are supported.
Native font limitations still apply: Latin letters, digits, spaces, German
umlauts/eszett and punctuation from `0123456789.:>/-|~"!$<,+^#*%`.
Additional alphabets need font support first; unsupported non-ASCII letters
are rejected rather than silently replaced.

`tutorial_00` through `tutorial_13` retain the zero-based row IDs from
`TUR39.256`. The final level uses `TUR38.256` with its own `ending_00`, because
its row zero is unrelated to tutorial row zero. Object `generalPurpose3`
still chooses the texture slot and `itemValue` still chooses the row; only
the text source changes. The old texture lookup is commented out in
`GameContext::HandleTextPromptContact` for reverse-engineering reference.
`item_XX` IDs preserve the **one-based** `BEZEICH.TXT` index. Shop descriptions
130..141 use `generalPurpose1`, and the old `=` price marker becomes `{price}`.
Entries 142..151 are inventory cursor-over item names, **not** shop descriptions.
Chapter IDs
are one-based menu positions; their playable level order still comes from HERZ.

The catalog lives alongside modern textures and shaders, separate from the
legacy DOS patching/localization flow below. `GenerateBuiltInCpps` embeds it as
`Strings.json` in `SharedSource/IO/builtin/BuiltInStrings.cpp`, using the same
built-in file registry as the additional textures (`BuiltInTextures.cpp`).
Shaders remain in their separate DX11 bundle. Run the generator from its project
directory after editing the JSON, then rebuild the game/tests. No loose
localization file needs to be shipped beside the executable.

The default locale is `en-us`. The loader accepts an explicit locale
(`de-de` for German), leaving an entry point for a future external language
setting; that settings file and language-selection UI are not implemented yet.
All locales must contain the same IDs. Missing locales/IDs, duplicate fields,
malformed JSON and unsupported letters report explicit errors rather than
silently switching languages.

For contributors, restore the pinned header-only JSON parser before the first
Visual Studio or CMake build, from the repository root:

```powershell
nuget restore Source\packages.config -PackagesDirectory Source\packages -NonInteractive
```

The DX11 host calls `LoadStringResourcesJson` with bytes read through its
built-in file manager, independent of the working directory. Standalone core
hosts can also call `LoadStringResources(path, locale)`; core tests default to
`Localization/strings.json` in their working directory. CMake stages the same
multilingual additional asset there for CTest only.

## How to start

1) Create your own localization folder, by creating a copy of [the folder for the English localization](Data/SourceAssets/Localization/English). In this folder, you will find all relevant files that have language-specific content.
1) Modify all these files to your liking, but check out the constraints listed in the paragraph below.  
1) Create a copy of `PackEnglishVersion.bat` in the [Bin/Tools](Bin/Tools) folder. Modify all parameters in the `.bat` to point to your directory instead of the English version. 
1) Finally, run the `.bat` file and watch out for potential conversion errors in the output console.

If everything went fine, there should be game binaries in the output folder.

## File format constraints

I wanted to keep the conversion tool simple, so it's not very sophisticated with regards to supported input file formats. Here are the limitations:

- **Text files**: All text files must be kept in `ANSI`, not `Utf-8`. Umlauts are not supported by the in-game font, except for the German ones. You will notice in the .txt files, that German umlauts use the old DOS codepage.
- **2d graphics format**: Only `.tga` is supported. It must be a 24bit (or 32bit, but transparency is ignored), true color, non-RLE compressed image. Plain black denotes transparent parts.
Graphics do not have to have the exact same dimension (width/height) as the original, however when used as on-screen overlays, the alignment often assumes a certain size (roughly).
- **Sound files**: Only `.wav` is supported. It must be uncompressed, 11kHz, 8bit, mono. You can use a tool like [ffmpeg](https://ffmpeg.org/) to convert a sound file to this format. 

If an input file does not meet these requirements, the tool outputs an error message to the console.

## Detailed file info

There are a couple of files in the localization folder, so here is a complete list, along with important information.

| Filename | Notes |
| -------- | ----- |
| EndMsg.txt, StartMsg.txt | Plain text files that are written to the DOS console.|
| IngameKjarthan.txt | An in-game text prompted when Grant sees you.|
| IngameTrainingInstructions.txt | In-game instructions in the tutorial level. Keeping the line numbering is important! Use a single '\\' for newline within one item.|
|Items.txt| The list of items in the game. Keep the line numbering intact (also empty lines!) and also keep the '(' for some items.
|MissionsMenu.txt|The 3 missions in the main menu when you start a new game. The numbers below every mission denote the internal level indices. Keep them unchanged, or play with it ;-)|
|Story.txt| All chapters in the story shown before every level. There are some special characters being used. Some are for formatting, others are mandatory. See dedicated paragraph below.|
|EXEPatch.txt|A string table to patch the strings that are compiled into DESPERAB.EXE. See detailed info in the paragraph below.|
|PressAnyKey.tga|Shows the 'Press any key" prompt in the auto-run levels.|
|IntroText.\*.\*.tga|The credits in the museum intro sequence.|
|StoryImage.0.0.tga|In the story book, the first chapter begins with an ornate version of character 'Z'. This special character is a graphics and there are no other characters of this type in the font. If character 'Z' does not fit with your story, change this graphics. The grayscale defines the intensity.
|StoryImage.0.1.tga|Also in the story book, this denotes the handwritten version of 'The End'.
|Witch0.wav|The witch says "The dagger costs 30 gold coins."|
|Witch1.wav|The witch says "The sword costs 50 gold coins."|
|Witch2.wav|The witch says "The crossbow costs 50 gold coins."|
|Witch3.wav|The witch says "The fireballs costs 50 gold coins."|
|Witch4.wav|The witch says "The rat trap costs 45 gold coins."|
|Witch5.wav|The witch says "One arrow costs 5 gold coins."|
|Witch6.wav|The witch says "Medicine costs 20 gold coins."|
|Witch7.wav|The witch says "The arrow poison costs 60 gold coins."|

### Graphics conversion

Deperabis runs in 256 color mode with a fixed color palette. Accordingly, when input .tga files are converted, the conversion tool tries to find best match with the used color palette. This is of course a lossy process. To see what the conversion tool generated, it outputs the converted 256 color version of the .tga into subfolder "ConversionResult" of the conversion output (by default the "_temp" folder in your localization directory).

### Formatting in story.txt

File story.txt is of course the most important file to localize. It holds all texts shown in the story book. If you modify the text, the new length can be longer or shorter - no problem. Line and page breaks are inserted automatically.

However, there are some special characters defined which have the following meaning:

|Character|Meaning|
| --- | --- |
| // | Comments in story.txt start with '//'. Comments are for users during localization only and are not converted to output file. Comments can be inserted into any line of the text, but it must be a full new line for each comment. Adding '//' to the end of an actual text line will not be treated as a comment.
| @\<number> | Defines the anchor for each level (level number). These are mandatory, so keep them in.|
| #, #+, #++, ...| Formatting: Inserts new line(s), where the number of '+' defines the paragraph spacing.|
| þ\<char> (þ = hexcode 0xFE) | Inserts a graphics into the page, where the following character defines which one from a set of images. It's most commonly used for the underline ornament for each headline: There are two versions of the underline ornament, one for 'short' headlines (þK) and one for 'long' headlines (þL). It's recommended to keep all the graphics in place.|
| = | While line break happens automatically on whitespaces, the '=' character denotes a placement hint for hyphenation in a word. If not needed for a line break, it's ignored.|

The modern renderer reads the packed `LEVEL/TEXTE.TXT` sections selected by
`LEVELS.INF`, using original font metrics and automatic pagination. It burns
text and illustrations into the original-size parchment, with a 3D page turn
on any fresh keyboard press (Escape also advances one page). New games and
normal progression show the story; save restores and developer navigation
bypass it. See [story playback recovery](../Source/Decompile/BehaviorRecovery.md#story-introductions-and-3d-book-playback).

### Patching the .Exe

Patching the strings that are compiled into the .EXE is a very error prone process, because it requires some understanding where to patch, where to set the new string length, and finding the maximum string length. Usually, the new string may not be longer than the original one but there are exceptions (arrays of strings). **Flawed patching may cause the game to crash randomly at any point.**

To make this process less error prone, the tool abstracts away the actual patching and instead reads the strings from a string table text file. This string table file is named `EXEPatch.txt`. It consists of name/value pairs, where each entry is of the form:
```
# Maximum length : 13 characters.
$PressAnyKey:Press any key
```
The comment line that starts with '#' is just a hint for the user to inform about the maximum length of the string. The next line is a key/value pair, separated by colon. The key describes (roughly) what the string is used for. The conversion tool has a table of keys/file offsets so it knows where to patch the strings. It handles string length limitations accordingly and outputs an error where exceeded.

## Localization tool source

The C++ source code of the conversion tool is part of this repo. It's a self-contained Visual Studio solution [here](../Source/). It's a good reference for some of the internal file formats.
