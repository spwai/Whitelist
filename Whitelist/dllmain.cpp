#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>

#include <MinHook.h>
#include <libhat.hpp>
#include "Utils/Utils.hpp"
#include "SDK/NametagEntry.hpp"
#include "SDK/HitResult.hpp"

typedef unsigned __int64 QWORD;

FILE* g_Console = nullptr;
HMODULE g_Module = nullptr;

using HitResultConstructor = __int64(__fastcall*)(__int64 a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5);
using ActorGetNameTag = void** (__fastcall*)(__int64 actor);
using MouseDeviceFeed = __int64(__fastcall*)(__int64 mouseDevice, char button, char action, __int16 mouseX, __int16 mouseY, __int16 movementX, __int16 movementY, char a8);
using NametagObject = QWORD * (__fastcall*)(__int64 a1, QWORD* a2, __int64 a3);

HitResultConstructor g_HitResultConstructor = nullptr;
ActorGetNameTag g_ActorGetNameTag = nullptr;
HitResultConstructor g_OriginalHitResultConstructor = nullptr;

MouseDeviceFeed g_MouseDeviceFeed = nullptr;
MouseDeviceFeed g_OriginalMouseDeviceFeed = nullptr;

NametagObject g_NametagObject = nullptr;
NametagObject g_OriginalNametagObject = nullptr;

std::vector<std::string> g_msrPlayers;
std::vector<std::string> g_qtPlayers;

bool g_RightMouseButtonHeld = false;
bool g_RenderingEnabled = true;
ULONGLONG g_LastMmbClickMs = 0;
bool g_MiddleMouseButtonHeld = false;
int g_MmbClickCount = 0;
ULONGLONG g_MmbClickTimer = 0;
bool g_MmbDoubleClickPending = false;
bool g_MmbToggleProcessed = false;

__int64 __fastcall hookedHitResultConstructor(__int64 a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5) {
	__int64 result = g_OriginalHitResultConstructor(a1, a2, a3, a4, a5);

	if (a4 != 0 && g_ActorGetNameTag) {
		void** nameTag = g_ActorGetNameTag(a4);
		if (nameTag) {
			std::string actorName = extractName(nameTag);
			if (!actorName.empty()) {
				std::string sanitizedName = sanitizeName(actorName);

				if (g_RightMouseButtonHeld && g_MiddleMouseButtonHeld && !g_MmbToggleProcessed) {
					if (!sanitizedName.empty()) {
						bool wasInQt = isInList(sanitizedName, g_qtPlayers);
						
						if (wasInQt) {
							g_qtPlayers.erase(std::remove(g_qtPlayers.begin(), g_qtPlayers.end(), sanitizedName), g_qtPlayers.end());
						} else {
							g_msrPlayers.erase(std::remove(g_msrPlayers.begin(), g_msrPlayers.end(), sanitizedName), g_msrPlayers.end());
							g_qtPlayers.push_back(sanitizedName);
						}

						g_MmbToggleProcessed = true;
						g_MmbClickCount = 0;
						g_MmbClickTimer = 0;
						g_MmbDoubleClickPending = false;
					}
					reinterpret_cast<HitResult*>(a1)->mType = HitResultType::NO_HIT;
					return result;
				}

				if (g_MiddleMouseButtonHeld && !g_RightMouseButtonHeld && !g_MmbToggleProcessed) {
					if (!sanitizedName.empty()) {
						bool wasInMsr = isInList(sanitizedName, g_msrPlayers);
						
						if (wasInMsr) {
							g_msrPlayers.erase(std::remove(g_msrPlayers.begin(), g_msrPlayers.end(), sanitizedName), g_msrPlayers.end());
						} else {
							g_qtPlayers.erase(std::remove(g_qtPlayers.begin(), g_qtPlayers.end(), sanitizedName), g_qtPlayers.end());
							g_msrPlayers.push_back(sanitizedName);
						}

						g_MmbToggleProcessed = true;
						g_MmbClickCount = 0;
						g_MmbClickTimer = 0;
						g_MmbDoubleClickPending = false;
					}
					reinterpret_cast<HitResult*>(a1)->mType = HitResultType::NO_HIT;
					return result;
				}

				if (!g_RightMouseButtonHeld && isInList(sanitizedName, g_msrPlayers)) {
					reinterpret_cast<HitResult*>(a1)->mType = HitResultType::NO_HIT;
					return result;
				}
			}
		}
	}

	return result;
}



