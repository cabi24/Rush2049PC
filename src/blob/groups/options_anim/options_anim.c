/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_8010AEAC = GC animText@optionscr.c (options screen text), func_8010A8D0 = animButtonText (button-mode
 * labels at projected 3D positions), func_8010A7A4 = printOption(x, y, item, name, value) (internal, register
 * parameters x=s1, y=s2, name=s3, value=s4, item=a0: IPA from the prototype order, GC signature
 * `static void printOption(int x, int y, int item, const unsigned char *name, const unsigned char *value)`).
 * draft (AEAC 357/413, A8D0 325/375, A7A4 16/75) -> (290/241/16) -> : all three unit EQUAL.
 * Levers:
 *  - TB_FlashTextColors_AEAC takes the GC colour union (`union { u8 c[4]; u32 rgba; }`, GC __anon_21da) BY VALUE and
 *    copies `.rgba` (GC TB_text.c: two lwz/stw; GC callers pass flashColors[0]/[1] = flashColors[2]): by-value
 *    aggregate parameters have stack homes, so retail's `sw t8,40(sp)`/`sw t9,44(sp)` (and 32/36) are the
 *    inlined parameter homes, the values stay in ring temps. A7A4 16 -> 0 words; A8D0 frame 160 -> 152.
 *    (Union-to-union assignment instead compiles to an `at` block copy.)
 *  - `case 0:` written LAST in both switches (retail places its code after case 13): A8D0 -> EQUAL, AEAC ops 0.
 *  - AEAC: gGameState read into a named local `gs` (retail keeps it in v0).
 * Shaping devices (disclosed): helpers non-static and uniquely named (*_AEAC, not on keep: stub rule);
 *  - inlined helpers gfx_lock_AEAC/gfx_unlock_AEAC/font_set_AEAC (family lock/font helpers), TB_SetJust_AEAC (= locked
 *    TB_SetJust's body), TB_CenterText_AEAC (GC TB_text.c), TB_FlashTextColors_AEAC (GC TB_text.c).
 *  - AEAC `s32 u0` is an UNUSED local = exact frame residual: retail frame 240; without it the frame is still 240
 *    but str1/itemsDrawn sit 4 B higher (180/222 vs retail 176/218: 5 words differ); one 4-byte named slot.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

typedef union Color {
    u8 c[4];
    u32 rgba;
} Color;

typedef struct TextHdr {
    char *str[64];
} TextHdr;

typedef struct LangInfo {
    u8 pad0[32];
    u16 base;
} LangInfo;

typedef struct Text {
    u8 pad0[4];
    TextHdr *hdr;
    u8 pad8[4];
    LangInfo *info;
    char **strings;
} Text;

typedef struct ButtonInfo {
    u8 pad0[8];
    f32 ang;
    u8 padC[48 - 12];
    f32 pos[3];
    u8 alpha;
    u8 pad3D[3];
} ButtonInfo;

typedef struct Pair {
    s32 h, v;
} Pair;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s32 playerInPause;
extern Text gText;
extern s32 options_buttonsInited;
extern s16 options_gSelectedOption;
extern ButtonInfo options_buttonInfo[];
extern f32 options_title_button_pos[3];
extern u8 gCamera[];
extern Color flashColors[2];
extern Color gFlashColor1;
extern Color gFlashColor2;
extern Pair gJust;
extern s8 gOptions[];
extern s16 gLang;
extern char *cStrLanguageNames[];
extern s32 options_validOption[14];
extern s16 options_gFirstOption;
extern s16 gOptionMenuSize;
extern s8 options_gControllerRemoved;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void HAL_SetTextDepth(f32);
void HAL_SetTextTranslucency(f32);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, void *);
u32 HAL_GetTextWidth(void *, s32);
s16 TB_GetFontLineHeight(void);
void ZOID_ProjectWindow(s32, f32 *, void *, f32 *, s16 *);
void ZeroCameras(void);
void TB_Sprintf(char *, char *, ...);

