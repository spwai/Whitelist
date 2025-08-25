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
#include "SDK/NametagEntry.hpp"
#include "SDK/MinecraftUIRenderContext.hpp"

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
MinecraftUIRenderContextFunc g_RenderCtxOriginalTarget = nullptr;

std::vector<std::string> g_msrPlayers;
std::vector<std::string> g_qtPlayers;

int g_PlayersOnScreen = 0;
int g_FriendAttacksBlocked = 0;

bool g_RightMouseButtonHeld = false;
bool g_LeftMouseButtonHeld = false;
bool g_NametagColorsEnabled = true;
ULONGLONG g_LastMmbClickMs = 0;
bool g_MiddleMouseButtonHeld = false;

RectangleArea g_UIButtonRect(0.0f, 0.0f, 0.0f, 0.0f);
RectangleArea g_TextRectCached(0.0f, 0.0f, 0.0f, 0.0f);
int g_UITextMode = 0; // 0: Info, 1: Stats, 2: Details
int g_MsrOnScreen = 0;
int g_QtOnScreen = 0;
float g_MousePosX = 0.0f;
float g_MousePosY = 0.0f;
float g_UIMousePosX = 0.0f;
float g_UIMousePosY = 0.0f;
RectangleArea g_LastFullRect(0.0f, 0.0f, 0.0f, 0.0f);
const ULONGLONG g_CacheIntervalMs = 10000ULL;
ULONGLONG g_LastCacheMs = 0ULL;
int g_LastEntryArrayCount = 0;

struct CachedNametagInfo {
    std::string name;
    std::string sanitizedName;
    void* entityTypeData;
    void* secondaryData;
    float bgR, bgG, bgB, bgA;
    float textR, textG, textB, textA;
    float posX, posY, posZ;
};

std::vector<CachedNametagInfo> g_CachedNametags;
int g_CurrentNametagIndex = 0;


