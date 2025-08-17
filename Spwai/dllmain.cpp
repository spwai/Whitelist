#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>

#include <MinHook.h>
#include <libhat.hpp>
#include "Utils/Utils.hpp"

FILE* g_Console = nullptr;
HMODULE g_Module = nullptr;

using GameModeAttackFn = __int64(__fastcall*)(__int64 a1, __int64 a2, char a3);
using ActorGetNameTagFn = void** (__fastcall*)(__int64 a1);

GameModeAttackFn g_GameModeAttack = nullptr;
ActorGetNameTagFn g_ActorGetNameTag = nullptr;
GameModeAttackFn g_OriginalGameModeAttack = nullptr;

std::vector<std::string> g_msrPlayers;
std::vector<std::string> g_qtPlayers;

__int64 __fastcall hookedGameModeAttack(__int64 a1, __int64 a2, char a3) {
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
}

void initialize() {
    //createConsole();
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