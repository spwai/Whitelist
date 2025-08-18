#pragma once

#include <string>
#include <vector>
#include <unordered_set>

std::string sanitizeName(const std::string& name);
std::string extractName(void** nameTag);
std::string downloadJson(const std::string& url);
void loadLists();
bool isInList(const std::string& sanitizedName, const std::unordered_set<std::string>& playerList);
bool isSpecialName(const std::string& sanitizedName);