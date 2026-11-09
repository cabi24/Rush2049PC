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

void UpdateBlit(Sprite *spr) {
    ZOID_UpdateBlit(spr->index, spr->unkC, spr->unk8, spr->unk4, spr->unkE, spr->unk10, spr->unk14, spr->unk16);
    ZOID_CropBlit(spr->index, spr->unk1C, spr->unk1E, spr->unk20, spr->unk22);
    ZOID_SetBlitData(spr->index, spr->unk1A, spr->unk18, spr->unk12);
    ZOID_FlipBlit(spr->index, spr->unk19);
    ZOID_TextBlit(spr->index, spr->unk0 == -1);
}
