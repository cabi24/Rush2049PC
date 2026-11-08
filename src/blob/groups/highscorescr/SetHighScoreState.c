/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F1210 = GC `_SetHighScoreState(newState)` (highscorescr.c, confident pair). newState in $s2
 * (IPA register parameter). Old state's panes are removed (RemovePane = sound_handles_array_clear), the new
 * state's panes are built (InitPane = InitPane) and the countdown timer set (SetCountdownTimer =
 * SetCountdownTimer). GC names: gHighScoreState D_80140B20, blit highscore_blit (RemoveBlit = RemoveBlit,
 * NewMultiBlit = NewMultiBlit, mblit highscore_mblit), namePanes D_80149418[], codePane D_80146208, ghost panes
 * ghostPanes[3], score panes scorePanes[2], gNumPlayers gNumPlayers, gNumWindows gNumWindows,
 * gPlayerRects highscore_gPlayerRects, gRace gRace, gTable gTable, gGhostMenuSelect gGhostMenuSelect,
 * gGhostPak gGhostPak, _UninitScreen highscore_UninitScreen, _FindGhosts func_800F0F44, _PrintScores func_800F0914,
 * HAL_SetFont = HAL_SetFont (inlined font_set), HAL_GetTextWidth = HAL_GetTextWidth,
 * TB_GetFontLineHeight = TB_GetFontLineHeight.
 * Shaping notes: the second switch reads the global back (`switch (D_80140B20)` after the store),
 * which is what makes uopt reload it per case for the final test; the font switches go through the
 * non-static internal helpers hs_font_set/hs_gfx_lock/hs_gfx_unlock (umerge inlines the split form, not a
 * one-piece wrapper with 4 sites); one function-level `p` for the player loops, block-scoped `i`/`pak`.
 * No unused locals, no compiled-out reads.
 */
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;
typedef struct Rect { s16 x,y,w,h; s16 unknown[2]; } Rect;
typedef struct TextInfo { char unknown[54]; u16 index; } TextInfo;
/* PC: gText's header has host pointers (port/assets/text.c), so info and strings sit at host offsets. */
typedef struct TextState { void *unknown[3]; TextInfo *info; char **strings; } TextState;
extern TextState gText;
extern intptr_t highscore_blit;
extern s32 gHighScoreState, gRace, gGhostMenuSelect;
extern s16 gNumPlayers, gNumWindows;
extern s8 *namePanes[], *codePane, *ghostPanes[3], *scorePanes[2];
extern s8 gGhostPak, gTable;
extern Rect highscore_gPlayerRects[][4];
extern char ghost_menu_title_text[];
extern struct pc_multiblit highscore_mblit[1];   /* NewMultiBlit's descriptor (pc_prelude.h) */
extern void RemoveBlit(intptr_t);
extern void RemovePane(s8 *);
extern void highscore_UninitScreen(void);
extern void *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
extern void SetCountdownTimer(f32);
typedef struct { s32 opaque[6]; } OSMesgQueue;
extern OSMesgQueue text_lock_queue;
extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
void hs_gfx_lock(void) { osRecvMesg(&text_lock_queue, (void *)0, 1); }
void hs_gfx_unlock(void) { osSendMesg(&text_lock_queue, (void *)0, 0); }
s32 hs_font_set(s32 font)
{
    s32 old;
    hs_gfx_lock();
    old = HAL_SetFontInternal(font);
    hs_gfx_unlock();
    return old;
}
extern s32 HAL_GetTextWidth(char *,s32);
extern s16 TB_GetFontLineHeight(void);
extern void FindGhosts(s32), PrintScores(void);
extern intptr_t NewMultiBlit(s32,s32,void *,s32);

