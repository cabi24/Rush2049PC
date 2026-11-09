#include "game/game.h"

typedef struct Sprite {
    /* 0x00 */ intptr_t unk0;
    /* 0x08 */ uintptr_t unk4;
    /* 0x10 */ uintptr_t unk8;
    /* 0x18 */ u16 unkC;
    /* 0x1A */ s16 unkE;
    /* 0x1C */ s16 unk10;
    /* 0x1E */ u16 unk12;
    /* 0x20 */ s16 unk14;
    /* 0x22 */ s16 unk16;
    /* 0x24 */ u8 unk18;
    /* 0x25 */ u8 unk19;
    /* 0x26 */ s8 unk1A;
    /* 0x27 */ u8 unk1B;
    /* 0x28 */ s16 unk1C;
    /* 0x2A */ s16 unk1E;
    /* 0x2C */ s16 unk20;
    /* 0x2E */ s16 unk22;
    /* 0x30 */ s16 color;
    /* 0x32 */ u16 reserved;
    /* 0x38 */ void *animFunc;
    /* 0x40 */ u32 animID;
    /* 0x48 */ intptr_t animDTA;
    /* 0x50 */ u16 index;
} Sprite;

typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 port;
    /* 0x02 */ u8 pad2[0x48 - 0x2]; void *ref;
} InputRec; /* 0x4C */

typedef struct {
    u8 pad0[6];
    s8 present;
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Controller;

extern InputRec gPlayers[];
VIEW(Controller[], gPaks);
#define gPaks gPaks_view

static s8 controller_present(u8 port)
{
    return gPaks[port].present;
}

s32 animControllerDimInternal(Sprite *spr)
{
    s32 i;
    s32 flag;

    flag = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (controller_present(gPlayers[i].port) == 0) {
            flag = 0;
        }
    }
    if (flag != spr->unk1A) {
        spr->unk1A = flag;
        UpdateBlit(spr);
    }
    return 1;
}