__int64 __fastcall hookedMouseDeviceFeed(__int64 mouseDevice, char button, char action, __int16 mouseX, __int16 mouseY, __int16 movementX, __int16 movementY, char a8) {
	if (button == 2) {
		if (action == 1) {
			g_RightMouseButtonHeld = true;
		}
		else if (action == 0) {
			g_RightMouseButtonHeld = false;
		}
	}

	if (button == 3) {
		if (action == 1) {
			g_MiddleMouseButtonHeld = true;
			ULONGLONG now = GetTickCount64();

			if (now - g_MmbClickTimer > 1000) {
				g_MmbClickCount = 0;
				g_MmbDoubleClickPending = false;
			}

			g_MmbClickCount++;
			g_MmbClickTimer = now;

		}
		else if (action == 0) {
			g_MiddleMouseButtonHeld = false;
			g_MmbToggleProcessed = false;

			if (g_MmbClickCount >= 3) {
				loadLists();
				g_MmbClickCount = 0;
				g_MmbClickTimer = 0;
				g_MmbDoubleClickPending = false;
			}
			else if (g_MmbClickCount == 2) {
				g_MmbDoubleClickPending = true;
			}
		}
	}

	return g_OriginalMouseDeviceFeed(mouseDevice, button, action, mouseX, mouseY, movementX, movementY, a8);
}

QWORD* __fastcall hookedNametagObject(__int64 a1, QWORD* array, __int64 a3) {
	if (g_MmbDoubleClickPending) {
		ULONGLONG now = GetTickCount64();
		if (now - g_MmbClickTimer > 500) {
			g_RenderingEnabled = !g_RenderingEnabled;
			g_MmbClickCount = 0;
			g_MmbClickTimer = 0;
			g_MmbDoubleClickPending = false;
		}
	}

	QWORD* result = g_OriginalNametagObject(a1, array, a3);

	if (array && array[2] && array[3]) {
		__int64 startAddr = array[2];
		__int64 endAddr = array[3];

		if (startAddr && endAddr && startAddr < endAddr) {
			__int64 numNametags = (endAddr - startAddr) / sizeof(::NametagEntry);

			for (__int64 i = 0; i < numNametags; i++) {
				::NametagEntry* nametag = reinterpret_cast<::NametagEntry*>(startAddr + (i * sizeof(::NametagEntry)));

				if (!nametag) continue;

				try {
					if (!IsBadReadPtr(&nametag->bgColor, sizeof(nametag->bgColor))) {
						std::string actorName;

						if (nametag->namePtr && nametag->namePtr != 0 && !IsBadReadPtr(nametag->namePtr, 1)) {
							actorName = std::string(reinterpret_cast<const char*>(nametag->namePtr));
						}
						else if (!IsBadReadPtr(nametag->name, sizeof(nametag->name))) {
							actorName = std::string(nametag->name);
						}

						if (!actorName.empty()) {
							std::string sanitizedName = sanitizeName(actorName);

							if (!g_RenderingEnabled) continue;

							if (g_RightMouseButtonHeld) {
								if (isInList(sanitizedName, g_msrPlayers)) {
									nametag->bgColor.r = 0.5f;
									nametag->bgColor.g = 1.0f;
									nametag->bgColor.b = 0.85f;
									nametag->bgColor.a = 0.25f;
								}
								else if (isInList(sanitizedName, g_qtPlayers)) {
									nametag->bgColor.r = 1.0f;
									nametag->bgColor.g = 0.0f;
									nametag->bgColor.b = 0.0f;
									nametag->bgColor.a = 0.3f;
								}
								else {
									nametag->bgColor.r = 0.0f;
									nametag->bgColor.g = 0.0f;
									nametag->bgColor.b = 0.0f;
									nametag->bgColor.a = 0.0f;
								}
							}
							else {
								if (isInList(sanitizedName, g_msrPlayers)) {
									nametag->bgColor.r = 0.0f;
									nametag->bgColor.g = 1.0f;
									nametag->bgColor.b = 0.35f;
									nametag->bgColor.a = 0.5f;
								}
								else if (isInList(sanitizedName, g_qtPlayers)) {
									nametag->bgColor.r = 1.0f;
									nametag->bgColor.g = 0.0f;
									nametag->bgColor.b = 0.0f;
									nametag->bgColor.a = 0.3f;
								}
								else {
									nametag->bgColor.r = 0.0f;
									nametag->bgColor.g = 0.0f;
									nametag->bgColor.b = 0.0f;
									nametag->bgColor.a = 0.0f;
								}
							}
						}
					}
				}
				catch (...) {
					continue;
				}
			}
		}
	}

	return result;
}

void createConsole() {
	AllocConsole();
	freopen_s(&g_Console, "CONOUT$", "w", stdout);
	freopen_s(&g_Console, "CONIN$", "r", stdin);
	SetConsoleTitleA("Whitelist");
}

