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

extern Bank *gFontHeader;
extern s32 gFontDatWaveInst;
extern s32 gFontTexWaveInst;
extern Tbl gWaveInstance_ptr[];
extern Entry *gFontLetters;
extern s16 gCharMap[256];
extern intptr_t gFontTextures[];
extern Pair gTexDefPtrs[];
extern s8 gLeading;
extern u8 gSpacing;
extern s32 font_image_format;
extern s32 font_texel_size;

extern s32 FindWaveInstanceInternal(s32, s32);

void SetupFontData(s32 force);



s8 HAL_SetFontFixedWidth(s8 value) {
 s8 old;
 SetupFontData(0);
 old = gFontHeader->f9;
 gFontHeader->f9 = value;
 return old;
}
void font_get_char_list(u8 *out) {
 s32 offset;
 s32 i;
 SetupFontData(0);
 for (i = 0, offset = 0; i < gFontHeader->f12; offset += 12, i++) {
  out[i] = *((u8 *)gFontLetters + offset + 4);
 }
 out[i] = 0;
}
s32 HAL_GetCharWidth(u16 id) {
 s16 index;
 SetupFontData(0);
 if ((u8)gFontHeader->f9 != 0) return gFontHeader->f6;
 if (id == 32 || id >= 256) return gFontHeader->f7;
 if (id == 10) return 0;
 index = gCharMap[id];
 if (index < 0) return gFontHeader->f7;
 return gFontLetters[index].pad[3] - gFontLetters[index].pad[1] + 1;
}