__int64 __fastcall hookedGameModeAttack(__int64 gamemode, __int64 actor, char a3) {
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
                            g_qtPlayers.erase(std::remove(g_qtPlayers.begin(), g_qtPlayers.end(), sanitizedName), g_qtPlayers.end());
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
            // Treat left-button release as click; toggle if released over text or square
            float mappedX = g_MousePosX;
            float mappedY = g_MousePosY;
            mapMouseToUI(g_MousePosX, g_MousePosY, g_LastFullRect, mappedX, mappedY);
            if (pointInRect(mappedX, mappedY, g_UIButtonRect) || pointInRect(mappedX, mappedY, g_TextRectCached)) {
                g_UITextMode = (g_UITextMode + 1) % 3;
            }
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
        if (delta != 0 && g_UITextMode == 2 && !g_CachedNametags.empty()) {
            g_CurrentNametagIndex = (g_CurrentNametagIndex + delta) % static_cast<int>(g_CachedNametags.size());
            if (g_CurrentNametagIndex < 0) g_CurrentNametagIndex += static_cast<int>(g_CachedNametags.size());
        }
    }

    if (movementY != 0) {
        if (g_UITextMode == 2 && !g_CachedNametags.empty()) {
            int16_t m = movementY;
            if (m >= 60 || m <= -60) {
                int delta = (m > 0) ? 1 : -1;
                g_CurrentNametagIndex = (g_CurrentNametagIndex + delta) % static_cast<int>(g_CachedNametags.size());
                if (g_CurrentNametagIndex < 0) g_CurrentNametagIndex += static_cast<int>(g_CachedNametags.size());
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
    // Append-only caching window, clear every g_CacheIntervalMs
    ULONGLONG nowMs = GetTickCount64();
    if (g_LastCacheMs == 0ULL) g_LastCacheMs = nowMs;
    ULONGLONG elapsed = nowMs - g_LastCacheMs;
    if (elapsed >= g_CacheIntervalMs) {
        g_CachedNametags.clear();
        g_LastCacheMs = nowMs;
    }
    
    if (array && array[2] && array[3]) {
        __int64 startAddr = array[2];
        __int64 endAddr = array[3];
        
        if (startAddr && endAddr && startAddr < endAddr) {
            __int64 numNametags = (endAddr - startAddr) / sizeof(::NametagEntry);
            g_PlayersOnScreen = static_cast<int>(numNametags);
            g_LastEntryArrayCount = static_cast<int>(numNametags);
            
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

                            // Cache basic details for details view
                            CachedNametagInfo cached{};
                            cached.name = actorName;
                            cached.sanitizedName = sanitizedName;
                            cached.entityTypeData = nametag->entityTypeData;
                            cached.secondaryData = nametag->secondaryData;
                            cached.bgR = nametag->bgColor.r;
                            cached.bgG = nametag->bgColor.g;
                            cached.bgB = nametag->bgColor.b;
                            cached.bgA = nametag->bgColor.a;
                            cached.textR = nametag->textColor.r;
                            cached.textG = nametag->textColor.g;
                            cached.textB = nametag->textColor.b;
                            cached.textA = nametag->textColor.a;
                            cached.posX = nametag->posX;
                            cached.posY = nametag->posY;
                            cached.posZ = nametag->posZ;
                            // Append-only: if exists, refresh dynamic fields in-place; do not remove
                            bool found = false;
                            for (auto& e : g_CachedNametags) {
                                if (e.entityTypeData == cached.entityTypeData && e.secondaryData == cached.secondaryData && e.sanitizedName == cached.sanitizedName) {
                                    // Update dynamic values so Details view stays current without reordering
                                    e.bgR = cached.bgR; e.bgG = cached.bgG; e.bgB = cached.bgB; e.bgA = cached.bgA;
                                    e.textR = cached.textR; e.textG = cached.textG; e.textB = cached.textB; e.textA = cached.textA;
                                    e.posX = cached.posX; e.posY = cached.posY; e.posZ = cached.posZ;
                                    e.name = cached.name; // Raw name could change color codes
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                g_CachedNametags.push_back(cached);
                            }
                            
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

                            if (g_UITextMode == 2 && !g_CachedNametags.empty()) {
                                int idx = g_CurrentNametagIndex;
                                if (idx < 0) idx = 0;
                                if (idx >= static_cast<int>(g_CachedNametags.size())) idx = static_cast<int>(g_CachedNametags.size()) - 1;
                                const CachedNametagInfo& sel = g_CachedNametags[idx];
                                if (sel.entityTypeData == nametag->entityTypeData && sel.secondaryData == nametag->secondaryData && sel.sanitizedName == sanitizedName) {
                                    nametag->bgColor.r = 1.0f;
                                    nametag->bgColor.g = 0.6f;
                                    nametag->bgColor.b = 0.0f;
                                    nametag->bgColor.a = 0.6f;
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

__int64 __fastcall wrappedMinecraftUIRenderContext(__int64 a1, __int64 a2) {
    if (a2) {
        MinecraftUIRenderContext* context = reinterpret_cast<MinecraftUIRenderContext*>(a2);

        RectangleArea fullRect = context->getFullClippingRectangle();
        float fullWidth = fullRect.right - fullRect.left;
        float fullHeight = fullRect.bottom - fullRect.top;

        if (fullWidth < 10.0f || fullHeight < 10.0f) {
            std::cout << "Full clipping rectangle invalid (" << fullRect.left << "," << fullRect.top
                << " -> " << fullRect.right << "," << fullRect.bottom << "), applying fallback 1920x1080" << std::endl;
            fullRect = RectangleArea(0.0f, 0.0f, 1920.0f, 1080.0f);
            fullWidth = 1920.0f;
            fullHeight = 1080.0f;
        }

        g_LastFullRect = fullRect;

        float uiMouseX = 0.0f, uiMouseY = 0.0f;
        mapMouseToUI(g_MousePosX, g_MousePosY, fullRect, uiMouseX, uiMouseY);
        g_UIMousePosX = uiMouseX;
        g_UIMousePosY = uiMouseY;

        RectangleArea textRect(2.0f, 2.0f, fullRect.right - 2.0f, 140.0f);
        TextMeasureData textMeasure(1.0f, true, false);
        CaretMeasureData caretMeasure;
        if (g_UITextMode == 0) {
            std::string infoText =
                "=== Info ===\n"
                "MMB + LMB             - Add MSR\n"
                "MMB + RMB + LMB    - Add QT\n"
                "MMB X2                  - Toggle Rendering\n"
                "Click to cycle menu";
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
            if (!g_CachedNametags.empty()) {
                if (g_CurrentNametagIndex >= static_cast<int>(g_CachedNametags.size())) {
                    g_CurrentNametagIndex = static_cast<int>(g_CachedNametags.size()) - 1;
                }
                if (g_CurrentNametagIndex < 0) {
                    g_CurrentNametagIndex = 0;
                }
                // Live refresh: update dynamic fields from latest nametag memory if still valid
                CachedNametagInfo c = g_CachedNametags[g_CurrentNametagIndex];
                // Search for a matching entry by pointer pair to refresh bg/text colors and pos
                // Note: if layout changes, this remains a best-effort refresh
                for (const CachedNametagInfo& latest : g_CachedNametags) {
                    if (latest.entityTypeData == c.entityTypeData && latest.secondaryData == c.secondaryData && latest.sanitizedName == c.sanitizedName) {
                        c.bgR = latest.bgR; c.bgG = latest.bgG; c.bgB = latest.bgB; c.bgA = latest.bgA;
                        c.textR = latest.textR; c.textG = latest.textG; c.textB = latest.textB; c.textA = latest.textA;
                        c.posX = latest.posX; c.posY = latest.posY; c.posZ = latest.posZ;
                        break;
                    }
                }
                std::ostringstream oss;
                oss.setf(std::ios::fixed); oss << std::setprecision(2);
                // Prevent RawName line wrapping by widening the rect below and keeping RawName first on its own line
                oss << "RawName: " << c.name << "§r\n";
                oss << "SanitizedName: " << c.sanitizedName << "\n";
                oss << "EntityData: " << c.entityTypeData << "\n";
                oss << "EntityData2: " << c.secondaryData << "\n";
                oss << "BgColor: " << c.bgR << ", " << c.bgG << ", " << c.bgB << ", " << c.bgA << "\n";
                oss << "TextColor: " << c.textR << ", " << c.textG << ", " << c.textB << ", " << c.textA << "\n";
                oss << "Pos: " << c.posX << ", " << c.posY << ", " << c.posZ << "\n";
                // Extra: entry array count and time until next cache clear
                {
                    ULONGLONG nowMsUI = GetTickCount64();
                    ULONGLONG elapsedMs = (nowMsUI >= g_LastCacheMs) ? (nowMsUI - g_LastCacheMs) : 0ULL;
                    int secondsLeft = static_cast<int>((g_CacheIntervalMs - (elapsedMs % g_CacheIntervalMs)) / 1000ULL);
                    oss << "Entries: " << g_LastEntryArrayCount << "\n";
                    oss << "Next cache clear: " << secondsLeft << "/" << (g_CacheIntervalMs / 1000ULL) << "\n";
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
            } else {
                context->drawDebugText(
                    textRect,
                    std::string("No Nametags"),
                    Color(1.0f, 1.0f, 1.0f, 1.0f),
                    0.6f,
                    TextAlignment::LEFT,
                    textMeasure,
                    caretMeasure
                );
            }
        }
        

        g_TextRectCached = textRect;

        float txtH = textRect.bottom - textRect.top;
        float sqSize = 18.0f;
        float margin = 8.0f;
        float sqLeft = textRect.right + margin;
        float sqTop = textRect.top + (txtH - sqSize) * 0.5f;
        g_UIButtonRect = RectangleArea(sqLeft, sqTop, sqLeft + sqSize, sqTop + sqSize);

        bool hovered = pointInRect(uiMouseX, uiMouseY, g_UIButtonRect);
        Color sqFill = hovered ? Color(1.0f, 1.0f, 1.0f, 1.0f) : Color(1.0f, 1.0f, 1.0f, 0.95f);
        context->fillRectangle(g_UIButtonRect, sqFill, 0.0f);
        context->drawRectangle(g_UIButtonRect, Color(0.0f, 0.0f, 0.0f, hovered ? 1.0f : 0.8f), 1.0f, 0);

        
    }

    __int64 result = 0;
    if (g_RenderCtxOriginalTarget) {
        result = g_RenderCtxOriginalTarget(a1, a2);
    }
    return result;
}

static bool writeRelativeCall(uintptr_t callAddr, void* newTarget) {
    DWORD oldProt;
    if (!VirtualProtect(reinterpret_cast<void*>(callAddr), 5, PAGE_EXECUTE_READWRITE, &oldProt)) return false;
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(newTarget) - (callAddr + 5));
    unsigned char bytes[5] = { 0xE8, 0, 0, 0, 0 };
    memcpy(&bytes[1], &rel, sizeof(rel));
    memcpy(reinterpret_cast<void*>(callAddr), bytes, 5);
    DWORD tmp; VirtualProtect(reinterpret_cast<void*>(callAddr), 5, oldProt, &tmp);
    return true;
}

static void restoreOriginalCall(uintptr_t callAddr, const unsigned char original[5]) {
    DWORD oldProt;
    if (!VirtualProtect(reinterpret_cast<void*>(callAddr), 5, PAGE_EXECUTE_READWRITE, &oldProt)) return;
    memcpy(reinterpret_cast<void*>(callAddr), original, 5);
    DWORD tmp; VirtualProtect(reinterpret_cast<void*>(callAddr), 5, oldProt, &tmp);
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
            g_GameModeAttack = reinterpret_cast<GameModeAttack>(attackResult.get());
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
            g_ActorGetNameTag = reinterpret_cast<ActorGetNameTag>(getNameTagResult.get());
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
            g_MouseDeviceFeed = reinterpret_cast<MouseDeviceFeed>(mouseDeviceFeedResult.get());
            g_OriginalMouseDeviceFeed = g_MouseDeviceFeed;
            std::cout << "MouseDevice::feed found" << std::endl;
        } else {
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
        } else {
            std::cout << "NametagObject not found" << std::endl;
        }
    }
    
    {
        std::string_view callsiteSig = "E8 ?? ?? ?? ?? 48 8B 44 24 50 48 8D 4C 24 50 48 8B 80 D8 00 00 00";
        auto sigParsed = hat::parse_signature(callsiteSig);
        if (sigParsed.has_value()) {
            auto sigResult = hat::find_pattern(sigParsed.value(), ".text");
            if (sigResult.has_result()) {
                g_RenderCtxCallAddr = reinterpret_cast<uintptr_t>(sigResult.get());
                memcpy(g_RenderCtxOriginalCallBytes, reinterpret_cast<void*>(g_RenderCtxCallAddr), 5);
                uintptr_t rel = *reinterpret_cast<int32_t*>(g_RenderCtxCallAddr + 1);
                uintptr_t target = g_RenderCtxCallAddr + 5 + rel;
                g_RenderCtxOriginalTarget = reinterpret_cast<MinecraftUIRenderContextFunc>(target);
                std::cout << "MinecraftUIRenderContext found" << std::endl;
            } else {
                std::cout << "MinecraftUIRenderContext not found" << std::endl;
            }
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

    if (g_RenderCtxCallAddr && g_RenderCtxOriginalTarget) {
        if (!writeRelativeCall(g_RenderCtxCallAddr, reinterpret_cast<void*>(&wrappedMinecraftUIRenderContext))) {
            std::cout << "Failed to patch UI callsite" << std::endl;
        } else {
            std::cout << "UI callsite patched to wrapper" << std::endl;
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
    
    if (g_RenderCtxCallAddr) {
        restoreOriginalCall(g_RenderCtxCallAddr, g_RenderCtxOriginalCallBytes);
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