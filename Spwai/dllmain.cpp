#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <MinHook.h>
#include <libhat.hpp>
#include "Utils/Utils.hpp"
#include "SDK/Minecraft/NametagEntry.hpp"
#include "SDK/Minecraft/MinecraftUIRenderContext.hpp"
#include "SDK/Signature.hpp"

typedef unsigned __int64 QWORD;

FILE* g_Console = nullptr;
HMODULE g_Module = nullptr;

using GameModeAttack = __int64(__fastcall*)(__int64 gamemode, __int64 actor, char a3);
using ActorGetNameTag = void** (__fastcall*)(__int64 actor);
using MouseDeviceFeed = __int64(__fastcall*)(__int64 mouseDevice, char button, char action, __int16 mouseX, __int16 mouseY, __int16 movementX, __int16 movementY, char a8);
using NametagObject = QWORD*(__fastcall*)(__int64 a1, QWORD* a2, __int64 a3);
using MinecraftUIRenderContextFunc = __int64(__fastcall*)(__int64 contextPtr, __int64 renderData);

GameModeAttack g_GameModeAttack = nullptr;
ActorGetNameTag g_ActorGetNameTag = nullptr;
GameModeAttack g_OriginalGameModeAttack = nullptr;

MouseDeviceFeed g_MouseDeviceFeed = nullptr;
MouseDeviceFeed g_OriginalMouseDeviceFeed = nullptr;

NametagObject g_NametagObject = nullptr;
NametagObject g_OriginalNametagObject = nullptr;

MinecraftUIRenderContextFunc g_MinecraftUIRenderContext = nullptr;
MinecraftUIRenderContextFunc g_OriginalMinecraftUIRenderContext = nullptr;

uintptr_t g_RenderCtxCallAddr = 0;
unsigned char g_RenderCtxOriginalCallBytes[5] = {0};

std::vector<std::string> g_msrPlayers;
std::vector<std::string> g_qtPlayers;

int g_PlayersOnScreen = 0;
int g_FriendAttacksBlocked = 0;

bool g_RightMouseButtonHeld = false;
bool g_LeftMouseButtonHeld = false;
bool g_NametagColorsEnabled = true;
ULONGLONG g_LastMmbClickMs = 0;
bool g_MiddleMouseButtonHeld = false;

int g_UITextMode = 0;
std::string g_LastHitRawName;
std::string g_LastHitSanitizedName;
__int64 g_LastHitEntityPtr = 0;
int g_MsrOnScreen = 0;
int g_QtOnScreen = 0;
float g_MousePosX = 0.0f;
float g_MousePosY = 0.0f;

__int64 __fastcall hookedGameModeAttack(__int64 gamemode, __int64 actor, char a3) {
    g_LastHitEntityPtr = actor;
    if (g_ActorGetNameTag) {
        void** nameTag = g_ActorGetNameTag(actor);
        if (nameTag) {
            std::string actorName = extractName(nameTag);
            g_LastHitRawName = actorName;
            g_LastHitSanitizedName = actorName.empty() ? std::string("") : sanitizeName(actorName);
        }
    }
    
    if (g_MiddleMouseButtonHeld && g_ActorGetNameTag) {
        void** nameTag = g_ActorGetNameTag(actor);
        if (nameTag) {
            std::string actorName = extractName(nameTag);
            if (!actorName.empty()) {
                std::string sanitizedName = sanitizeName(actorName);
                if (!sanitizedName.empty()) {
                    if (g_RightMouseButtonHeld && g_LeftMouseButtonHeld) {
                        if (isInList(sanitizedName, g_qtPlayers)) {
                            g_qtPlayers.erase(std::remove(g_qtPlayers.begin(), g_qtPlayers.end(), sanitizedName), g_qtPlayers.end());
                        } else {
                            g_msrPlayers.erase(std::remove(g_msrPlayers.begin(), g_msrPlayers.end(), sanitizedName), g_msrPlayers.end());
                            g_qtPlayers.push_back(sanitizedName);
                        }
                    } else if (!g_RightMouseButtonHeld) {
                        if (isInList(sanitizedName, g_msrPlayers)) {
                            g_msrPlayers.erase(std::remove(g_msrPlayers.begin(), g_msrPlayers.end(), sanitizedName), g_msrPlayers.end());
                        } else {
                            g_qtPlayers.erase(std::remove(g_qtPlayers.begin(), g_qtPlayers.end(), sanitizedName), g_msrPlayers.end());
                            g_msrPlayers.push_back(sanitizedName);
                        }
                    }
                }
            }
        }
        return 0;
    }

    if (g_RightMouseButtonHeld) {
        return g_OriginalGameModeAttack(gamemode, actor, a3);
    }
    
    if (g_ActorGetNameTag) {
        void** nameTag = g_ActorGetNameTag(actor);
        if (nameTag) {
            std::string actorName = extractName(nameTag);
            if (!actorName.empty()) {
                std::string sanitizedName = sanitizeName(actorName);
                if (isInList(sanitizedName, g_msrPlayers)) {
                    g_FriendAttacksBlocked++;
                    return 0;
                }
            }
        }
    }
    
    return g_OriginalGameModeAttack(gamemode, actor, a3);
}

