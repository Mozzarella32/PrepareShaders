#include <filesystem>
#include <string>
#include <fstream>
#include <algorithm>
#include <unordered_set>
#include <map>
#include <vector>
#include <iostream>

const std::filesystem::path HashPath = ".hash";

size_t Hash;

bool HasChanged() {

	size_t OldHash = 0;
	{
		std::ifstream HashFile(HashPath);
		if (HashFile) {
			HashFile >> OldHash;
		}
	}

	std::cout << "Last Hash:    " << OldHash << "\n";

	std::cout << "Current Hash: ";

	std::stringstream HashString;
	HashString << __TIME__;
	for (const auto& de : std::filesystem::directory_iterator(std::filesystem::path("Source"))) {
		if (de.is_regular_file()) {
			HashString << de.last_write_time();
		}
	}

	Hash = std::hash<std::string>{}(HashString.str());

	std::cout << Hash << "\n";

	return OldHash != Hash;
}

int main(int argc, char* argv[]) {
	std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());

	std::cout << "ShadersToCharArray:\n";

	if (!HasChanged()) {
		std::cout << "All Up To Date\n";
		return 0;
	}

	std::unordered_set<std::string> Verts;
	std::unordered_set<std::string> Frags;

	std::cout << "Copyed Files: ";

	std::filesystem::remove_all("CharArrays");
	std::filesystem::create_directories("CharArrays");

	for (const auto& de : std::filesystem::directory_iterator(std::filesystem::path("Source"))) {
		if (de.is_regular_file()) {

			std::stringstream FileContent;
			FileContent << "static const char * ";
			auto Extention = de.path().extension().string();
			if (Extention.empty()) {
				continue;
			}
			Extention = Extention.substr(1, Extention.length() - 1);
			auto Name = de.path().stem().string() + "_" + Extention;
			FileContent << Name;
			FileContent << " = R\"---(";
			std::ifstream in(de.path());
			if (in) {
				FileContent << in.rdbuf();
			}
			FileContent << ")---\";";
			std::ofstream of(std::filesystem::path("CharArrays") / Name);
			of << FileContent.str();
			if (Extention == "vert") {
				Verts.emplace(de.path().stem().string());
			}
			if (Extention == "frag") {
				Frags.emplace(de.path().stem().string());
			}
		}
	}

	std::cout << Verts.size() << " .verts & " << Frags.size() << " .frags\n";

	std::cout << "Creating Mapping\n";
	std::map<std::string, std::vector<std::string>> VertsToFrags;

	for (const auto& v : Verts) {
		for (auto f = begin(Frags); f != end(Frags);) {
			f = std::find_if(f, end(Frags), [&v](const std::string& elem) {
				return elem.starts_with(v);
				});
			if (f != end(Frags)) {
				VertsToFrags[v].push_back(*f);
				f++;
			}
		}
	}

	std::cout << "Writing Shader_X_List.h\n";

	std::stringstream Ss;
	Ss << R"----(#pragma once

#include "pch.h"

#define XList_Shaders_Names \
)----";

	for (const auto& Vert : VertsToFrags) {
		for (const auto& Frag : Vert.second) {
			Ss << "X(" << Frag << ")\\\n";
		}
	}
	Ss << R"---(
#define XList_Shaders_Combined \
)---";

	for (const auto& Vert : VertsToFrags) {
		for (const auto& Frag : Vert.second) {
			Ss << "X(" << Vert.first << ", " << Frag << ")\\\n";
		}
	}

	std::string NewShaderXLists = Ss.str();

	Ss = std::stringstream();
	if (std::filesystem::exists("Shader_X_List.h")) {
		std::ifstream I("Shader_X_List.h");
		Ss << I.rdbuf();
		std::string OldShaderXLists = Ss.str();
		if (NewShaderXLists != OldShaderXLists) {
			std::ofstream O("Shader_X_List.h");
			O << NewShaderXLists;
			std::cout << "Old Shader X Lists =\n\""
				<< OldShaderXLists << "\"\n"
				<< "New Shader X Lists = \n\""
				<< NewShaderXLists << "\"\n";
		}
		Ss = std::stringstream();
	}
	else {
		std::ofstream O("Shader_X_List.h");
		O << NewShaderXLists;
	}
	
	std::cout << "Writing Shader_Includes.h\n";

	Ss << R"---(#pragma once

#include "pch.h"

)---";

	for (const auto& Vert : VertsToFrags) {
		Ss << "#include \"CharArrays/" + Vert.first + "_vert\"\n";
		for (const auto& Frag : Vert.second) {
			Ss << "#include \"CharArrays/" + Frag + "_frag\"\n";
		}
		Ss << "\n";
	}

	std::string NewShaderIncludes = Ss.str();

	Ss = std::stringstream();
	if (std::filesystem::exists("Shader_Includes.h")) {
		std::ifstream I("Shader_Includes.h");
		Ss << I.rdbuf();
		std::string OldShaderIncludes = Ss.str();
		if (NewShaderIncludes != OldShaderIncludes) {
			std::ofstream O("Shader_Includes.h");
			O << NewShaderIncludes;
			std::cout << "Old Shader Includes =\n\""
				<< OldShaderIncludes << "\"\n"
				<< "New Shader Includes = \n\""
				<< NewShaderIncludes << "\"\n";
		}
		Ss = std::stringstream();
	}
	else {
		std::ofstream O("Shader_Includes.h");
		O << NewShaderIncludes;
	}

	std::ofstream HashFile(HashPath);
	HashFile << Hash;

	return 0;
}