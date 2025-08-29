#pragma once

class EntityContext;

class Facing {
public:
    enum class Name : unsigned char {
        DOWN,
        UP,
        NORTH,
        SOUTH,
        WEST,
        EAST,
        MAX
    };
};

using FacingID = Facing::Name;

class Vec3 {
public:
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

class BlockPos {
public:
    int x, y, z;
    
    BlockPos() : x(0), y(0), z(0) {}
    BlockPos(int x, int y, int z) : x(x), y(y), z(z) {}
};

using WeakEntityRef = __int64;

enum class HitResultType : int {
    TILE = 0x0,
    ENTITY = 0x1,
    ENTITY_OUT_OF_RANGE = 0x2,
    NO_HIT = 0x3,
};

class HitResult {
public:
    Vec3 mStartPos;
    Vec3 mRayDir;
    HitResultType mType;
    FacingID mFacing;
    BlockPos mBlock;
    Vec3 mPos;
    WeakEntityRef mEntity;
    bool mIsHitLiquid;
    FacingID mLiquidFacing;
    BlockPos mLiquid;
    Vec3 mLiquidPos;
    bool mIndirectHit;
};