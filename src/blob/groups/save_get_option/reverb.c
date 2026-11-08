/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * GetPlayerOption (historical label): read one signed option byte of a save slot.
 *   GetPlayerOption(owner, selector): selectors 0..20 are the common bytes at fields+0x20; the rest depend on the
 *   current page gTrackNum: pages 0..5 and 19..24 -> GetRaceSetting (bytes 0x50..0x59), 6..13 -> 0x64..0x6B,
 *   14..17 -> 0x74..0x7C, any other page -> 0x88..0x8B.  audio_bus_mix (0x800B4818) is the matching setter.
 * Whole-program structure this depends on (it cannot match alone):
 *   - the three non-modeA getters are separate functions returning s8 that umerge inlines here; they are the
 *     caller-less `jr ra; nop` stubs GetBattleSetting / GetStuntSetting / GetObstacleSetting that sit before
 *     GetRaceSetting (which of the three stubs is which getter is an assumption; the setter family has the same
 *     three stubs at 0x800B4720..0x800B4730).  The s8 -> s32 conversion of the inlined result is what leaves
 *     the callee-exit label between `move v0,zero` and the jump, so as1 emits `move v0,zero; b; lw ra`
 *     for the two table defaults (the only such sequence in the image);
 *   - GetRaceSetting takes a third, unused parameter (the page): internal, so the caller stores it to the a2 home
 *     slot (`sw v0,8(sp)`) instead of loading a2.  Its body is unchanged (37 words).
 * Own rodata: three jump tables (0x80123CE4 GetRaceSetting, 0x80123D10 and 0x80123D58 GetPlayerOption).
 * No arcade ancestor (N64 options/save code).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef int s32;
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
extern s8 gTrackNum;

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
