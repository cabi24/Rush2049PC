/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Video-options screen text (GC videoscr.c), first complete draft (NOT a match).
 * func_8010F218 (kept) = GC animText@videoscr.c with GC drawColorBars inlined at its head (as vs_drawColorBars:
 * PrimColor/FillRect GBI words on the blit_dl display-list cursor: a white tick, 32 grey bars, a black bar, the
 * white/black/red/green/blue reference blocks); func_8010EA08 (internal, sole caller func_8010F218, unsaved
 * s0-s8/f20-f30) = GC animButtonText@videoscr.c (N64 drops GC's frequency row; keeps the test and confirm panes).
 * Callee map: RESULTS §4. TB_FlashTextColors = TB_FlashTextColors with GC's union type (as the audioscr).
 * Function-local statics in GC (width$636/height$637, width$645/height$646) are written as their retail data
 * symbols here (video_test_box_width/height, video_confirm_box_width/height); the two `sh 160` at EA08's entry are the widths.
 * : the 32-bar loop as `for (c = 0; c < 32; c++)` with colour `c * 8` (uopt then unrolls by 4 with four
 * strength-reduced colour IVs and the exit test `!= 280`, as retail; `c += 8` / `i` counters do not), and the
 * 3-block loop branch-free (PrimColor word `(0xFF000000 >> (i * 8)) | 0xFF` = red/green/blue) with GC's
 * white/black pairs: F218 536/611 -> 518 words, rows 529 -> 324. Residual: retail fully unrolls the 3-block loop
 * (constant colours, x chain in s0/t4); here uopt keeps the loop (trip counts 3/4/8 alike: a body-size limit?),
 * so the constant webs -1/255 (s3/s1 in retail) and the frame (384 vs 456) differ.
 * Residuals (unit): F218 536/611 words -- retail's 32-bar loop is unrolled by 4 and the 3 colour blocks are
 * straight-line on an x induction register (uopt unrolling); none of 8 loop forms unrolled here. EA08 511/514
 * words, frame exact, 287 rows: the FlashTextColors argument copies (retail: first through a stack temp with an
 * `at` word copy, second loaded directly and stored to its home), colour of the loop pointers.
 * Shaping: inlined non-static helpers vs_* (GC HAL_SetFont / TB_CenterText / TB_RightText / TB_SetJust).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct { u32 w0; uintptr_t w1; } Gwords;
typedef union { Gwords words; long long int force_structure_alignment; } Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, 0xE7)
#define gDPWords(pkt, a, b) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (a); \
    _g->words.w1 = (b); \
}
#define gDPSetPrimColor(pkt, m, l, r, g, b, a) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(0xFA, 24, 8) | _SHIFTL(m, 8, 8) | _SHIFTL(l, 0, 8)); \
    _g->words.w1 = (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)); \
}
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(0xF6, 24, 8) | _SHIFTL((lrx), 14, 10) | _SHIFTL((lry), 2, 10)); \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10)); \
}

typedef struct OSMesgQueue OSMesgQueue;
typedef struct VsIdx { u8 pad0[48]; u16 yesNo; u8 pad50[32]; u16 optionNames; } VsIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; VsIdx *indices; char **strings; } TextState;
typedef struct ButtonInfo { u8 pad[8]; f32 angle; u8 pad12[36]; u8 pos[16]; } ButtonInfo;   /* 64 B */
typedef union TextColor { u8 c[4]; u32 rgba; } TextColor;   /* GC union __anon_21da */

extern Gfx *blit_dl;                          /* display-list cursor */
extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern s32 video_buttonsInited;                           /* buttonsInited */
extern s8 video_optionValid[];                          /* validOption[4] */
extern s32 gVideoMenuSelect;
extern s32 gVertCenter;
extern s32 gHorzCenter;
extern s8 gTestFrequency;
extern s8 gConfirmFrequency;
extern s8 gYesFrequency;
extern void *gTestPane;
extern void *gConfirmPane;
extern s8 video_gControllerRemoved;                            /* gControllerRemoved */
typedef struct { s32 x, w, h, cx; } MenuRect;          /* MAT3 videoscr.c menuRect */
extern MenuRect video_menuRect;                         /* menuRect: cx, the options' right edge, the values' left */
                           /* width$636 */
                           /* width$645 */
extern s32 playerInPause;
extern s32 gGameState;
extern ButtonInfo video_buttonInfo[];                  /* buttonInfo */

