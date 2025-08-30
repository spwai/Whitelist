#pragma once

#include <string>
#include <vector>

#define VERSION "0.2.4"
#define ID "0de29395388e78e7b697a107b0b839ff18cd66707b55dfa5e0e4a3749d54a729"

std::string sanitizeName(const std::string& name);
std::string extractName(void** nameTag);
std::string downloadJson(const std::string& url);
void loadLists();
bool checkVersionAndExit();
bool isInList(const std::string& sanitizedName, const std::vector<std::string>& playerList);