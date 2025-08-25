#include "Signature.hpp"
#include <iostream>
#include <libhat.hpp>
#include <Windows.h>

typedef unsigned __int64 QWORD;

using GameModeAttack = __int64(__fastcall*)(__int64 gamemode, __int64 actor, char a3);
using ActorGetNameTag = void** (__fastcall*)(__int64 actor);
using MouseDeviceFeed = __int64(__fastcall*)(__int64 mouseDevice, char button, char action, __int16 mouseX, __int16 mouseY, __int16 movementX, __int16 movementY, char a8);
using NametagObject = QWORD*(__fastcall*)(__int64 a1, QWORD* a2, __int64 a3);
using MinecraftUIRenderContextFunc = __int64(__fastcall*)(__int64 contextPtr, __int64 renderData);

extern GameModeAttack g_GameModeAttack;
extern ActorGetNameTag g_ActorGetNameTag;
extern GameModeAttack g_OriginalGameModeAttack;
extern MouseDeviceFeed g_MouseDeviceFeed;
extern MouseDeviceFeed g_OriginalMouseDeviceFeed;
extern NametagObject g_NametagObject;
extern NametagObject g_OriginalNametagObject;
extern uintptr_t g_RenderCtxCallAddr;
extern unsigned char g_RenderCtxOriginalCallBytes[5];
extern MinecraftUIRenderContextFunc g_RenderCtxOriginalTarget;

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
