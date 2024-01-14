// PackingTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <format>
#include <vector>
#include <cassert>
#include <sstream>
#include <direct.h>

#include "FileLists.h"
#include "Texture.h"
#include "Sound.h"
#include "DatFile.h"
#include "FileUtils.h"

/*
TODO:
- GRAFIK\TASTE.256
- GRAFIK\TXTBILD.AN2
   - (0,0) = "Z"
   - (0,1) = "ENDE"
- GRAFIK\VORSPANN.AN2
- wav files conversion to target format with ffmpeg
*/


using namespace std;

#define SUCCEED_OR_RETURN(_f) if (!(_f)) return false;

////////////////////////////////////////////////////////////////////////////
/// Conversion functions from Input->Desperabis 
////////////////////////////////////////////////////////////////////////////


bool PlainCopyFiles()
{
	SUCCEED_OR_RETURN(FileUtils::PlainCopy("Items.txt", "LEVEL\\BEZEICH.TXT"));
	SUCCEED_OR_RETURN(FileUtils::PlainCopy("StartMsg.txt", "LEVEL\\STARTMSG.TXT"));
	SUCCEED_OR_RETURN(FileUtils::PlainCopy("EndMsg.txt", "LEVEL\\ENDMSG.TXT"));
	SUCCEED_OR_RETURN(FileUtils::PlainCopy("Story.txt", "LEVEL\\TEXTE.TXT"));
	return true;
}

bool ConvertTextToTextures()
{
	Texture t;
	SUCCEED_OR_RETURN(t.FromText("IngameKjarthan.txt"));
	SUCCEED_OR_RETURN(t.Save("GRAFIK\\TUR38.256"));
	SUCCEED_OR_RETURN(t.FromText("IngameTrainingInstructions.txt"));
	SUCCEED_OR_RETURN(t.Save("GRAFIK\\TUR39.256"));
	SUCCEED_OR_RETURN(t.FromMissionText("MissionsMenu.txt"));
	SUCCEED_OR_RETURN(t.Save("GRAFIK\\HERZ.256"));
	return true;
}

bool ConvertTGAToGraphics()
{
	Texture t;
	Palette standardPal;
	SUCCEED_OR_RETURN(standardPal.Load("GRAFIK\\BACKGR1.PAL"));

	SUCCEED_OR_RETURN(t.LoadTGA("PressAnyKey.tga", standardPal));
	SUCCEED_OR_RETURN(t.Save("GRAFIK\\TASTE.256"));
	SUCCEED_OR_RETURN(t.SaveTGA("ConversionResult\\PressAnyKey.tga", standardPal)); // output image as it would show in the game
	return true;
}


