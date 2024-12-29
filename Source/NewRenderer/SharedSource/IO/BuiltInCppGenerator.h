#pragma once
#include <DesperabisCommon.h>

namespace BuiltInCppFileGenerator
{
	void GenerateCpp(std::filesystem::path outCppFile, std::string_view initFunctionName, const std::vector<std::filesystem::path>& filenames, const std::vector<std::string>& outfilenames);
	void GenerateCpp(std::filesystem::path outCppFile, std::string_view initFunctionName, const std::vector<std::filesystem::path>& filenames, std::string_view pathToSubtract);
	void GenerateCpp(std::filesystem::path outCppFile, std::string_view initFunctionName, std::filesystem::path rootPath, std::string_view pathToSubtract);

	void AddSingleFile(std::ofstream& outCpp, std::filesystem::path, std::string_view usedFileName);
	void AddSingleFile(std::ofstream& outCpp, const std::vector<unsigned char>& data, std::string_view usedFileName);

	void EnumerateFilesRecursive(std::filesystem::path rootFolder, std::vector<std::filesystem::path>& filenames);
};
