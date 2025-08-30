#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <format>

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

  std::cout << "Last Hash:    " << OldHash << " read from "
            << std::filesystem::absolute(HashPath) << "\n";

  std::cout << "Current Hash: ";

  std::stringstream HashString;
  for (const auto &de :
       std::filesystem::directory_iterator(std::filesystem::path("Source"))) {
    if (de.is_regular_file()) {
      HashString << static_cast<long long>(
          de.last_write_time().time_since_epoch().count());
    }
  }

  Hash = std::hash<std::string>{}(HashString.str());

  std::cout << Hash << "\n";

  return OldHash != Hash;
}

int main([[maybe_unused]]int argc, char *argv[]) {

  if(argc != 2){
    std::cout << std::format("Usage: {} direcotry\n", argv[0]);
    exit(-1);
  }

  const auto path = std::filesystem::path(argv[1]);

  if(!std::filesystem::is_directory(path)) {
    std::cout << std::format("{} is not a directory\n", path.string());
    exit(-1);
  }

  std::filesystem::current_path(path);

  std::cout << "ShadersToCharArray:\n";

  if (!HasChanged()) {
    std::cout << "All Up To Date\n";
    return 0;
  }

  std::set<std::string> Verts;
  std::set<std::string> Geoms;
  std::set<std::string> Frags;

  std::cout << "Copyed Files: ";

  std::filesystem::remove_all("CharArrays");
  std::filesystem::create_directories("CharArrays");

  for (const auto &de :
       std::filesystem::directory_iterator(std::filesystem::path("Source"))) {
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
      else if (Extention == "geom") {
        Geoms.emplace(de.path().stem().string());
      }
      else if (Extention == "frag") {
        Frags.emplace(de.path().stem().string());
      }
    }
  }

  std::cout << Verts.size() << " .verts & " << Geoms.size() << " .geoms &"
            << Frags.size() << " .frags\n";

  std::cout << "Creating Mapping\n";

  std::map<std::string, std::vector<std::string>> VertsToFrags;
  for (const auto &v : Verts) {
    for (auto f = begin(Frags); f != end(Frags);) {
      f = std::find_if(f, end(Frags), [&v](const std::string &elem) {
        return elem.starts_with(v);
      });
      if (f != end(Frags)) {
        VertsToFrags[v].push_back(*f);
        f++;
      }
    }
  }

  std::map<std::string, std::vector<std::string>> VertsToGeoms;
  for (const auto &v : Verts) {
    for (auto g = begin(Geoms); g != end(Geoms);) {
      g = std::find_if(g, end(Geoms), [&v](const std::string &elem) {
        return elem.starts_with(v);
      });
      if (g != end(Geoms)) {
        VertsToGeoms[v].push_back(*g);
        g++;
      }
    }
  }

  std::vector<std::tuple<std::string, std::string, std::string>> vertGeomFrag;
  std::vector<std::pair<std::string, std::string>> vertFrag;
  for (const auto& [vert, frags] : VertsToFrags) {
    auto geomIt = VertsToGeoms.find(vert);
    if (geomIt != VertsToGeoms.end() && !geomIt->second.empty()) {
      for (const auto& geom : geomIt->second) {
        for (const auto& frag : frags) {
          vertGeomFrag.emplace_back(vert, geom, frag);
        }
      }
    } else {
      for (const auto& frag : frags) {
        vertFrag.emplace_back(vert, frag);
      }
    }
  }

  std::cout << "Writing Shader_X_List.hpp\n";

  std::stringstream Ss;

  Ss << R"----(#pragma once

#include "pch.hpp"

#define XList_Shaders_Names \
)----";

  for (const auto& tuple : vertFrag) {
    Ss << "X(" << std::get<1>(tuple) << ")\\\n";
  }
  for (const auto& tuple : vertGeomFrag) {
    Ss << "X(" << std::get<2>(tuple) << ")\\\n";
  }

  Ss << R"---(
#define XList_Shaders_VertFrag \
)---";
  for (const auto& [vert, frag]: vertFrag) {
    Ss << "X(" << vert << ", " << frag << ")\\\n";
  }

  Ss << R"---(
#define XList_Shaders_VertGeomFrag \
)---";
  for (const auto& [vert, geom, frag] : vertGeomFrag) {
    Ss << "X(" << vert << ", " << geom << ", " << frag << ")\\\n";
  }

  Ss << "\n";

  std::string NewShaderXLists = Ss.str();

  Ss = std::stringstream();
  if (std::filesystem::exists("Shader_X_List.hpp")) {
    std::ifstream I("Shader_X_List.hpp");
    Ss << I.rdbuf();
    std::string OldShaderXLists = Ss.str();
    if (NewShaderXLists != OldShaderXLists) {
      std::ofstream O("Shader_X_List.hpp");
      O << NewShaderXLists;
      std::cout << "Old Shader X Lists =\n\"" << OldShaderXLists << "\"\n"
                << "New Shader X Lists = \n\"" << NewShaderXLists << "\"\n";
    }
    Ss = std::stringstream();
  } else {
    std::ofstream O("Shader_X_List.hpp");
    O << NewShaderXLists;
  }

  std::cout << "Writing Shader_Includes.hpp\n";

  Ss << R"---(#pragma once

#include "pch.hpp"

)---";

  for (const auto& [vert, frag] : vertFrag) {
    Ss << "#include \"CharArrays/" << vert << "_vert\"\n"
       << "#include \"CharArrays/" << frag << "_frag\"\n"
       << "\n";
  }

  for (const auto& [vert, geom, frag] : vertGeomFrag) {
    Ss << "#include \"CharArrays/" << vert << "_vert\"\n"
       << "#include \"CharArrays/" << geom << "_geom\"\n"
       << "#include \"CharArrays/" << frag << "_frag\"\n"
       << "\n";
  }

  
  for (const auto& vert : Verts) {
      Ss << "#include \"CharArrays/" << vert << "_vert\"\n";
      auto geomIt = VertsToGeoms.find(vert);
      if (geomIt != VertsToGeoms.end() && !geomIt->second.empty()) {
          for (const auto& geom : geomIt->second) {
          Ss << "#include \"CharArrays/" << geom << "_geom\"\n";
              for (const auto& frag : VertsToFrags[vert]) {
                  if (frag.starts_with(geom)) {
                    Ss << "#include \"CharArrays/" << frag << "_frag\"\n";
                  }
              }
          }
      } else {
          for (const auto& frag : VertsToFrags[vert]) {
               Ss << "#include \"CharArrays/" << frag << "_frag\"\n";
          }
      }
      Ss << "\n";
  }
  std::string NewShaderIncludes = Ss.str();

  Ss = std::stringstream();
  if (std::filesystem::exists("Shader_Includes.hpp")) {
    std::ifstream I("Shader_Includes.hpp");
    Ss << I.rdbuf();
    std::string OldShaderIncludes = Ss.str();
    if (NewShaderIncludes != OldShaderIncludes) {
      std::ofstream O("Shader_Includes.hpp");
      O << NewShaderIncludes;
      std::cout << "Old Shader Includes =\n\"" << OldShaderIncludes << "\"\n"
                << "New Shader Includes = \n\"" << NewShaderIncludes << "\"\n";
    }
    Ss = std::stringstream();
  } else {
    std::ofstream O("Shader_Includes.hpp");
    O << NewShaderIncludes;
  }

  std::ofstream HashFile(HashPath);
  HashFile << Hash;

  return 0;
}
