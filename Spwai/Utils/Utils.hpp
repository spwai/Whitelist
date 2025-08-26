#pragma once

#include <string>
#include <vector>

#define VERSION "0.1.6"
#define ID "9e190a1f546146c4d3a5a0be367d1ef4ec0d8a4d68536c16b7069197112d0825"

std::string sanitizeName(const std::string& name);
std::string extractName(void** nameTag);
std::string downloadJson(const std::string& url);
void loadLists();
bool checkVersionAndExit();
bool isInList(const std::string& sanitizedName, const std::vector<std::string>& playerList);
bool isSpecialName(const std::string& sanitizedName);