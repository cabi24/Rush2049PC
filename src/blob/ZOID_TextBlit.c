#include "game/game.h"

typedef struct PadConfig_s {
    struct PadConfig_s *unk0;
    struct PadConfig_s *unk4;
    s16 unk8;
    s16 unkA;
    s16 unkC;
    s16 unkE;
    s16 unk10;
    s16 unk12;
    u8 unk14;
    u8 unk15;
    u8 unk16;
    u8 pad17;
    s16 unk18;
    s16 unk1A;
    s16 unk1C;
    s16 unk1E;
} PadConfig;
VIEW(PadConfig[4], gBlitInstanceList);
#define gBlitInstanceList (gBlitInstanceList_view[0])

void ZOID_TextBlit(s32 arg0, s32 arg1) {
    PadConfig *temp_v0;
    PadConfig *temp_v0_2;

    if (arg1 != 0) {
        temp_v0 = &(&gBlitInstanceList)[arg0];
        temp_v0->unk15 |= 0x80;
        return;
    }
    temp_v0_2 = &(&gBlitInstanceList)[arg0];
    temp_v0_2->unk15 &= 0xFF7F;
}