void gfx_lock_AEAC(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_AEAC(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_AEAC(s32 font)
{
    s32 old;

    gfx_lock_AEAC();
    old = HAL_SetFontInternal(font);
    gfx_unlock_AEAC();
    return old;
}

void TB_FlashTextColors_AEAC(Color a, Color b)
{
    gFlashColor1.rgba = a.rgba;
    gFlashColor2.rgba = b.rgba;
}

void TB_SetJust_AEAC(s32 h, s32 v)
{
    gJust.v = v;
    gJust.h = h;
}

s16 TB_CenterText_AEAC(void *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}

void options_printOption(s32 x, s32 y, s32 item, char *name, char *value)
{
    if (options_buttonsInited == 1) {
        if (options_buttonInfo[item].ang < 1.5707964f) {
            TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
            TB_SetTextColor(22);
        }
    } else {
        TB_SetTextColor(item == options_gSelectedOption ? 22 : 1);
    }
    TB_DrawText(x - 105, y, name);
    TB_DrawText(x + 40, y, value);
}

#define OPT_VALUE(k) gText.strings[gText.info->base + gOptions[k]]

void options_animButtonText(void)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    s16 screenPos[2];

    HAL_SetTextDepth(0.0f);
    TB_SetJust_AEAC(0, 1);
    HAL_SetTextDepth(32320.0f);
    ZOID_ProjectWindow(0, options_title_button_pos, gCamera, 0, screenPos);
    x = screenPos[0];
    y = screenPos[1];
    font_set_AEAC(13);
    TB_SetTextColor(1);
    str = gText.hdr->str[50];
    TB_DrawText(TB_CenterText_AEAC(str, x), y, str);
    font_set_AEAC(11);
    for (i = 0; i < 14; i++) {
        if ((0.69813168f < options_buttonInfo[i].ang && options_buttonInfo[i].ang < 2.443461f) || !options_buttonInfo[i].alpha) {
            continue;
        }
        HAL_SetTextTranslucency(options_buttonInfo[i].alpha);
        HAL_SetTextDepth(32384.0f);
        ZOID_ProjectWindow(0, options_buttonInfo[i].pos, gCamera, 0, screenPos);
        x = screenPos[0] + 50;
        y = screenPos[1];
        switch (i) {
        case 1:
            options_printOption(x, y, 1, gText.hdr->str[13], OPT_VALUE(10));
            break;
        case 2:
            options_printOption(x, y, 2, gText.hdr->str[15], OPT_VALUE(0));
            break;
        case 3:
            options_printOption(x, y, 3, gText.hdr->str[16], OPT_VALUE(1));
            break;
        case 4:
            options_printOption(x, y, 4, gText.hdr->str[17], OPT_VALUE(2));
            break;
        case 5:
            options_printOption(x, y, 5, gText.hdr->str[18], OPT_VALUE(3));
            break;
        case 6:
            options_printOption(x, y, 6, gText.hdr->str[19], OPT_VALUE(4));
            break;
        case 7:
            options_printOption(x, y, 7, gText.hdr->str[20], OPT_VALUE(5));
            break;
        case 8:
            options_printOption(x, y, 8, gText.hdr->str[21], OPT_VALUE(6));
            break;
        case 9:
            options_printOption(x, y, 9, gText.hdr->str[22], OPT_VALUE(7));
            break;
        case 10:
            options_printOption(x, y, 10, gText.hdr->str[23], OPT_VALUE(8));
            break;
        case 11:
            options_printOption(x, y, 11, gText.hdr->str[24], OPT_VALUE(9));
            break;
        case 12:
            options_printOption(x, y, 12, gText.hdr->str[26], OPT_VALUE(18));
            break;
        case 13:
            options_printOption(x, y, 13, gText.hdr->str[14], OPT_VALUE(11));
            break;
        case 0:
            options_printOption(x, y, 0, gText.hdr->str[25], cStrLanguageNames[gLang]);
            break;
        }
    }
    ZeroCameras();
    TB_SetJust_AEAC(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
}

s32 options_animText(void *item)
{
    u32 gs;
    s32 u0;
    s32 i;
    s32 y;
    char *str;
    s16 itemsDrawn;
    char str1[40];

    if (options_buttonsInited == 1) {
        options_animButtonText();
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    font_set_AEAC(13);
    TB_SetTextColor(1);
    gs = gGameState;
    if ((gs & 0x7C0000) && options_gControllerRemoved) {
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText.hdr->str[233]);
        TB_DrawText(TB_CenterText_AEAC(str1, 160), 10, str1);
    } else {
        str = gText.hdr->str[50];
        TB_DrawText(TB_CenterText_AEAC(str, 160), 10, str);
    }
    y = TB_GetFontLineHeight() * 2 + 10;
    font_set_AEAC(11);
    itemsDrawn = 0;
    for (i = 0; i < 14; i++) {
        if (!options_validOption[i]) {
            continue;
        }
        if (i < options_gFirstOption) {
            continue;
        }
        if (itemsDrawn >= gOptionMenuSize) {
            continue;
        }
        switch (i) {
        case 1:
            options_printOption(160, y, 1, gText.hdr->str[13], OPT_VALUE(10));
            break;
        case 2:
            options_printOption(160, y, 2, gText.hdr->str[15], OPT_VALUE(0));
            break;
        case 3:
            options_printOption(160, y, 3, gText.hdr->str[16], OPT_VALUE(1));
            break;
        case 4:
            options_printOption(160, y, 4, gText.hdr->str[17], OPT_VALUE(2));
            break;
        case 5:
            options_printOption(160, y, 5, gText.hdr->str[18], OPT_VALUE(3));
            break;
        case 6:
            options_printOption(160, y, 6, gText.hdr->str[19], OPT_VALUE(4));
            break;
        case 7:
            options_printOption(160, y, 7, gText.hdr->str[20], OPT_VALUE(5));
            break;
        case 8:
            options_printOption(160, y, 8, gText.hdr->str[21], OPT_VALUE(6));
            break;
        case 9:
            options_printOption(160, y, 9, gText.hdr->str[22], OPT_VALUE(7));
            break;
        case 10:
            options_printOption(160, y, 10, gText.hdr->str[23], OPT_VALUE(8));
            break;
        case 11:
            options_printOption(160, y, 11, gText.hdr->str[24], OPT_VALUE(9));
            break;
        case 12:
            options_printOption(160, y, 12, gText.hdr->str[26], OPT_VALUE(18));
            break;
        case 13:
            options_printOption(160, y, 13, gText.hdr->str[14], OPT_VALUE(11));
            break;
        case 0:
            options_printOption(160, y, 0, gText.hdr->str[25], cStrLanguageNames[gLang]);
            break;
        }
        y += TB_GetFontLineHeight();
        itemsDrawn++;
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
