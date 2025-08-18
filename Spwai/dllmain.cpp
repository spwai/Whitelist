#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#include <MinHook.h>
#include <libhat.hpp>
#include "Utils/Utils.hpp"
#include "SDK/NametagEntry.hpp"

typedef unsigned __int64 QWORD;

FILE* g_Console = nullptr;
HMODULE g_Module = nullptr;

using GameModeAttackFn = __int64(__fastcall*)(__int64 a1, __int64 a2, char a3);
using ActorGetNameTagFn = void** (__fastcall*)(__int64 a1);
using MouseDeviceFeedFn = __int64(__fastcall*)(__int64 a1, char a2, char a3, __int16 a4, __int16 a5, __int16 a6, __int16 a7, char a8);
using PerNametagObjectFn = QWORD*(__fastcall*)(__int64 a1, QWORD* a2, __int64 a3);

GameModeAttackFn g_GameModeAttack = nullptr;
ActorGetNameTagFn g_ActorGetNameTag = nullptr;
GameModeAttackFn g_OriginalGameModeAttack = nullptr;

MouseDeviceFeedFn g_MouseDeviceFeed = nullptr;
MouseDeviceFeedFn g_OriginalMouseDeviceFeed = nullptr;

PerNametagObjectFn g_PerNametagObject = nullptr;
PerNametagObjectFn g_OriginalPerNametagObject = nullptr;

std::vector<std::string> g_msrPlayers;
std::vector<std::string> g_qtPlayers;

bool g_RightMouseButtonHeld = false;

__int64 __fastcall hookedGameModeAttack(__int64 a1, __int64 a2, char a3) {
    if (g_RightMouseButtonHeld) {
        return g_OriginalGameModeAttack(a1, a2, a3);
    }
    
    if (g_ActorGetNameTag) {
        void** nameTag = g_ActorGetNameTag(a2);
        
        if (nameTag) {
            std::string actorName = extractName(nameTag);
            
            if (!actorName.empty()) {
                std::string sanitizedName = sanitizeName(actorName);
                
                if (isInList(sanitizedName, g_msrPlayers)) {
                    return 0;
                }
            }
        }
    }
    
    return g_OriginalGameModeAttack(a1, a2, a3);
}

__int64 __fastcall hookedMouseDeviceFeed(__int64 a1, char a2, char a3, __int16 a4, __int16 a5, __int16 a6, __int16 a7, char a8) {
    if (a2 == 2) {
        if (a3 == 1) {
            g_RightMouseButtonHeld = true;
        } else if (a3 == 0) {
            g_RightMouseButtonHeld = false;
        }
    }
    
    return g_OriginalMouseDeviceFeed(a1, a2, a3, a4, a5, a6, a7, a8);
}

