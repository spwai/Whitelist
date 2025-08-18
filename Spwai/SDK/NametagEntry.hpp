#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct NametagEntry {
    union {
        char name[16];
        const char* namePtr;
    };
    uint8_t padding1[0x20];
    void* entityTypeData;      // Offset 0x30 (48)
    void* secondaryData;       // Offset 0x38 (56)
    struct { float r, g, b, a; } bgColor;    // Offset 0x40 (64) - 16 bytes
    struct { float r, g, b, a; } textColor;  // Offset 0x50 (80) - 16 bytes
    float posX;                // Offset 0x60 (96)
    float posY;                // Offset 0x64 (100)
    float posZ;                // Offset 0x68 (104)
    uint8_t padding2[4];       // Offset 0x6C (108)
};
#pragma pack(pop)
static_assert(sizeof(NametagEntry) == 0x70, "NametagEntry must be 0x70 bytes!");