bool ConvertWAVToSMP()
{
	Sound smp;

	// Witch sounds (samples 109..116):
	for (int i = 0; i < 8; i++)
	{
		std::ostringstream smpName, wavName;
		smpName << "SAMP\\SOUND" << i + 109 << ".SMP";
		wavName << "Witch" << i << ".wav";
		string s = wavName.str();
		SUCCEED_OR_RETURN(smp.LoadWAV(s.c_str()));
		s = smpName.str();
		SUCCEED_OR_RETURN(smp.SaveSMP(s.c_str()));
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////
/// Conversion functions from Desperabis -> Input
///		Needed as a once-off step to populate the Localization folder.
///     See extractFromOriginal flag.
////////////////////////////////////////////////////////////////////////////


bool ConvertTexturesToText()
{
	Texture t;
	SUCCEED_OR_RETURN(t.Load("GRAFIK\\TUR38.256"));
	SUCCEED_OR_RETURN(t.ToText("IngameKjarthan.txt"));
	SUCCEED_OR_RETURN(t.Load("GRAFIK\\TUR39.256"));
	SUCCEED_OR_RETURN(t.ToText("IngameTrainingInstructions.txt"));

	SUCCEED_OR_RETURN(t.Load("GRAFIK\\HERZ.256"));
	SUCCEED_OR_RETURN(t.ToMissionText("MissionsMenu.txt"));
	return true;
}

bool ConvertSMPToWAV()
{
	Sound smp;

	// Witch sounds (samples 109..116):
	for (int i = 0; i < 8; i++)
	{
		std::ostringstream smpName, wavName;
		smpName << "SAMP\\SOUND" << i + 109 << ".SMP";
		wavName << "Witch" << i << ".wav";
		string s = smpName.str();
		SUCCEED_OR_RETURN(smp.LoadSMP(s.c_str()));
		s = wavName.str();
		SUCCEED_OR_RETURN(smp.SaveWAV(s.c_str()));
	}

	return true;
}

bool ConvertGraphicsToTGA()
{
	Texture t;
	Palette standardPal;
	SUCCEED_OR_RETURN(standardPal.Load("GRAFIK\\BACKGR1.PAL"));
//	SUCCEED_OR_RETURN(t.Load("GRAFIK\\TASTE.256"));
//	SUCCEED_OR_RETURN(t.SaveTGA("PressAnyKey.tga", standardPal));
	SUCCEED_OR_RETURN(t.LoadTGA("TestTGA.tga", standardPal));
	//SUCCEED_OR_RETURN(t.SaveTGA("TestTGA-result.tga", standardPal));
	SUCCEED_OR_RETURN(t.Save("GRAFIK\\TASTE.256"));
	return true;
}

int main(int argc, char* argv[])
{
	std::filesystem::path tempPath, outputPath;
	if (argc <= 1)
	{
		std::cout << "Command line args: " << std::endl;
		std::cout << "  -s <folder> : Adds one source root folder (absolute path) under which the unpacked game assets are located" << std::endl;
		std::cout << "  -t <folder> : Temp folder to create the patch files." << std::endl;
		std::cout << "  -o <folder> : Output folder for the .DAT files." << std::endl;
		return 0;
	}
	else
	{
//		std::cout << "Number of arguments: " << argc - 1 << std::endl;
		// Print each command line argument
		for (int i = 1; i < argc; ++i)
		{
//			std::cout << "  Argument " << i << ": " << argv[i] << std::endl;
			if (_stricmp(argv[i], "-s") == 0 && i < argc - 1)
			{
				FileUtils::AddInputDirectory(argv[i + 1]);
				i++;
			}
			if (_stricmp(argv[i], "-o") == 0 && i < argc - 1)
			{
				outputPath = argv[i + 1];
				i++;
			}
			if (_stricmp(argv[i], "-t") == 0 && i < argc - 1)
			{
				tempPath = argv[i + 1];
				i++;
			}
		}
	}

	const bool extractFromOriginal = false;
	if (extractFromOriginal)
	{
		FileUtils::SetOutputDirectory(tempPath);
		// Some playground code to do the inverse operations, not executed by actual tool
		/*
		string snd = DatFile::BuildFileList("SOUNDS.DAT", "SOUNDS");
		string music = DatFile::BuildFileList("MUSIC.DAT", "MUSIC");
		string game = DatFile::BuildFileList("GAME.DAT", "GAME");

		if (!ConvertSMPToWAV())
		{
			cerr << "An error occurred during sound extraction." << endl;
			return 2;
		}
		if (!ConvertTexturesToText())
		{
			cerr << "An error occurred during texture to text extraction." << endl;
			return 3;
		}
		*/
		if (!ConvertGraphicsToTGA())
		{
			cerr << "An error occurred during texture to text extraction." << endl;
			return 4;
		}

		cout << "Extracting localizable files was successful." << endl;;
	}
	else
	{
		// Conversion pass
		if (!tempPath.empty())
		{
			FileUtils::SetOutputDirectory(tempPath);
			cout << "Plain copy files...";
			if (!PlainCopyFiles())
			{
				cerr << "An error occurred during file copy stage." << endl;
				return 2;
			}
			cout << "success." << endl;

			cout << "Start packaging of text files...";
			if (!ConvertTextToTextures())
			{
				cerr << "An error occurred during text baking." << endl;
				return 3;
			}
			cout << "success." << endl;

			cout << "Start converting graphics files...";
			if (!ConvertTGAToGraphics())
			{
				cerr << "An error occurred during graphics conversion." << endl;
				return 4;
			}
			cout << "success." << endl;
			

			cout << "Start packaging of sound files...";
			if (!ConvertWAVToSMP())
			{
				cerr << "An error occurred during sound baking." << endl;
				return 5;
			}
			cout << "success." << endl;
		}

		// Packaging path
		if (!outputPath.empty())
		{
			if (!tempPath.empty())
			{
				// the scratch path is the topmost input path
				FileUtils::AddInputDirectory(tempPath, true);
			}
			FileUtils::SetOutputDirectory(outputPath);
			string datOutFile;
			if (!DatFile::Game.PackageAndSave(&datOutFile))
			{
				cerr << "An error occurred during packaging GAME.DAT." << endl;
				return 101;
			}
			cout << "Package successfully written to: " << datOutFile.c_str() << endl;
			if (!DatFile::Sounds.PackageAndSave(&datOutFile))
			{
				cerr << "An error occurred during packaging SOUNDS.DAT." << endl;
				return 101;
			}
			cout << "Package successfully written to : " << datOutFile.c_str() << endl;
			if (!DatFile::Music.PackageAndSave(&datOutFile))
			{
				cerr << "An error occurred during packaging MUSIC.DAT." << endl;
				return 101;
			}
			cout << "Package successfully written to: " << datOutFile.c_str() << endl;
		}
	}

	cout << "All successful." << endl;
	return 0;
}