QWORD* __fastcall hookedPerNametagObject(__int64 a1, QWORD* a2, __int64 a3) {
    QWORD* result = g_OriginalPerNametagObject(a1, a2, a3);
    
    if (a2 && a2[2] && a2[3]) {
        __int64 startAddr = a2[2];
        __int64 endAddr = a2[3];
        
        if (startAddr && endAddr && startAddr < endAddr) {
            __int64 numNametags = (endAddr - startAddr) / sizeof(NametagEntry);
            
            for (__int64 i = 0; i < numNametags; i++) {
                NametagEntry* nametag = reinterpret_cast<NametagEntry*>(startAddr + (i * sizeof(NametagEntry)));
                
                if (!nametag) {
                    continue;
                }
                
                try {
                    if (!IsBadReadPtr(&nametag->bgColor, sizeof(nametag->bgColor))) {
                        std::string actorName;
                        
                        if (nametag->namePtr && nametag->namePtr != 0 && !IsBadReadPtr(nametag->namePtr, 1)) {
                            actorName = std::string(reinterpret_cast<const char*>(nametag->namePtr));
                        } else if (!IsBadReadPtr(nametag->name, sizeof(nametag->name))) {
                            actorName = std::string(nametag->name);
                        }
                        
                        if (!actorName.empty()) {
                            std::string sanitizedName = sanitizeName(actorName);
                            
                            if (g_RightMouseButtonHeld) {
                                if (isInList(sanitizedName, g_msrPlayers) || isInList(sanitizedName, g_qtPlayers)) {
                                    nametag->bgColor.r = 0.0f;
                                    nametag->bgColor.g = 1.0f;
                                    nametag->bgColor.b = 0.5f;
                                    nametag->bgColor.a = 0.3f;
                                } else {
                                    nametag->bgColor.r = 0.0f;
                                    nametag->bgColor.g = 0.0f;
                                    nametag->bgColor.b = 0.0f;
                                    nametag->bgColor.a = 0.0f;
                                }
                            } else {
                                if (isInList(sanitizedName, g_msrPlayers)) {
                                    nametag->bgColor.r = 0.0f;
                                    nametag->bgColor.g = 1.0f;
                                    nametag->bgColor.b = 0.35f;
                                    nametag->bgColor.a = 0.5f;
                                } else if (isInList(sanitizedName, g_qtPlayers)) {
                                    nametag->bgColor.r = 1.0f;
                                    nametag->bgColor.g = 0.0f;
                                    nametag->bgColor.b = 0.0f;
                                    nametag->bgColor.a = 0.3f;
                                } else {
                                    nametag->bgColor.r = 0.0f;
                                    nametag->bgColor.g = 0.0f;
                                    nametag->bgColor.b = 0.0f;
                                    nametag->bgColor.a = 0.0f;
                                }
                            }
                        }
                    }
                } catch (...) {
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
    SetConsoleTitleA("Spwai");
}

void scanSignatures() {
    std::cout << "Scanning for signatures..." << std::endl;
    
    std::string_view attackSig = "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D AC 24 A0 FD FF FF 48 81 EC 60 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 50 02 00 00 45";
    
    auto attackSignature = hat::parse_signature(attackSig);
    if (attackSignature.has_value()) {
        auto attackResult = hat::find_pattern(attackSignature.value(), ".text");
        
        if (attackResult.has_result()) {
            g_GameModeAttack = reinterpret_cast<GameModeAttackFn>(attackResult.get());
            g_OriginalGameModeAttack = g_GameModeAttack;
            std::cout << "GameMode::attack found" << std::endl;
        } else {
            std::cout << "GameMode::attack not found" << std::endl;
        }
    }
    
    std::string_view getNameTagSig = "48 83 EC 28 48 8B 81 28 01 00 00 48 85 C0 74 4F";
    
    auto getNameTagSignature = hat::parse_signature(getNameTagSig);
    if (getNameTagSignature.has_value()) {
        auto getNameTagResult = hat::find_pattern(getNameTagSignature.value(), ".text");
        
        if (getNameTagResult.has_result()) {
            g_ActorGetNameTag = reinterpret_cast<ActorGetNameTagFn>(getNameTagResult.get());
            std::cout << "Actor::getNameTag found" << std::endl;
        } else {
            std::cout << "Actor::getNameTag not found" << std::endl;
        }
    }

    std::string_view mouseDeviceFeedSig = "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 57 41 54 41 55 41 56 41 57 48 83 EC 60";
    
    auto mouseDeviceFeedSignature = hat::parse_signature(mouseDeviceFeedSig);
    if (mouseDeviceFeedSignature.has_value()) {
        auto mouseDeviceFeedResult = hat::find_pattern(mouseDeviceFeedSignature.value(), ".text");
        
        if (mouseDeviceFeedResult.has_result()) {
            g_MouseDeviceFeed = reinterpret_cast<MouseDeviceFeedFn>(mouseDeviceFeedResult.get());
            g_OriginalMouseDeviceFeed = g_MouseDeviceFeed;
            std::cout << "Mouse device feed found" << std::endl;
        } else {
            std::cout << "Mouse device feed not found" << std::endl;
        }
    }
    
    std::string_view perNametagObjectSig = "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 10 FE FF FF 48 81 EC F0 02 00 00 0F";
    
    auto perNametagObjectSignature = hat::parse_signature(perNametagObjectSig);
    if (perNametagObjectSignature.has_value()) {
        auto perNametagObjectResult = hat::find_pattern(perNametagObjectSignature.value(), ".text");
        
        if (perNametagObjectResult.has_result()) {
            g_PerNametagObject = reinterpret_cast<PerNametagObjectFn>(perNametagObjectResult.get());
            g_OriginalPerNametagObject = g_PerNametagObject;
            std::cout << "PerNametagObject found" << std::endl;
        } else {
            std::cout << "PerNametagObject not found" << std::endl;
        }
    }
}

void initializeHooks() {
    if (MH_Initialize() != MH_OK) {
        std::cout << "Failed to initialize MinHook!" << std::endl;
        return;
    }
    
    if (g_GameModeAttack) {
        if (MH_CreateHook(g_GameModeAttack, &hookedGameModeAttack, reinterpret_cast<LPVOID*>(&g_OriginalGameModeAttack)) == MH_OK) {
            if (MH_EnableHook(g_GameModeAttack) == MH_OK) {
                // Hook successfully enabled
            } else {
                std::cout << "Failed to enable GameMode::attack hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create GameMode::attack hook" << std::endl;
        }
    }
    
    if (g_MouseDeviceFeed) {
        if (MH_CreateHook(g_MouseDeviceFeed, &hookedMouseDeviceFeed, reinterpret_cast<LPVOID*>(&g_OriginalMouseDeviceFeed)) == MH_OK) {
            if (MH_EnableHook(g_MouseDeviceFeed) == MH_OK) {
                // Hook successfully enabled
            } else {
                std::cout << "Failed to enable mouse device feed hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create mouse device feed hook" << std::endl;
        }
    }
    
    if (g_PerNametagObject) {
        if (MH_CreateHook(g_PerNametagObject, &hookedPerNametagObject, reinterpret_cast<LPVOID*>(&g_OriginalPerNametagObject)) == MH_OK) {
            if (MH_EnableHook(g_PerNametagObject) == MH_OK) {
                // Hook successfully enabled
            } else {
                std::cout << "Failed to enable PerNametagObject hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create PerNametagObject hook" << std::endl;
        }
    }
}

void initialize() {
    createConsole();
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