void SetHighScoreState(s32 newState)
{
    s32 p;
    s32 width;

    if (newState == gHighScoreState) return;
    if (highscore_blit != 0) {
        RemoveBlit(highscore_blit);
        highscore_blit = 0;
    }
    switch (gHighScoreState) {
    case 1: {
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] != 0) {
                RemovePane(namePanes[p]);
                namePanes[p] = 0;
            }
        }
        break;
    }
    case 2:
        if (codePane != 0) {
            RemovePane(codePane);
            codePane = 0;
        }
        break;
    case 3: {
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] != 0) {
                RemovePane(namePanes[p]);
                namePanes[p] = 0;
            }
        }
        break;
    }
    case 4: {
        s32 i;
        for (i = 0; i < 3; i++) {
            if (ghostPanes[i] != 0) {
                RemovePane(ghostPanes[i]);
                ghostPanes[i] = 0;
            }
        }
        break;
    }
    case 5: {
        s32 i;
        for (i = 0; i < 2; i++) {
            if (scorePanes[i] != 0) {
                RemovePane(scorePanes[i]);
                scorePanes[i] = 0;
            }
        }
        break;
    }
    }
    gHighScoreState = newState;
    switch (gHighScoreState) {
    case 0:
        highscore_UninitScreen();
        break;
    case 1: {
        s32 p;
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] == 0) {
                namePanes[p] = InitPane(highscore_gPlayerRects[gNumWindows - 1][p].x,
                    highscore_gPlayerRects[gNumWindows - 1][p].y,
                    highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                    highscore_gPlayerRects[gNumWindows - 1][p].y + highscore_gPlayerRects[gNumWindows - 1][p].h, 176, 0, 0, 0);
            }
        }
        SetCountdownTimer(600.0f);
        break;
    }
    case 2:
        codePane = InitPane(83, 70, 237, 136, 192, 0, 0, 0);
        SetCountdownTimer(600.0f);
        break;
    case 3: {
        s32 p;
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] == 0) {
                namePanes[p] = InitPane(highscore_gPlayerRects[gNumWindows - 1][p].x,
                    highscore_gPlayerRects[gNumWindows - 1][p].y,
                    highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                    highscore_gPlayerRects[gNumWindows - 1][p].y + highscore_gPlayerRects[gNumWindows - 1][p].h, 176, 0, 0, 0);
            }
        }
        SetCountdownTimer(600.0f);
        break;
    }
    case 4: {
        s32 pak;
        if (ghostPanes[0] == 0) {
            hs_font_set(13);
            width = HAL_GetTextWidth(ghost_menu_title_text, -1);
            ghostPanes[0] = InitPane(160 - width / 2 - 8, 6, width / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
        }
        if (ghostPanes[1] == 0) {
            hs_font_set(10);
            ghostPanes[1] = InitPane(91, 51, 229, TB_GetFontLineHeight() * 3 + 59, 176, 0, 0, 0);
        }
        if (ghostPanes[2] == 0) {
            hs_font_set(10);
            ghostPanes[2] = InitPane(91, 106, 229, TB_GetFontLineHeight() * 5 + 114, 176, 0, 0, 0);
        }
        SetCountdownTimer(600.0f);
        gGhostMenuSelect = 0;
        gGhostPak = 0;
        for (pak = 0; pak < 4; pak++) {
            FindGhosts(pak);
        }
        break;
    }
    case 5:
        gTable = (gRace == 2) ? 2 : 0;
        PrintScores();
        if (scorePanes[0] == 0) {
            hs_font_set(13);
            width = HAL_GetTextWidth(gText.strings[gText.info->index + gTable], -1);
            scorePanes[0] = InitPane(160 - width / 2 - 8, 6, width / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
        }
        if (scorePanes[1] == 0) {
            scorePanes[1] = InitPane(8, 75, 312, 200, 176, 0, 0, 0);
        }
        if (gRace == 2) {
            SetCountdownTimer(30.0f);
        } else {
            SetCountdownTimer(15.0f);
        }
        break;
    }
    if (gHighScoreState != 0) {
        highscore_blit = NewMultiBlit(0, 0, highscore_mblit, 1);
    }
}
