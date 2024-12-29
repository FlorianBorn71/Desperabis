// GenerateBuiltInCpps.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "pch.h"

#include <iostream>
#include <IO/BuiltInCppGenerator.h>

int main()
{
    std::filesystem::path cwd = std::filesystem::current_path();

    //////////////////////////////////////////////////////////////////////
    // Shared Desp files
    //////////////////////////////////////////////////////////////////////
    if (false)
    {
#define ADD_ASSETFOLDER(folder)						\
            std::cout << "Generate "folder"\n";     \
            BuiltInCppFileGenerator::GenerateCpp(	\
				outPath / "BuiltIn"folder".cpp",    \
				"RegisterFiles_BuiltIn"folder,		\
				assetpath / folder,				    \
				assetRootPath						\
				);

        std::filesystem::path outPath = cwd;
        outPath.append("..\\SharedSource\\IO\\builtin");
        assert(std::filesystem::exists(outPath));
        std::filesystem::path assetpath = cwd;
        assetpath.append("..\\..\\..\\Data\\SourceAssets\\Original");
        std::string assetRootPath = assetpath.lexically_normal().string();

        ADD_ASSETFOLDER("ANIMS");
        ADD_ASSETFOLDER("GRAFIK");
        ADD_ASSETFOLDER("LEVEL");
        ADD_ASSETFOLDER("VEKT");
        //ADD_ASSETFOLDER("SAMP");

    }

    //////////////////////////////////////////////////////////////////////
    // DX11 CSO (Release)
    //////////////////////////////////////////////////////////////////////
    if (true)
    {
        std::cout << "Generate DX11 Shaders\n";
        // shaders:
        std::filesystem::path csoPath = cwd;
		csoPath.append("..\\x64\\Debug\\DesperabisAR\\AppX"); // for simplicity, we use Debug here
        std::filesystem::path additionalAssetPath = cwd;
        additionalAssetPath.append("..\\AdditionalAssets");

        std::filesystem::path cppOutPath = cwd;
        cppOutPath.append("..\\SharedSource\\DX11");
		assert(std::filesystem::exists(cppOutPath));
        assert(std::filesystem::exists(additionalAssetPath));
        assert(std::filesystem::exists(csoPath));

		std::vector<std::filesystem::path> listOfFiles;
		std::vector<std::string> outFiles;
        listOfFiles.push_back(csoPath / "StandardGeometry_VS.cso"); outFiles.push_back("StandardGeometry_VS.cso");
        listOfFiles.push_back(csoPath / "StandardGeometry_PS.cso"); outFiles.push_back("StandardGeometry_PS.cso");
        listOfFiles.push_back(csoPath / "MirrorReceiver_VS.cso"); outFiles.push_back("MirrorReceiver_VS.cso");
        listOfFiles.push_back(csoPath / "MirrorReceiver_PS.cso"); outFiles.push_back("MirrorReceiver_PS.cso");
        listOfFiles.push_back(csoPath / "Corona_VS.cso"); outFiles.push_back("Corona_VS.cso");
        listOfFiles.push_back(csoPath / "Corona_PS.cso"); outFiles.push_back("Corona_PS.cso");
        listOfFiles.push_back(csoPath / "Corona_GS.cso"); outFiles.push_back("Corona_GS.cso");
		listOfFiles.push_back(csoPath / "InFlames_VS.cso"); outFiles.push_back("InFlames_VS.cso");
		listOfFiles.push_back(csoPath / "InFlames_PS.cso"); outFiles.push_back("InFlames_PS.cso");

        listOfFiles.push_back(additionalAssetPath / "textures/water-normal.dds"); outFiles.push_back("textures/water-normal.dds");
        listOfFiles.push_back(additionalAssetPath / "textures/mainlens.dds"); outFiles.push_back("textures/mainlens.dds");
        listOfFiles.push_back(additionalAssetPath / "textures/flare13.dds"); outFiles.push_back("textures/flare13.dds");

        BuiltInCppFileGenerator::GenerateCpp(
            cppOutPath / "BuiltInShaders_Release.cpp",
            "RegisterFiles_BuiltInShaders",
            listOfFiles,
            outFiles
        );
    }

    //////////////////////////////////////////////////////////////////////
    // OpenGL
    //////////////////////////////////////////////////////////////////////
    if (false)
    {
        std::cout << "Generate OpenGL shaders\n";
        std::filesystem::path openglPath = cwd;
        openglPath.append("..\\SharedSource\\OpenGL");
        std::string openGlRootPath = openglPath.lexically_normal().string();
        std::filesystem::path outPathShaders = cwd;
        outPathShaders.append("..\\SharedSource\\OpenGL");

        BuiltInCppFileGenerator::GenerateCpp(
            outPathShaders / "BuiltInShaders.cpp",
            "RegisterFiles_BuiltInShaders",
            openglPath / "shaders",
            openGlRootPath
        );
    }


    std::cout << "All done.\n";
}