__int64 __fastcall hookedMouseDeviceFeed(__int64 mouseDevice, char button, char action, __int16 mouseX, __int16 mouseY, __int16 movementX, __int16 movementY, char a8) {
    g_MousePosX = static_cast<float>(mouseX);
    g_MousePosY = static_cast<float>(mouseY);

    if (button == 1) {
        if (action == 1) {
            g_LeftMouseButtonHeld = true;
        } else if (action == 0) {
            g_LeftMouseButtonHeld = false;
        }
    }
    
    if (button == 2) {
        if (action == 1) {
            g_RightMouseButtonHeld = true;
        } else if (action == 0) {
            g_RightMouseButtonHeld = false;
        }
    }

    if (button == 3) {
        if (action == 1) {
            g_MiddleMouseButtonHeld = true;
            ULONGLONG now = GetTickCount64();
            if (now - g_LastMmbClickMs <= 500) {
                g_NametagColorsEnabled = !g_NametagColorsEnabled;
                g_LastMmbClickMs = 0;
            } else {
                g_LastMmbClickMs = now;
            }
        } else if (action == 0) {
            g_MiddleMouseButtonHeld = false;
        }
    }

    if (button == 4 || button == 5 || button == 6 || button == 7) {
        int delta = 0;
        if (action > 0) delta = 1; else if (action < 0) delta = -1;
        if (g_RightMouseButtonHeld) {
            if (delta < 0) {
                g_UITextMode = (g_UITextMode + 1) % 4;
            } else if (delta > 0) {
                g_UITextMode = (g_UITextMode + 3) % 4;
            }
        }
    }
    
    return g_OriginalMouseDeviceFeed(mouseDevice, button, action, mouseX, mouseY, movementX, movementY, a8);
}

