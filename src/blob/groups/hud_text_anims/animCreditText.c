/*
 * animCreditText (0x8010A53C, 616 B): credits page text (attract). GC counterpart by strings: hud.c
 * animCreditText/ShowHUD credit-name table (41 names "ABRAHAM ARANDA"...). Hidden unless gGameState bit 3
 * and gShowCredits; the page corner gCorner (0..3) picks x = 30 or 290 - gCreditWidth and y = 30 or
 * 155 - 10 * gNumCreditNames; draws the title strings[lang + gCredit] in colour 14, then gNumCreditNames names
 * of page gCredit (cStrCreditNames, 11 pointers per page) 10 px apart in colour 1.
 * Shaping (disclosed): font_set_A53C is the inlined font-switch helper (non-static so umerge leaves no
 * deleted-procedure stub); its returned old font is the retail `move s1,v0`. Declaration order x before
 * hide places x's spill home at 60(sp). `i = 0; y += 15; for (;;)` gives retail's addiu order.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    void *name, *image, *info;
    u16 texIndex;
    s16 x, y;
    u16 z;
    s16 width, height;
    u8 alpha, flip;
    s8 hidden;
} Blit;

typedef struct LangInfo {
    u16 lang;
} LangInfo;

/* PC: gText's header has host pointers (port/assets/text.c), so its fields sit at host offsets */
typedef struct StrTable {
    void *pad0[3];
    LangInfo *info;
    char **strings;
} StrTable;

typedef struct CreditPage {
    char *names[11];
} CreditPage;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s32 gShowCredits;
extern s32 gCorner;
extern s32 gCreditWidth;
extern s32 gNumCreditNames;
extern s32 gCredit;
extern StrTable gText;
extern CreditPage cStrCreditNames[];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, void *);


s32 HAL_SetFont(s32 fontID);
s32 animCreditText(Blit *blt)
{
    s32 x;
    s32 hide;
    s32 y;
    s16 sx;
    s32 i;

    hide = !(gGameState & 8) || !gShowCredits;
    if (blt->hidden != hide) {
        blt->hidden = hide;
        UpdateBlit(blt);
    }
    if (blt->hidden) {
        return 1;
    }
    switch (gCorner) {
    case 0:
        x = 30;
        y = 30;
        break;
    case 1:
        x = 290 - gCreditWidth;
        y = 30;
        break;
    case 2:
        x = 30;
        y = 155 - gNumCreditNames * 10;
        break;
    default:
        x = 290 - gCreditWidth;
        y = 155 - gNumCreditNames * 10;
        break;
    }
    HAL_SetFont(11);
    TB_SetTextColor(14);
    sx = x;
    TB_DrawText(sx, y, gText.strings[gText.info->lang + gCredit]);
    TB_SetTextColor(1);
    i = 0;
    y += 15;
    for (; i < gNumCreditNames; i++) {
        TB_DrawText(sx, y, cStrCreditNames[gCredit].names[i]);
        y += 10;
    }
    return 1;
}
