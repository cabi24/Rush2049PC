/* flags: -g0 -O3 -mips2 -G 0 -non_shared (IPA group: ZOID_UpdateBlit is inlined into UpdateBlit) */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

/* The sprite list gBlitInstanceList: palette and texture are pointer-sized (particle_knot's Sprite on the PC). */
typedef struct {uintptr_t word0,word4;u16 half8,halfA,halfC,padE,half10,half12;u8 tail[12];} Record;
extern Record gBlitInstanceList[];

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9): Name, the
 * palette (Image) and the texture (Info) are pointer-sized, and so are AnimFunc and AnimDTA in the tail. */
typedef struct {
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

void ZOID_CropBlit(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void ZOID_SetBlitData(s32 arg0, s32 arg1, s32 arg2, unsigned short arg3);
void ZOID_FlipBlit(s32 arg0, s32 arg1);
void ZOID_TextBlit(s32 arg0, s32 arg1);

void ZOID_UpdateBlit(u32 index,u32 h8,uintptr_t w4,uintptr_t w0,u32 hA,u32 hC,u32 h10,u32 h12);

void UpdateBlit(Sprite *spr) {
    ZOID_UpdateBlit(spr->index, spr->unkC, spr->unk8, spr->unk4, spr->unkE, spr->unk10, spr->unk14, spr->unk16);
    ZOID_CropBlit(spr->index, spr->unk1C, spr->unk1E, spr->unk20, spr->unk22);
    ZOID_SetBlitData(spr->index, spr->unk1A, spr->unk18, spr->unk12);
    ZOID_FlipBlit(spr->index, spr->unk19);
    ZOID_TextBlit(spr->index, spr->unk0 == -1);
}
