#pragma once

#include <string>
#include <vector>

std::string sanitizeName(const std::string& name);
std::string extractName(void** nameTag);
std::string downloadJson(const std::string& url);
void loadLists();
bool isInList(const std::string& sanitizedName, const std::vector<std::string>& playerList);
bool isSpecialName(const std::string& sanitizedName);