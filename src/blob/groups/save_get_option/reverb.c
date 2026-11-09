#include "game/game.h"

typedef struct Fields {
    u8 opaque0[32];
    /* 0x20 */ s8 normal[21];
    u8 opaque53[27];
    /* 0x50 */ s8 modeA[10];
    u8 opaque90[10];
    /* 0x64 */ s8 primary[8];
    u8 opaque108[8];
    /* 0x74 */ s8 secondary[9];
    u8 opaque125[11];
    /* 0x88 */ s8 fallback[4];
} Fields;
typedef struct Resource {Fields *fields;} Resource;
typedef struct Object {void *next; void *prev; void *fileh; u8 opaque0_C[0x2C - 0xC];Resource *resource;} Object;
typedef struct Owner {Object *object;} Owner;

s8 GetBattleSetting(Owner *owner, u8 selector) {
    Fields *fields = owner->object->resource->fields;

    switch (selector) {
    case 23: return fields->primary[0];
    case 28: return fields->primary[1];
    case 29: return fields->primary[2];
    case 30: return fields->primary[3];
    case 37: return fields->primary[4];
    case 38: return fields->primary[5];
    case 39: return fields->primary[6];
    case 40: return fields->primary[7];
    default: return 0;
    }
}

s8 GetStuntSetting(Owner *owner, u8 selector) {
    Fields *fields = owner->object->resource->fields;

    switch (selector) {
    case 23: return fields->secondary[0];
    case 28: return fields->secondary[1];
    case 29: return fields->secondary[2];
    case 30: return fields->secondary[3];
    case 32: return fields->secondary[4];
    case 33: return fields->secondary[5];
    case 34: return fields->secondary[6];
    case 35: return fields->secondary[7];
    case 36: return fields->secondary[8];
    default: return 0;
    }
}

s8 GetObstacleSetting(Owner *owner, u8 selector) {
    Fields *fields = owner->object->resource->fields;

    switch (selector) {
    case 23: return fields->fallback[0];
    case 28: return fields->fallback[1];
    case 29: return fields->fallback[2];
    case 30: return fields->fallback[3];
    default: return 0;
    }
}

s8 GetRaceSetting(Owner *owner, u8 selector, s32 mode) {
    Fields *fields = owner->object->resource->fields;

    switch (selector) {
    case 21: return fields->modeA[0];
    case 22: return fields->modeA[1];
    case 23: return fields->modeA[2];
    case 24: return fields->modeA[3];
    case 26: return fields->modeA[4];
    case 27: return fields->modeA[5];
    case 28: return fields->modeA[6];
    case 29: return fields->modeA[7];
    case 30: return fields->modeA[8];
    case 31: return fields->modeA[9];
    default: return 0;
    }
}

s32 GetPlayerOption(Owner *owner, u8 selector) {
    s32 mode;

    if (selector < 21) {
        return owner->object->resource->fields->normal[selector];
    }
    mode = gTrackNum;
    if (mode >= 0 && mode < 6) {
        return GetRaceSetting(owner, selector, mode);
    }
    if (mode >= 19 && mode < 25) {
        return GetRaceSetting(owner, selector, mode);
    }
    if (mode >= 6 && mode < 14) {
        return GetBattleSetting(owner, selector);
    }
    if (mode >= 14 && mode < 18) {
        return GetStuntSetting(owner, selector);
    }
    return GetObstacleSetting(owner, selector);
}
