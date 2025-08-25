#pragma once

#include <string>
#include <vector>
#include "../SDK/MinecraftUIRenderContext.hpp"

std::string sanitizeName(const std::string& name);
std::string extractName(void** nameTag);
std::string downloadJson(const std::string& url);
void loadLists();
bool isInList(const std::string& sanitizedName, const std::vector<std::string>& playerList);
bool isSpecialName(const std::string& sanitizedName);

bool pointInRect(float x, float y, const RectangleArea& r);
void clampToRect(float& x, float& y, const RectangleArea& r);
void mapMouseToUI(float rawX, float rawY, const RectangleArea& fullRect, float& outX, float& outY);