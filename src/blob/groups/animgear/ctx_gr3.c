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

typedef struct { u8 *ptr; u8 id; u8 pad[7]; } Entry;
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

extern int __unit_dead[];
void SetupFontData(s32 force);


u8 HAL_GetFontAscent(void);
u8 HAL_GetFontDescent(void);

s16 TB_GetFontHeight(void) { u8 first; first=HAL_GetFontAscent(); return (s16)(first + HAL_GetFontDescent()); }
s16 TB_GetFontLineHeight(void) {
 u8 first;
 SetupFontData(0);
 first = *((u8 *)gFontHeader + 3);
 SetupFontData(0);
 return (s16)(gLeading + *((u8 *)gFontHeader + 2) + first);
}
