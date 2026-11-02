#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace bcpproduct {
void validateOutput(const std::filesystem::path& source,const std::filesystem::path& output);
void publishNewFile(const std::filesystem::path& ready,const std::filesystem::path& output);
std::string dimensionGroup(const std::string& row,const std::string& side,const std::string& sourceWall);
}