extern u8 gCamera[];
extern TextColor flashColors[2];                  /* flashColors */
extern TextColor gFlashColor1, gFlashColor2;
extern s32 gJust[2];   /* gJust: tentative definition (as controls.c / credits_scroll_grp audio_animPanes_demo_animText.c); unit: extern_data */
extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
extern u32 HAL_GetTextWidth(char *, s32);
extern s16 TB_GetFontLineHeight(void);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, char *);
extern s32 TB_Sprintf(char *, char *, ...);
extern void HAL_SetTextDepth(f32);
extern void HAL_SetTextTranslucency(f32);
extern void ZOID_ProjectWindow(s32, void *, void *, void *, s16 *);
extern s16 TB_GetTextNumLines(s16, char *);
extern void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
extern void TB_DrawTextScrollBox(s16, s16, s16, s16, s32, s32, char *);
extern void RemovePane(void *);

void TB_FlashTextColors(TextColor first, TextColor second);   /* TB_FlashTextColors (ctx_flash.c) */
void vs_setWidth(s16 *width)
{
    *width = 160;
}
void video_animButtonText(void);

s32 HAL_SetFont(s32 fontID);
void vs_SetFont(s32 font)
{
    HAL_SetFont(font);
}
s8 vs_validOption(s32 item)
{
    s8 valid = video_optionValid[item];
    return valid;
}

void vs_drawColorBars(void)
{
    s32 x;
    s32 c;
    s32 i;

    gDPPipeSync(blit_dl++);
    gDPWords(blit_dl++, 0xFCFFFFFF, 0xFFFDF6FB);
    gDPWords(blit_dl++, 0xEE000000, 0x00100000);
    gDPSetPrimColor(blit_dl++, 0, 0, 255, 255, 255, 255);
    gDPFillRectangle(blit_dl++, 24, 140, 32, 160);
    x = 32;
    for (c = 0; c < 32; c++) {
        gDPSetPrimColor(blit_dl++, 0, 0, c * 8, c * 8, c * 8, 255);
        gDPFillRectangle(blit_dl++, x, 140, x + 8, 160);
        x += 8;
    }
    gDPSetPrimColor(blit_dl++, 0, 0, 0, 0, 0, 255);
    gDPFillRectangle(blit_dl++, x, 140, x + 8, 160);
    gDPSetPrimColor(blit_dl++, 0, 0, 255, 255, 255, 255);
    gDPFillRectangle(blit_dl++, 24, 160, 56, 180);
    gDPSetPrimColor(blit_dl++, 0, 0, 0, 0, 0, 255);
    gDPFillRectangle(blit_dl++, 24, 180, 56, 200);
    x = 56;
    for (i = 0; i < 3; i++) {
        gDPWords(blit_dl++, 0xFA000000, (0xFF000000 >> (i * 8)) | 0xFF);
        gDPFillRectangle(blit_dl++, x, 160, x + 48, 200);
        x += 48;
        gDPSetPrimColor(blit_dl++, 0, 0, 255, 255, 255, 255);
        gDPFillRectangle(blit_dl++, x, 160, x + 32, 180);
        gDPSetPrimColor(blit_dl++, 0, 0, 0, 0, 0, 255);
        gDPFillRectangle(blit_dl++, x, 180, x + 32, 200);
        x += 32;
    }
    gDPPipeSync(blit_dl++);
}

