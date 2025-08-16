#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>

#include <MinHook.h>
#include <libhat.hpp>

FILE* g_Console = nullptr;
HMODULE g_Module = nullptr;

using GameModeAttackFn = __int64(__fastcall*)(__int64 a1, __int64 a2, char a3);
using ActorGetNameTagFn = void** (__fastcall*)(__int64 a1);

GameModeAttackFn g_GameModeAttack = nullptr;
ActorGetNameTagFn g_ActorGetNameTag = nullptr;
GameModeAttackFn g_OriginalGameModeAttack = nullptr;

std::string sanitizeName(const std::string& name) {
    std::string sanitized;
    sanitized.reserve(name.length());
    
    std::string tempName = name;
    size_t pos = 0;
    
    while ((pos = tempName.find('§', pos)) != std::string::npos) {
        if (pos + 1 < tempName.length()) {
            tempName.erase(pos, 2);
        } else {
            tempName.erase(pos, 1);
        }
    }
    
    for (char c : tempName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') {
            sanitized += c;
        }
    }
    
    return sanitized;
}

std::string extractNameSafely(void** nameTag) {
    if (!nameTag) return "";
    
    if (IsBadReadPtr(nameTag, sizeof(void*)) == FALSE && *nameTag) {
        void* unionPtr = *nameTag;
        
        char buffer[17] = { 0 };
        if (IsBadReadPtr(unionPtr, 16) == FALSE) {
            memcpy(buffer, unionPtr, 16);
            
            bool validInline = true;
            int validLength = 0;
            
            for (int i = 0; i < 16; i++) {
                if (buffer[i] == 0) {
                    validLength = i;
                    break;
                }
                
                unsigned char ch = buffer[i];
                if (!(isprint(ch) || ch == 0xC2 || ch == 0xA7 || ch == 0xC3 || ch == 0xBA || ch == 0xE2 || ch == 0x94)) {
                    validInline = false;
                    break;
                }
                validLength = i + 1;
            }
            
            if (validInline && validLength > 0 && validLength < 16) {
                std::string name(buffer, validLength);
                name.erase(std::remove(name.begin(), name.end(), '§'), name.end());
                
                if (!name.empty()) {
                    return name;
                }
            }
        }
        
        if (reinterpret_cast<uintptr_t>(unionPtr) > 0x10000) {
            if (IsBadReadPtr(unionPtr, 1) == FALSE) {
                const char* potentialStr = reinterpret_cast<const char*>(unionPtr);
                size_t len = strnlen(potentialStr, 256);
                
                if (len > 0 && len < 256) {
                    int printableCount = 0;
                    
                    for (size_t i = 0; i < len; i++) {
                        unsigned char ch = potentialStr[i];
                        if (isprint(ch) || ch == 0xC2 || ch == 0xA7 || ch == 0xC3 || ch == 0xBA || ch == 0xE2 || ch == 0x94) {
                            printableCount++;
                        }
                    }
                    
                    if (printableCount > len / 2) {
                        std::string name(potentialStr, len);
                        name.erase(std::remove(name.begin(), name.end(), '§'), name.end());
                        
                        if (!name.empty()) {
                            return name;
                        }
                    }
                }
            }
        }
    }
    
    return "";
}

__int64 __fastcall hookedGameModeAttack(__int64 a1, __int64 a2, char a3) {
    std::cout << "GameMode::attack called!" << std::endl;
    
    if (g_ActorGetNameTag) {
        void** nameTag = g_ActorGetNameTag(a2);
        
        if (nameTag) {
            std::string actorName = extractNameSafely(nameTag);
            
            if (!actorName.empty()) {
                std::string sanitizedName = sanitizeName(actorName);
                std::cout << "Actor name: " << actorName << std::endl;
                std::cout << "Sanitized: " << sanitizedName << std::endl;
            } else {
                std::cout << "Actor name: (empty or invalid)" << std::endl;
            }
        } else {
            std::cout << "Failed to get actor name tag" << std::endl;
        }
    }
    
    return g_OriginalGameModeAttack(a1, a2, a3);
}

void createConsole() {
    AllocConsole();
    freopen_s(&g_Console, "CONOUT$", "w", stdout);
    freopen_s(&g_Console, "CONIN$", "r", stdin);
    SetConsoleTitleA("Spwai - Signature Scanner");
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
                std::cout << "GameMode::attack hook enabled" << std::endl;
            } else {
                std::cout << "Failed to enable GameMode::attack hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create GameMode::attack hook" << std::endl;
        }
    }
}

void initialize() {
    createConsole();
    scanSignatures();
    initializeHooks();
    
    std::cout << "Initialization complete. Press any key to continue..." << std::endl;
    std::cin.get();
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