void scanSignatures() {
	std::string_view hitResultSig = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 F2";

	auto hitResultSignature = hat::parse_signature(hitResultSig);
	if (hitResultSignature.has_value()) {
		auto hitResultResult = hat::find_pattern(hitResultSignature.value(), ".text");

		if (hitResultResult.has_result()) {
			g_HitResultConstructor = reinterpret_cast<HitResultConstructor>(hitResultResult.get());
			g_OriginalHitResultConstructor = g_HitResultConstructor;
			std::cout << "HitResult constructor found" << std::endl;
		}
		else {
			std::cout << "HitResult constructor not found" << std::endl;
		}
	}

	std::string_view getNameTagSig = "48 83 EC 28 48 8B 81 28 01 00 00 48 85 C0 74 4F";

	auto getNameTagSignature = hat::parse_signature(getNameTagSig);
	if (getNameTagSignature.has_value()) {
		auto getNameTagResult = hat::find_pattern(getNameTagSignature.value(), ".text");

		if (getNameTagResult.has_result()) {
			g_ActorGetNameTag = reinterpret_cast<ActorGetNameTag>(getNameTagResult.get());
			std::cout << "Actor::getNameTag found" << std::endl;
		}
		else {
			std::cout << "Actor::getNameTag not found" << std::endl;
		}
	}

	std::string_view mouseDeviceFeedSig = "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 57 41 54 41 55 41 56 41 57 48 83 EC 60";

	auto mouseDeviceFeedSignature = hat::parse_signature(mouseDeviceFeedSig);
	if (mouseDeviceFeedSignature.has_value()) {
		auto mouseDeviceFeedResult = hat::find_pattern(mouseDeviceFeedSignature.value(), ".text");

		if (mouseDeviceFeedResult.has_result()) {
			g_MouseDeviceFeed = reinterpret_cast<MouseDeviceFeed>(mouseDeviceFeedResult.get());
			g_OriginalMouseDeviceFeed = g_MouseDeviceFeed;
			std::cout << "MouseDevice::feed found" << std::endl;
		}
		else {
			std::cout << "MouseDevice::feed not found" << std::endl;
		}
	}

	std::string_view nametagObjectSig = "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 10 FE FF FF 48 81 EC F0 02 00 00 0F";

	auto nametagObjectSignature = hat::parse_signature(nametagObjectSig);
	if (nametagObjectSignature.has_value()) {
		auto nametagObjectResult = hat::find_pattern(nametagObjectSignature.value(), ".text");

		if (nametagObjectResult.has_result()) {
			g_NametagObject = reinterpret_cast<NametagObject>(nametagObjectResult.get());
			g_OriginalNametagObject = g_NametagObject;
			std::cout << "NametagObject found" << std::endl;
		}
		else {
			std::cout << "NametagObject not found" << std::endl;
		}
	}
}

void initializeHooks() {
	if (MH_Initialize() != MH_OK) {
		std::cout << "Failed to initialize MinHook!" << std::endl;
		return;
	}

	if (g_HitResultConstructor) {
		if (MH_CreateHook(g_HitResultConstructor, &hookedHitResultConstructor, reinterpret_cast<LPVOID*>(&g_OriginalHitResultConstructor)) == MH_OK) {
			if (MH_EnableHook(g_HitResultConstructor) == MH_OK) {
			}
			else {
				std::cout << "Failed to enable HitResult::hitResult hook" << std::endl;
			}
		}
		else {
			std::cout << "Failed to create HitResult::hitResult hook" << std::endl;
		}
	}

	if (g_MouseDeviceFeed) {
		if (MH_CreateHook(g_MouseDeviceFeed, &hookedMouseDeviceFeed, reinterpret_cast<LPVOID*>(&g_OriginalMouseDeviceFeed)) == MH_OK) {
			if (MH_EnableHook(g_MouseDeviceFeed) == MH_OK) {

			}
			else {
				std::cout << "Failed to enable MouseDevice::feed hook" << std::endl;
			}
		}
		else {
			std::cout << "Failed to create MouseDevice::feed hook" << std::endl;
		}
	}

	if (g_NametagObject) {
		if (MH_CreateHook(g_NametagObject, &hookedNametagObject, reinterpret_cast<LPVOID*>(&g_OriginalNametagObject)) == MH_OK) {
			if (MH_EnableHook(g_NametagObject) == MH_OK) {

			}
			else {
				std::cout << "Failed to enable NametagObject hook" << std::endl;
			}
		}
		else {
			std::cout << "Failed to create NametagObject hook" << std::endl;
		}
	}
}

void initialize() {
	if (!checkVersionAndExit()) {
		return;
	}
	scanSignatures();
	loadLists();
	initializeHooks();
}

void cleanup() {
	if (g_Console) {
		fclose(g_Console);
		g_Console = nullptr;
	}

	MH_Uninitialize();
	FreeConsole();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		g_Module = hModule;
		DisableThreadLibraryCalls(hModule);

		CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
			initialize();
			return 0;
			}, nullptr, 0, nullptr);
		break;

	case DLL_PROCESS_DETACH:
		cleanup();
		break;
	}

	return TRUE;
}