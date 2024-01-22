# Localization

This repo comes with a tool to create custom localizations for the game. There are the following components that require language specific assets:

- All the texts (obviously), including texts that are compiled into the .EXE,
- some 2d graphics like the "press any key" text overlay and - less important - the intro text,
- the sound files related to the voice of the witch.

To make the process easy, all relevant assets have been extracted and converted to standard formats and put into the respective localization folder.

## How to start

1) Create your own localization folder, by creating a copy of [the folder for the English localization](Data/SourceAssets/Localization/English). In this folder, you will find all relevant files that have language-specific content.
1) Modify all these files to your liking, but check out the constraints listed below.  
1) Create a copy of PackEnglishVersion.bat in the [Bin/Tools](Bin/Tools) folder. Modify all parameters in the .bat to point to your directory instead of the English version. 
1) Finally, run the .bat file and watch out for potential conversion errors in the output console.

If everything went fine, there should be game binaries in the output folder.

## File format constraints

I wanted to keep the conversion tool simple, so it's not very sophisticated with regards to supported input file formats. Here are the limitations:

- **Text files**: All text files must be kept in ANSI, not Utf-8. Umlauts are not supported by the in-game font, except for the German ones. You will notice in the .txt files, that these use the old DOS codepage.
- **2d graphics format**: Only .tga is supported. It must be a 24bit (or 32bit, but transparency is ignored), true color, non-RLE compressed image. Plain black denotes transparent parts.
Graphics do not have to have the exact same dimension (width/height) as the original, however when used as on-screen overlays, the alignment often assumes a certain size (roughly).
- **Sound files**: Only .wav is supported. It must be uncompressed, 11kHz, 8bit, mono. You can use a tool like [ffmpeg](https://ffmpeg.org/) to convert a sound file to this format. 

If an input file does not meet these requirements, the tool outputs an error message to the console.

## Detailed file info

There are a couple of files in the localization folder, so here is some detailed info on each, along with important information.

| Filename | Notes |
| -------- | ----- |
| EndMsg.txt, StartMsg.txt | Plain text files that are written to the DOS console |
| IngameKjarthan.txt | An in-game text prompted when Grant sees you |
| IngameTrainingInstructions.txt | In-game instructions in the tutorial level. Keeping the line numbering is important! Use a single '\' for newline within one item.|
|Items.txt| The list of items in the game. Keep the line numbering intact (also empty lines!) and also keep the '(' for some items.
|MissionsMenu.txt|The 3 missions in the main menu when you start a new game. The numbers below every mission denote the internal level indices. Keep them unchanged, or play with it ;-)|
|Story.txt| All chapters in the story shown before every level. There are some special characters being used. Some are for formatting, others are mandatory. See dedicated paragraph below.|
|PressAnyKey.tga|Shows the 'Press any key" prompt in the auto-run levels.|
|IntroText.*.*.tga|The credits in the museum intro sequence.|
|StoryImage.0.0.tga|In the story book, the first chapter begins with an ornate version of character 'Z'. This special character is a graphics and there are no other characters of this type. If character 'Z' does not fit with your story, change this graphics.
|StoryImage.0.0.tga|Also in the story book, this denotes the handwritten version of 'The End'.
|Witch0.wav|The witch says "The dagger costs 30 gold coins."|
|Witch1.wav|The witch says "The sword costs 50 gold coins."|
|Witch2.wav|The witch says "The crossbow costs 50 gold coins."|
|Witch3.wav|The witch says "The fireballs costs 50 gold coins."|
|Witch4.wav|The witch says "The rat trap costs 45 gold coins."|
|Witch5.wav|The witch says "One arrow costs 5 gold coins."|
|Witch6.wav|The witch says "Medicine costs 20 gold coins."|
|Witch7.wav|The witch says "The arrow poison costs 60 gold coins."|
