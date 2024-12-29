#include "pch.h"
#include <IO/BuiltInCppGenerator.h>
#include <filesystem>

void BuiltInCppFileGenerator::EnumerateFilesRecursive(std::filesystem::path rootFolder, std::vector<std::filesystem::path>& filenames)
{
	for (auto& entry : std::filesystem::directory_iterator(rootFolder))
	{
		//std::path
		std::filesystem::path p = entry.path();
		if (entry.is_directory())
		{
			EnumerateFilesRecursive(p, filenames);
		}
		else
		{
			filenames.push_back(p);
		}
	}
}


void BuiltInCppFileGenerator::GenerateCpp(std::filesystem::path outCppFile, std::string_view initFunctionName, std::filesystem::path rootPath, std::string_view pathToSubtract)
{
	std::vector<std::filesystem::path> filenames;
	EnumerateFilesRecursive(rootPath, filenames);
	GenerateCpp(outCppFile, initFunctionName, filenames, pathToSubtract);
}


void BuiltInCppFileGenerator::GenerateCpp(std::filesystem::path outCppFile, std::string_view initFunctionName, const std::vector<std::filesystem::path>& filenames, std::string_view pathToSubtract)
{
	size_t subtract = pathToSubtract.length();
	if (pathToSubtract.back() != '\\')
	{
		subtract++;
	}

	std::vector<std::string> outFiles;
	for (auto& f : filenames)
	{
		std::string usedFile = f.lexically_normal().string();
		usedFile = usedFile.substr(subtract);
		outFiles.push_back(usedFile);
	}

	GenerateCpp(outCppFile, initFunctionName, filenames, outFiles);
}


void BuiltInCppFileGenerator::GenerateCpp(std::filesystem::path outCppFile, std::string_view initFunctionName, const std::vector<std::filesystem::path>& filenames, const std::vector<std::string>& outfilenames)
{
	std::ofstream out(outCppFile);
	assert(out.good());

	out << "#include \"pch.h\"\n";
	out << "#include <IO/FileManagerBuiltIn.h>\n\n";

	//	out << "// Register\n";
	out << "void " << initFunctionName << "()\n{\n";

	for (int i = 0; i < filenames.size(); i++)
	{
		AddSingleFile(out, filenames[i], outfilenames[i]);
	}

	out << "}\n";
	out.close();

}

void BuiltInCppFileGenerator::AddSingleFile(std::ofstream& outCpp, std::filesystem::path pathToFile, std::string_view usedFileName)
{
	std::vector<unsigned char> data;
	std::ifstream in(pathToFile, std::ios::binary | std::ios::ate);
	std::streamsize size = in.tellg();
	assert(size >= 0);
	in.seekg(0, std::ios::beg);
	data.resize(size);
	in.read((char*)data.data(), size);
	in.close();

	AddSingleFile(outCpp, data, usedFileName);
}


void BuiltInCppFileGenerator::AddSingleFile(std::ofstream& outCpp, const std::vector<unsigned char>& data, std::string_view usedFileName)
{
	std::string name(usedFileName);
	
	// stringify the path properly so it can go into source file without escaping
	std::replace(name.begin(), name.end(), '\\', '/');

	outCpp << "    {\n";
	outCpp << "        static const unsigned char data[] = {";

	for (int i = 0; i < data.size(); i++)
	{
		if ((i % 1024) == 0) outCpp << "\n        ";
		if (i > 0) outCpp << ',';
		outCpp << (int)data[i];
	}
	outCpp << "};\n";

	outCpp << "        FileManagerBuiltIn::RegisterFile(L\"" << name << "\", data, sizeof(data));\n";
	outCpp << "    }\n\n";
}

