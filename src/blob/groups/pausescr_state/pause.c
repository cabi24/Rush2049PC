/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800B4FB0 (0x800B4FB0, 1472 B) = GC setPauseState@pausescr.c (MAT3 pausescr.c, GC 0x80078F08) with GC
 * updatePauseStateCreate (GC 0x800788A4) inlined; updatePauseStateCleanup = updatePauseStateCleanup (jal), _TitlePane = pause_TitlePane.
 * : first typed port; levers: GC `width /= 2` / `max /= 2` are halvings inside the InitPane arguments
 * on the N64 (retail divides after the TB_GetFontLineHeight() argument call, and the rumble-pane loop's half width is
 * hoisted into the loop preheader), and the rumble-pane loop counter is `i` (GC reuses `max`: one variable = one web
 * gives max and the loop counter one register; retail has s2 / s1). Helpers ps_gfx_lock/ps_gfx_unlock/ps_font_set/
 * ps_SetFont (GC HAL_SetFont) and ps_updatePauseStateCreate are non-static and not on keep (stub rule).
 * No unused locals, no block breaks.
 * N64 differences: the pause menu has 8 entries (text table 56, entries +1..+8, no GC i>3 skip) and its pane is
 * 8 lines tall; string indices single[226/171/170/216] (GC 238/181/180/227).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct OSMesgQueue OSMesgQueue;
typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;
typedef struct TextState { void *word0; char **single; void *word8; u16 *tab; char **taba; } TextState;
typedef struct { s16 x, y; } Pos;
typedef struct { s32 x, y, w, h, cx, cy; } Rect;   /* 0x18 */

extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern s32 gPauseState;
extern Blit *pause_DimBlit;                   /* DimBlit */
extern Blit *pause_TitleBlit;                   /* TitleBlit */
extern Blit *pause_PauseBlit;                   /* PauseBlit */
extern Blit *pause_SureBlit;                   /* SureBlit */
extern Blit *pause_RumbleBlit;                   /* RumbleBlit */
extern const MultiBlit DimMultiBlit, TitleMultiBlit, PauseMultiBlit, SureMultiBlit, RumbleMultiBlit;
            /* DimMultiBlit, TitleMultiBlit, PauseMultiBlit, SureMultiBlit, RumbleMultiBlit */
extern Pos pause_titlePos;                     /* titlePos */
extern s8 pause_gYes;                      /* gYes */
extern Rect pause_gPlayerRects[][4];               /* gPlayerRects */
extern s16 gNumPlayers;

s32 osRecvMesg(OSMesgQueue *, void *, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32);
u32 HAL_GetTextWidth(void *, s16);
s16 TB_GetFontLineHeight(void);
void *InitPane(s32, s32, s32, s32, s32, void *, s32, s32);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);
void updatePauseStateCleanup(void);
void pause_TitlePane(void);                     /* _TitlePane@pausescr.c */
s32 animControllerDimInternal();                       /* animRumblePane */

s32 HAL_SetFont(s32 fontID);
void ps_SetFont(s32 font)
{
    HAL_SetFont(font);
}

void ps_updatePauseStateCreate(void)
{
    s32 max;
    s32 width;
    s32 i;
    s32 x;
    s32 y;

    switch (gPauseState) {
    case 1:
        if (!pause_DimBlit) pause_DimBlit = NewMultiBlit(0, 0, &DimMultiBlit, 1);
        pause_TitlePane();
        ps_SetFont(13);
        x = pause_titlePos.x;
        y = pause_titlePos.y + 2 * TB_GetFontLineHeight();
        ps_SetFont(10);
        max = 0;
        for (i = 0; i < 8; i++) {
            width = HAL_GetTextWidth(gText.taba[i + 1 + gText.tab[56]], -1);
            if (max < width) max = width;
        }
        InitPane(x - max / 2 - 8, y - 4, x + max / 2 + 8, y + 8 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_PauseBlit = NewMultiBlit(0, 0, &PauseMultiBlit, 1);
        break;
    case 6:
        pause_gYes = 0;
        pause_TitlePane();
        ps_SetFont(10);
        x = pause_gPlayerRects[0][1].cx;
        y = pause_gPlayerRects[0][1].cy - TB_GetFontLineHeight() / 2;
        width = HAL_GetTextWidth(gText.single[226], -1);
        InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + TB_GetFontLineHeight() + 4, 176, animControllerDimInternal, 0, 0);
        ps_SetFont(11);
        width = HAL_GetTextWidth(gText.single[171], -1);
        width = (u32)width > HAL_GetTextWidth(gText.single[170], -1) ? width : HAL_GetTextWidth(gText.single[170], -1);
        for (i = 0; i < gNumPlayers; i++) {
            x = pause_gPlayerRects[gNumPlayers - 1][i].cx;
            y = pause_gPlayerRects[gNumPlayers - 1][i].cy - TB_GetFontLineHeight();
            InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + 2 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        }
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_RumbleBlit = NewMultiBlit(0, 0, &RumbleMultiBlit, 1);
        break;
    case 7:
    case 8:
        pause_gYes = 0;
        pause_TitlePane();
        ps_SetFont(13);
        x = pause_titlePos.x;
        y = pause_titlePos.y + 2 * TB_GetFontLineHeight();
        ps_SetFont(10);
        width = HAL_GetTextWidth(gText.single[216], -1);
        InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + 2 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_SureBlit = NewMultiBlit(0, 0, &SureMultiBlit, 1);
        break;
    }
}

void setPauseState(s32 state)
{
    gPauseState = state;
    updatePauseStateCleanup();
    ps_updatePauseStateCreate();
}