s16 TB_CenterText(void *text, s16 centerX);
s16 TB_RightText(void *text, s16 rightX);
s32 video_animText(s32 arg0)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    char str1[40];

    vs_drawColorBars();
    if (video_buttonsInited == 1) {
        video_animButtonText();
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    vs_SetFont(13);
    TB_SetTextColor(1);
    if ((gGameState & 0x7C0000) && video_gControllerRemoved) {
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText.fixed[233]);
        TB_DrawText(TB_CenterText(str1, 160), 10, str1);
    } else {
        str = gText.fixed[53];
        TB_DrawText(TB_CenterText(str, 160), 10, str);
    }
    y = TB_GetFontLineHeight() * 2 + 10;
    HAL_SetFont(11);
    x = video_menuRect.cx + 5;
    for (i = 0; i < 4; i++) {
        if (!video_optionValid[i]) continue;
        TB_SetTextColor(i == gVideoMenuSelect ? 22 : 1);
        str = gText.strings[gText.indices->optionNames + i];
        TB_DrawText(TB_RightText(str, video_menuRect.cx - 5), y, str);
        switch (i) {
        case 0:
            TB_Sprintf(str1, "%s%d", gVertCenter > 0 ? "+" : "", gVertCenter);
            TB_DrawText(x, y, str1);
            break;
        case 1:
            TB_Sprintf(str1, "%s%d", gHorzCenter > 0 ? "+" : "", gHorzCenter);
            TB_DrawText(x, y, str1);
            break;
        }
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

void TB_SetJust(u32 first, u32 second);
void video_animButtonText(void)
{
    s32 i;
    s32 j;
    s32 x;
    s32 y;
    char *str;
    char str1[40];
    s16 screenPos[2];
    static s16 testWidth;
    static s16 confirmWidth;
    s16 lines;

    vs_setWidth(&confirmWidth);
    testWidth = 160;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    HAL_SetFont(13);
    TB_SetTextColor(1);
    ZOID_ProjectWindow(0, video_buttonInfo[11].pos, gCamera, 0, screenPos);   /* the title's position */
    x = screenPos[0];
    y = screenPos[1];
    str = gText.fixed[53];
    TB_DrawText(TB_CenterText(str, x), y, str);
    HAL_SetFont(11);
    for (i = 0; i < 4; i++) {
        if (vs_validOption(i)) {
            if (video_buttonInfo[i].angle > 0.69813168f && video_buttonInfo[i].angle < 2.443461f) continue;
            if (video_buttonInfo[i].angle > 0.34906584f && video_buttonInfo[i].angle < 2.7925267f) HAL_SetTextTranslucency(128.0f);
            else HAL_SetTextTranslucency(-1.0f);
            if (video_buttonInfo[i].angle < 1.5707964f) {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(22);
            }
            ZOID_ProjectWindow(0, video_buttonInfo[i].pos, gCamera, 0, screenPos);
            x = screenPos[0] - 55;
            y = screenPos[1];
            str = gText.strings[gText.indices->optionNames + i];
            TB_DrawText(x, y, str);
            ZOID_ProjectWindow(0, video_buttonInfo[i + 4].pos, gCamera, 0, screenPos);
            x = screenPos[0] - 35;
            y = screenPos[1];
            switch (i) {
            case 0:
                TB_Sprintf(str1, "%s%d", gVertCenter > 0 ? "+" : "", gVertCenter);
                TB_DrawText(x, y, str1);
                break;
            case 1:
                TB_Sprintf(str1, "%s%d", gHorzCenter > 0 ? "+" : "", gHorzCenter);
                TB_DrawText(x, y, str1);
                break;
            }
        }
    }
    TB_SetJust(1, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    if (gTestFrequency == 1) {
        static s16 height = 1;
        HAL_SetFont(10);
        if (gTestPane == 0) {
            lines = TB_GetTextNumLines(testWidth, gText.fixed[217]);
            height = TB_GetFontLineHeight() * lines;
            gTestPane = InitPane(152 - testWidth / 2, 51, testWidth / 2 + 168, height + 59, 176, 0, 0, 0);
        }
        TB_DrawTextScrollBox(160, 55, testWidth, height, -1, 0, gText.fixed[217]);
    } else if (gTestPane != 0) {
        RemovePane(gTestPane);
        gTestPane = 0;
    }
    if (gConfirmFrequency == 1) {
        static s16 height = 1;
        HAL_SetFont(10);
        if (gConfirmPane == 0) {
            lines = TB_GetTextNumLines(confirmWidth, gText.fixed[218]) + 1;
            height = TB_GetFontLineHeight() * lines;
            gConfirmPane = InitPane(152 - confirmWidth / 2, 51, confirmWidth / 2 + 168, height + 59, 176, 0, 0, 0);
        }
        TB_DrawTextScrollBox(160, 55, confirmWidth, height, -1, 0, gText.fixed[218]);
        y = height - TB_GetFontLineHeight() + 55;
        x = 160 - confirmWidth / 4;
        str = gText.strings[gText.indices->yesNo];
        TB_SetTextColor(gYesFrequency ? 22 : 1);
        TB_DrawText(x, y, str);
        x = confirmWidth / 4 + 160;
        str = gText.strings[gText.indices->yesNo + 1];
        TB_SetTextColor(gYesFrequency ? 1 : 22);
        TB_DrawText(x, y, str);
    } else if (gConfirmPane != 0) {
        RemovePane(gConfirmPane);
        gConfirmPane = 0;
    }
    TB_SetJust(0, 3);
}
