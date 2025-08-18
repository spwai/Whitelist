#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct NametagEntry {
    union {
        char name[16];
        const char* namePtr;
    };
    uint8_t padding1[0x20];
    void* entityTypeData;
    void* secondaryData;
    struct { float r, g, b, a; } bgColor;
    struct { float r, g, b, a; } textColor;
    float posX;
    float posY;
    float posZ;
    uint8_t padding2[4];
};
#pragma pack(pop)
static_assert(sizeof(NametagEntry) == 0x70, "NametagEntry must be 0x70 bytes!");