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
VIEW(PadConfig, gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

/* PC: frees sprite arg0 (state 2) in the sprite list's host layout (particle_knot's Sprite, 0x28 bytes, docs/port/
 * POINTERS.md 9); the m2c body adds arg0 * 32 to the N64 address 0x80140C06 = &gBlitInstanceList[0].state. */
typedef struct { void *pal, *tex; u16 model; s16 x, y; u16 mode; s16 w, h; u8 color, flags; s8 state; u8 pad17;
    s16 t0, s0, t1, s1; } PcSprite;
void ZOID_RemoveBlit(s32 arg0) {
    ((PcSprite *) &gBlitInstanceList)[arg0].state = 2;
}