QWORD* __fastcall hookedNametagObject(__int64 a1, QWORD* array, __int64 a3) {
    QWORD* result = g_OriginalNametagObject(a1, array, a3);
    
    g_PlayersOnScreen = 0;
    g_MsrOnScreen = 0;
    g_QtOnScreen = 0;
    
    if (array && array[2] && array[3]) {
        __int64 startAddr = array[2];
        __int64 endAddr = array[3];
        
        if (startAddr && endAddr && startAddr < endAddr) {
            __int64 numNametags = (endAddr - startAddr) / sizeof(::NametagEntry);
            g_PlayersOnScreen = static_cast<int>(numNametags);
            
            for (__int64 i = 0; i < numNametags; i++) {
                ::NametagEntry* nametag = reinterpret_cast<::NametagEntry*>(startAddr + (i * sizeof(::NametagEntry)));
                
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

                            if (!g_NametagColorsEnabled) {
                                continue;
                            }
                            
                            if (isSpecialName(sanitizedName)) {
                                ULONGLONG nowMs = GetTickCount64();
                                float t = static_cast<float>(nowMs % 1000ULL) / 1000.0f;
                                float tri = (t < 0.5f) ? (t * 2.0f) : (1.0f - (t - 0.5f) * 2.0f);
                                const float baseR = 0.95f, baseG = 0.3f, baseB = 0.65f, baseA = 0.3f;
                                nametag->bgColor.r = baseR + (1.0f - baseR) * tri;
                                nametag->bgColor.g = baseG + (1.0f - baseG) * tri;
                                nametag->bgColor.b = baseB + (1.0f - baseB) * tri;
                                nametag->bgColor.a = baseA;
                            } else if (g_RightMouseButtonHeld) {
                                if (isInList(sanitizedName, g_msrPlayers)) {
                                    nametag->bgColor.r = 0.5f;
                                    nametag->bgColor.g = 1.0f;
                                    nametag->bgColor.b = 0.85f;
                                    nametag->bgColor.a = 0.25f;
                                    g_MsrOnScreen++;
                                } else if (isInList(sanitizedName, g_qtPlayers)) {
                                    nametag->bgColor.r = 1.0f;
                                    nametag->bgColor.g = 0.0f;
                                    nametag->bgColor.b = 0.0f;
                                    nametag->bgColor.a = 0.3f;
                                    g_QtOnScreen++;
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
                                    g_MsrOnScreen++;
                                } else if (isInList(sanitizedName, g_qtPlayers)) {
                                    nametag->bgColor.r = 1.0f;
                                    nametag->bgColor.g = 0.0f;
                                    nametag->bgColor.b = 0.0f;
                                    nametag->bgColor.a = 0.3f;
                                    g_QtOnScreen++;
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

__int64 __fastcall hookedMinecraftUIRenderContext(__int64 a1, __int64 a2) {
    if (a2) {
        MinecraftUIRenderContext* context = reinterpret_cast<MinecraftUIRenderContext*>(a2);

        RectangleArea fullRect = context->getFullClippingRectangle();

        RectangleArea textRect(2.0f, 2.0f, fullRect.right - 2.0f, 120.0f);
        TextMeasureData textMeasure(1.0f, true, false);
        CaretMeasureData caretMeasure;
        
        if (g_UITextMode == 0) {
            std::string infoText =
                "MMB + LMB -> Add MSR\n"
                "MMB + RMB + LMB -> Add QT\n"
                "MMB X2 -> Toggle Rendering\n"
                "RMB + Scroll Wheel -> Cycle Menu (Info/Stats/LastHit/Off)";
            context->drawDebugText(
                textRect,
                infoText,
                Color(1.0f, 1.0f, 1.0f, 1.0f),
                0.6f,
                TextAlignment::LEFT,
                textMeasure,
                caretMeasure
            );
        } else if (g_UITextMode == 1) {
            std::string statsText =
                std::to_string(g_msrPlayers.size()) + " MSR " + std::to_string(g_qtPlayers.size()) + " QT Loaded\n" +
                std::to_string(g_PlayersOnScreen) + " Player " + std::to_string(g_MsrOnScreen) + " MSR " + std::to_string(g_QtOnScreen) + " QT\n" +
                std::to_string(g_FriendAttacksBlocked) + " Hits Blocked";
              
            context->drawDebugText(
                textRect,
                statsText,
                Color(1.0f, 1.0f, 1.0f, 1.0f),
                0.6f,
                TextAlignment::LEFT,
                textMeasure,
                caretMeasure
            );
        } else if (g_UITextMode == 2) {
            std::ostringstream oss;
            oss << "rawName: " << (g_LastHitRawName.empty() ? std::string("<none>") : g_LastHitRawName) << "§r\n";
            oss << "name: " << (g_LastHitSanitizedName.empty() ? std::string("<none>") : g_LastHitSanitizedName) << "\n";
            {
                uintptr_t addr = static_cast<uintptr_t>(g_LastHitEntityPtr);
                oss << "entityPtr: 0x" << std::uppercase << std::hex
                    << std::setw(sizeof(void*) * 2) << std::setfill('0') << addr;
            }
            context->drawDebugText(
                textRect,
                oss.str(),
                Color(1.0f, 1.0f, 1.0f, 1.0f),
                0.6f,
                TextAlignment::LEFT,
                textMeasure,
                caretMeasure
            );
        } else if (g_UITextMode == 3) {
        }   
    }

    return g_OriginalMinecraftUIRenderContext(a1, a2);
}

void createConsole() {
    AllocConsole();
    freopen_s(&g_Console, "CONOUT$", "w", stdout);
    freopen_s(&g_Console, "CONIN$", "r", stdin);
    SetConsoleTitleA("Spwai");
}

void initializeHooks() {
    if (MH_Initialize() != MH_OK) {
        std::cout << "Failed to initialize MinHook!" << std::endl;
        return;
    }
    
    if (g_GameModeAttack) {
        if (MH_CreateHook(g_GameModeAttack, &hookedGameModeAttack, reinterpret_cast<LPVOID*>(&g_OriginalGameModeAttack)) == MH_OK) {
            if (MH_EnableHook(g_GameModeAttack) != MH_OK) {
                std::cout << "Failed to enable GameMode::attack hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create GameMode::attack hook" << std::endl;
        }
    }
    
    if (g_MouseDeviceFeed) {
        if (MH_CreateHook(g_MouseDeviceFeed, &hookedMouseDeviceFeed, reinterpret_cast<LPVOID*>(&g_OriginalMouseDeviceFeed)) == MH_OK) {
            if (MH_EnableHook(g_MouseDeviceFeed) != MH_OK) {
                std::cout << "Failed to enable MouseDevice::feed hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create MouseDevice::feed hook" << std::endl;
        }
    }
    
    if (g_NametagObject) {
        if (MH_CreateHook(g_NametagObject, &hookedNametagObject, reinterpret_cast<LPVOID*>(&g_OriginalNametagObject)) == MH_OK) {
            if (MH_EnableHook(g_NametagObject) != MH_OK) {
                std::cout << "Failed to enable NametagObject hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create NametagObject hook" << std::endl;
            }
    }

    if (g_MinecraftUIRenderContext) {
        if (MH_CreateHook(g_MinecraftUIRenderContext, &hookedMinecraftUIRenderContext, reinterpret_cast<LPVOID*>(&g_OriginalMinecraftUIRenderContext)) == MH_OK) {
            if (MH_EnableHook(g_MinecraftUIRenderContext) != MH_OK) {
                std::cout << "Failed to enable MinecraftUIRenderContext hook" << std::endl;
            }
        } else {
            std::cout << "Failed to create MinecraftUIRenderContext hook" << std::endl;
        }
    }
}

void initialize() {
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