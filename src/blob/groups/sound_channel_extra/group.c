/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Bank {
    /* 0x0 */ u8 pad0;
    /* 0x1 */ u8 mode;
    /* 0x2 */ u8 pad2[2];
    /* 0x4 */ u8 f4;
    /* 0x5 */ u8 pad5;
    /* 0x6 */ u8 f6;
    /* 0x7 */ u8 f7;
    /* 0x8 */ u8 f8;
    /* 0x9 */ s8 f9;
    /* 0xA */ u8 f10;
    /* 0xB */ u8 f11;
    /* 0xC */ u8 f12;
} Bank;

/* PC: a font letter is 12 bytes as in the file, its kern slot a 4-byte window address (port/assets/font.c). */
typedef struct { u32 ptr; u8 id; u8 pad[7]; } Entry;
typedef struct { Bank *bank; } Hdr;
typedef struct { Hdr *hdr; s32 pad[4]; } Tbl;
typedef struct { s32 first; s32 second; } Pair;

extern Bank *D_801497F0;
extern s32 D_80149780;
extern s32 D_801497A4;
extern Tbl D_80156D44[];
extern Entry *D_80149800;
extern s16 D_80149878[256];
extern intptr_t D_80149820[];
extern Pair gTexDefPtrs[];
extern s8 D_80149B60;
extern u8 D_80149B70;
extern s32 D_80149B08;
extern s32 D_80149B28;

extern s32 func_80097694(s32, s32);

void sound_update_channel(s32 force);



s8 HAL_SetFontFixedWidth(s8 value) {
 s8 old;
 sound_update_channel(0);
 old = D_801497F0->f9;
 D_801497F0->f9 = value;
 return old;
}
void func_800F68A4(u8 *out) {
 s32 offset;
 s32 i;
 sound_update_channel(0);
 for (i = 0, offset = 0; i < D_801497F0->f12; offset += 12, i++) {
  out[i] = *((u8 *)D_80149800 + offset + 4);
 }
 out[i] = 0;
}
s32 HAL_GetCharWidth(u16 id) {
 s16 index;
 sound_update_channel(0);
 if ((u8)D_801497F0->f9 != 0) return D_801497F0->f6;
 if (id == 32 || id >= 256) return D_801497F0->f7;
 if (id == 10) return 0;
 index = D_80149878[id];
 if (index < 0) return D_801497F0->f7;
 return D_80149800[index].pad[3] - D_80149800[index].pad[1] + 1;
}
