/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * (integration, coordinator decision): group highscore_anim landed with members DrawName, DrawGhost and
 *   DrawUnlocked PROVISIONAL ON THE CALLER FORM. The members are byte-exact (group MATCH; DrawGhost's two-region own
 *   .rodata proven by the splice replay) and EQUAL in the whole-program unit, but their only caller highscore_animText
 *   below is an EQUIVALENT draft (27/445, frame 704 vs retail 584) kept as context, not a match. The calling
 * conventions it imposes are regmap-verified (DrawName takes p in s5, ipa_callees.json). precedent: the
 * ShowPakError cells on ShowPakError. When highscore_animText closes, add it as a member here.
 */
/*
 * : DrawUnlocked now EQUAL in the unit (was 588/629, 48 ops). Levers, in order of effect:
 * (1) GC type: `x` is `int` (GC DrawUnlocked locals x,y,i,p int; str[64]; stuffCount int) -- the s16 x of -
 *       was the whole 48-op residual (588 -> 68 words, ops 0);
 *   (2) DISCLOSED SHAPING DEVICE: compiled-out `if (p) {}` at the head of the player loop raises p's priority over the
 *       &str address web (p 42.25 vs &str 42.86 per block, ctrace) so p takes s5 and &str s6 as in retail (68 -> 4);
 *   (3) du_gfx_unlock DEFINED AFTER du_font_set (prototype before): as1 breaks the ready-list tie by source line, so
 *       the font helper's `move s0,v0` is scheduled before the unlock's argument set-up and the osSendMesg delay slot
 *       gets `move a2,zero` (retail) (4 -> 0).
 */
/*
 * : DrawGhost (DrawGhost) now EQUAL in the unit: a compiled-out `do {} while (0);` after the status-1/2 time
 *   sprintf (DISCLOSED SHAPING DEVICE) ends the block holding the TB_Sprintf call, so the PRE reload of
 *   gGhostStatus[pak] after it no longer shares that block and uopt lets the status web take v0 (retail).
 * high-score unit (GC highscorescr.c), complete drafts of all four N64 functions:
 *   DrawUnlocked = DrawUnlocked (internal), DrawName = DrawName(p) (internal, p in s5 by IPA),
 *   DrawGhost = DrawGhost (internal), highscore_animText = animText@highscorescr.c (kept; DrawCircuitCode and
 *   DrawScores are inlined into it).
 * Unit (Pi): the whole-program unit --tag score highscore_animText DrawName DrawGhost DrawUnlocked --with THIS
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 *   -> EQUAL DrawName (260 words, internal); DrawGhost 17/601 (6 unverified own-.rodata rows; colour);
 *      highscore_animText 27/445 (frame 704 vs 584: helper inline areas); DrawUnlocked 588/629 (frame/length exact; colour).
 * NOT a match deliverable: DrawName's only caller highscore_animText is unmatched (provisional).
 * Shaping devices (disclosed): non-static inlined helpers (not on keep) hs_SetFont (void HAL_SetFont wrapper,
 *   DrawName), hs_SetJust (TB_SetJust, DrawGhost), gh_font_set/gh_SetFont (DrawGhost's own font helper copy + void
 *   wrapper), hs_CenterText at the 9 DrawUnlocked sites; one unused local `s16 gFreePages` in DrawGhost (GC local;
 *   exact frame residual 568 -> 576, one slot).
 * The rest of this header is the DrawUnlocked note (historical).
 */
/*
 * DrawUnlocked = GC DrawUnlocked (highscorescr.c): per player with new unlocks (gStuffUnlocked[p] =
 * gStuffUnlocked), count the items unlocked this race (new flag set, old flag clear) over seven categories,
 * resize the player's name pane (D_80149418[p], ChangePaneSize = ChangePaneSize) around the list and draw
 * the header lines (fixed[55], fixed[56]) and one line per new item, centred on the player rect.
 * Internal (no callee saves); only caller highscore_animText (unmatched) -> provisional at best.
 * draft, complete. Measured KEPT (the whole-program unit without --internal, so +9 callee saves):
 *   FAIL DrawUnlocked: 619 of 629 words differ; compiled body is 631 words | words 573 ops 103 | frame 264/232
 * The ops rows are the save/restore block, the x/y narrowing copies and the spill homes; the control flow,
 * category order/sizes, globals and every call's arguments agree with retail. Internal measurement needs a
 * real caller: umerge inlines it into any C stand-in (1, 2 or 6 sites, with the whole-program unit's dead block, with a
 * 600-byte caller frame, stand-in defined first: all inlined), so it waits for highscore_animText (GC DrawScores?).
 * GC names: gStuffUnlocked gStuffUnlocked, namePanes D_80149418, gPlayerRects highscore_gPlayerRects (Rect.cx/cy at +8/+10),
 * gNumWindows gNumWindows, gTrackTitles gTrackTitles, CarTitles CarTitles, ChangePaneSize ChangePaneSize,
 * TB_GetFontLineHeight TB_GetFontLineHeight, HAL_GetTextWidth HAL_GetTextWidth (u32: retail `srl`),
 * TB_Sprintf TB_Sprintf ("%s %s" style formats unlocked_circuit_format..unlocked_frame_format), header lines fixed[55]/fixed[56].
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct Rect { s16 x, y, w, h; s16 cx, cy; } Rect;
typedef struct UIdx { u16 h[48]; } UIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; UIdx *indices; char **strings; } TextState;

extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern s16 gNumPlayers, gNumWindows;      /* gNumPlayers, gNumWindows */
extern Rect highscore_gPlayerRects[][4];                     /* gPlayerRects */
extern s8 *namePanes[];                         /* namePanes */
extern s8 gStuffUnlocked[];                          /* gStuffUnlocked[p] */
extern s8 gUnlockedTracks[][19], gPrevUnlockedTracks[][19];    /* tracks: unlocked now / before */
extern s8 gUnlockedCars[][13], gPrevUnlockedCars[][13];    /* cars */
extern s8 gUnlockedCircuits[][4], gPrevUnlockedCircuits[][4];
extern s8 gUnlockedTrans[][8], gPrevUnlockedTrans[][8];
extern s8 gUnlockedEngines[][9], gPrevUnlockedEngines[][9];
extern s8 gUnlockedTires[][5], gPrevUnlockedTires[][5];
extern s8 gUnlockedFrames[][6], gPrevUnlockedFrames[][6];
extern char *gTrackTitles[];                       /* gTrackTitles */
extern char *CarTitles[];                       /* CarTitles */
extern char unlocked_circuit_format[], unlocked_trans_format[], unlocked_engine_format[], unlocked_tires_format[], unlocked_frame_format[];
extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
extern u32 HAL_GetTextWidth(char *, s32);
extern s16 TB_GetFontLineHeight(void);
extern void ChangePaneSize(void *, s32, s32, s32, s32);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, char *);
extern s32 TB_Sprintf(char *, char *, ...);

void du_gfx_lock(void) { osRecvMesg(&text_lock_queue, 0, 1); }
void du_gfx_unlock(void);
s32 du_font_set(s32 font)
{
    s32 old;
    du_gfx_lock();
    old = HAL_SetFontInternal(font);
    du_gfx_unlock();
    return old;
}

void du_gfx_unlock(void) { osSendMesg(&text_lock_queue, 0, 0); }
s16 hs_CenterText(char *text, s16 centerX);

void DrawUnlocked(void)
{
    s32 x;
    s32 y;
    s32 i;
    s32 p;
    char str[64];
    s32 stuffCount;

    du_font_set(11);
    for (p = 0; p < gNumPlayers; p++) {
        if (p) {}   /* : disclosed compiled-out holder (p over &str priority) */
        if (gStuffUnlocked[p] == 0) continue;
        stuffCount = 3;
        for (i = 0; i < 19; i++) if (gUnlockedTracks[p][i] && !gPrevUnlockedTracks[p][i]) stuffCount++;
        for (i = 0; i < 13; i++) if (gUnlockedCars[p][i] && !gPrevUnlockedCars[p][i]) stuffCount++;
        for (i = 0; i < 4; i++) if (gUnlockedCircuits[p][i] && !gPrevUnlockedCircuits[p][i]) stuffCount++;
        for (i = 0; i < 8; i++) if (gUnlockedTrans[p][i] && !gPrevUnlockedTrans[p][i]) stuffCount++;
        for (i = 0; i < 9; i++) if (gUnlockedEngines[p][i] && !gPrevUnlockedEngines[p][i]) stuffCount++;
        for (i = 0; i < 5; i++) if (gUnlockedTires[p][i] && !gPrevUnlockedTires[p][i]) stuffCount++;
        for (i = 0; i < 6; i++) if (gUnlockedFrames[p][i] && !gPrevUnlockedFrames[p][i]) stuffCount++;
        x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
        y = highscore_gPlayerRects[gNumWindows - 1][p].cy - stuffCount * TB_GetFontLineHeight() / 2;
        if (namePanes[p] != 0) {
            ChangePaneSize(namePanes[p], highscore_gPlayerRects[gNumWindows - 1][p].x, y - 4,
                highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                y + stuffCount * TB_GetFontLineHeight() + 4);
        }
        TB_SetTextColor(1);
        TB_DrawText(hs_CenterText(gText.fixed[55], x), y, gText.fixed[55]);
        y += TB_GetFontLineHeight();
        TB_DrawText(hs_CenterText(gText.fixed[56], x), y, gText.fixed[56]);
        y += TB_GetFontLineHeight() * 2;
        TB_SetTextColor(22);
        for (i = 0; i < 19; i++) {
            if (gUnlockedTracks[p][i] && !gPrevUnlockedTracks[p][i]) {
                TB_DrawText(hs_CenterText(gTrackTitles[i], x), y, gTrackTitles[i]);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 13; i++) {
            if (gUnlockedCars[p][i] && !gPrevUnlockedCars[p][i]) {
                TB_DrawText(hs_CenterText(CarTitles[i], x), y, CarTitles[i]);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 4; i++) {
            if (gUnlockedCircuits[p][i] && !gPrevUnlockedCircuits[p][i]) {
                TB_Sprintf(str, unlocked_circuit_format, gText.strings[gText.indices->h[44] + i], gText.fixed[42]);
                TB_DrawText(hs_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 8; i++) {
            if (gUnlockedTrans[p][i] && !gPrevUnlockedTrans[p][i]) {
                TB_Sprintf(str, unlocked_trans_format, gText.strings[gText.indices->h[4] + i], gText.strings[gText.indices->h[2] + 1]);
                TB_DrawText(hs_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 9; i++) {
            if (gUnlockedEngines[p][i] && !gPrevUnlockedEngines[p][i]) {
                TB_Sprintf(str, unlocked_engine_format, gText.strings[gText.indices->h[6] + i], gText.strings[gText.indices->h[2] + 3]);
                TB_DrawText(hs_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 5; i++) {
            if (gUnlockedTires[p][i] && !gPrevUnlockedTires[p][i]) {
                TB_Sprintf(str, unlocked_tires_format, gText.strings[gText.indices->h[7] + i], gText.strings[gText.indices->h[2] + 4]);
                TB_DrawText(hs_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 6; i++) {
            if (gUnlockedFrames[p][i] && !gPrevUnlockedFrames[p][i]) {
                TB_Sprintf(str, unlocked_frame_format, gText.strings[gText.indices->h[8] + i], gText.strings[gText.indices->h[2] + 5]);
                TB_DrawText(hs_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
    }
}


/* ---- highscore_animText = GC animText@highscorescr.c (draft) ---- */
typedef struct HsIdx { u8 pad54[54]; u16 tableNames; u8 pad56[54]; u16 scoreNames; } HsIdx;
extern s32 gHighScoreState;
extern char highscore_gCircuitCode[];                        /* gCircuitCode */
extern s8 gTable;                            /* gTable */
extern s8 gNewTotalScore[][5];                       /* gNewTotalScore[table][5] */
extern s32 playerTextColor[];                         /* playerTextColor */
extern char *gScoreNames[];                       /* gScoreNames */
extern char gScoreTimes[][10];                    /* gScoreTimes */
extern s8 gDrawName[];                          /* gDrawName */
extern void HAL_SetTextDepth(f32);
extern u32 TB_Strlen(char *);                /* strlen */
extern s32 sprintf(char *, const char *, ...);
extern void TB_EllipseText(char *, char *, s32);
extern char *gPlayerNameChars;                         /* gPlayerNameChars */
extern s16 highscore_gNameSelect[];                         /* gNameSelect[p] */
extern char gScoreName[][13];                    /* gScoreName[p] */
extern char name_entry_del_label[], name_entry_end_label[], name_entry_space_label[];   /* DrawName statics del$1225 "DEL", end$1226 "END", spc$1227 "SPC" */
/* SaveInit' Ghost: its link and file handle are pointer-sized on the PC (docs/port/CASTS.md). */
typedef struct Ghost { void *ll; void **fileh; u8 track, backward; char name[22]; f32 time; } Ghost;
extern Ghost **gSavedGhost;                       /* gSavedGhost */
extern Ghost **gPakGhosts[];                     /* gPakGhosts[pak] */
extern s8 gGhostPak;                            /* gGhostPak */
extern s8 gGhostMenuConflict;                            /* gGhostMenuConflict */
extern s8 gGhostMenuConfirm;                            /* gGhostMenuConfirm */
extern s8 gGhostMenuNotify;                            /* gGhostMenuNotify */
extern s8 highscore_gYes;                            /* confirm menu selection (two options) */
extern s32 gGhostMenuSelect;                           /* gGhostMenuSelect */
extern s32 gGhostStatus[];                         /* gGhostStatus[pak] */
extern char *gPS_PakName[];                       /* gPS_PakName[pak] ("1".."4") */
extern s32 gJust[2];                        /* TB_SetJust pair */
extern void TB_DrawTextScrollBox(s16, s16, s16, s16, s32, s32, char *);  /* TB_DrawTextScrollBox */

s16 hs_CenterText(char *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}

s16 hs_RightText(char *text, s16 rightX)
{
    return rightX - HAL_GetTextWidth(text, -1);
}

s32 hs_font_set(s32 font);

void hs_SetFont(s32 font)
{
    hs_font_set(font);
}

/* DrawName = GC DrawName(p)@highscorescr.c: name-entry grid for player p (internal; p arrives in s5). */
void DrawName(s32 p)
{
    char ltr[2];
    s32 x;
    s32 y;
    s32 i;

    hs_SetFont(12);
    ltr[1] = 0;
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 4;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
    TB_SetTextColor(1);
    TB_DrawText(hs_CenterText(gText.fixed[150], x), y, gText.fixed[150]);
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 15;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
    TB_SetTextColor(22);
    TB_DrawText(hs_CenterText(gScoreName[p], x), y, gScoreName[p]);
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 30;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 48;
    for (i = 0; i < 54; i++) {
        ltr[0] = gPlayerNameChars[i];
        TB_SetTextColor(i == highscore_gNameSelect[p] ? 22 : 1);
        TB_DrawText(hs_CenterText(ltr, x), y, ltr);
        if ((i + 1) % 9 == 0) {
            y += 10;
            x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 48;
        } else {
            x += 12;
        }
    }
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 36;
    TB_SetTextColor(highscore_gNameSelect[p] == -3 ? 22 : 16);
    TB_DrawText(hs_CenterText(name_entry_space_label, x), y, name_entry_space_label);
    x += 36;
    TB_SetTextColor(highscore_gNameSelect[p] == -2 ? 22 : 16);
    TB_DrawText(hs_CenterText(name_entry_del_label, x), y, name_entry_del_label);
    x += 36;
    TB_SetTextColor(highscore_gNameSelect[p] == -1 ? 22 : 16);
    TB_DrawText(hs_CenterText(name_entry_end_label, x), y, name_entry_end_label);
}

void DrawCircuitCode(void)
{
    s32 x;
    s32 y;
    s32 i;
    s32 length;
    char str[64];

    hs_font_set(11);
    TB_SetTextColor(1);
    TB_DrawText(hs_CenterText(gText.fixed[64], 160), 80, gText.fixed[64]);
    TB_DrawText(hs_CenterText(gText.fixed[65], 160), 120, gText.fixed[65]);
    if (TB_Strlen(highscore_gCircuitCode) > 22) length = 22;
    else length = TB_Strlen(highscore_gCircuitCode);
    str[1] = 0;
    x = 160 - (length + 1) / 2 * 10 / 2;
    y = 95;
    for (i = 0; i < length; i++) {
        str[0] = highscore_gCircuitCode[i];
        TB_DrawText(hs_CenterText(str, x + 5), y, str);
        if ((i + 1) % ((length + 1) / 2) == 0) {
            x = 160 - (length + 1) / 2 * 10 / 2;
            y += 10;
        } else {
            x += 10;
        }
    }
}

void DrawScores(void)
{
    s32 i;
    s32 numWid;
    char str[96];
    char label[256];

    hs_font_set(13);
    TB_SetTextColor(1);
    TB_DrawText(hs_CenterText(gText.strings[((HsIdx *)gText.indices)->tableNames + gTable], 160), 10, gText.strings[((HsIdx *)gText.indices)->tableNames + gTable]);
    for (i = 0; i < 5; i++) {
        sprintf(str, "%d", i + 1);
        hs_font_set(5);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        numWid = HAL_GetTextWidth(str, -1);
        TB_DrawText(16, 76 + i * 24, str);
        hs_font_set(11);
        TB_DrawText(numWid + 16, 78 + i * 24, gText.strings[((HsIdx *)gText.indices)->scoreNames + i]);
    }
    for (i = 0; i < 5; i++) {
        hs_font_set(10);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        TB_EllipseText(label, gScoreNames[i], 160);
        TB_DrawText(69, 80 + i * 24, label);
    }
    for (i = 0; i < 5; i++) {
        hs_font_set(2);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        TB_DrawText(hs_RightText(gScoreTimes[i], 304), 80 + i * 24, gScoreTimes[i]);
    }
}

/* DrawGhost = GC DrawGhost@highscorescr.c (internal; only caller highscore_animText state 4). */
void hs_SetJust(s32 h, s32 v)
{
    gJust[0] = h;
    gJust[1] = v;
}

s32 gh_font_set(s32 font)
{
    s32 old;
    du_gfx_lock();
    old = HAL_SetFontInternal(font);
    osSendMesg(&text_lock_queue, 0, 0);
    return old;
}

void gh_SetFont(s32 font)
{
    gh_font_set(font);
}

void DrawGhost(void)
{
    s32 i;
    s32 y;
    char *str;
    s16 gFreePages;
    char str1[128];
    char time[16];

    gh_SetFont(13);
    TB_SetTextColor(1);
    str = gText.fixed[219];
    TB_DrawText(hs_CenterText(str, 160), 10, str);
    gh_SetFont(11);
    str = gText.fixed[220];
    TB_DrawText(hs_CenterText(str, 160), 55, str);
    gh_SetFont(10);
    y = TB_GetFontLineHeight() + 55;
    str = (*gSavedGhost)->name;
    TB_DrawText(hs_CenterText(str, 160), y, str);
    y += TB_GetFontLineHeight();
    gh_SetFont(2);
    TB_Sprintf(time, "%01d:%02d.%03d", (s32)((*gSavedGhost)->time / 60.0f), (s32)(*gSavedGhost)->time % 60,
                 (s32)((*gSavedGhost)->time * 1000.0f) % 1000);
    TB_DrawText(hs_CenterText(time, 160), y, time);
    if (gGhostMenuConflict == 1) {
        gh_SetFont(10);
        TB_Sprintf(str1, gText.fixed[223], (*gPakGhosts[gGhostPak])->name);
        hs_SetJust(1, 3);
        TB_DrawTextScrollBox(160, 110, 130, TB_GetFontLineHeight() * 6, -1, 0, str1);
        hs_SetJust(0, 3);
    } else if (gGhostMenuConfirm == 1) {
        gh_SetFont(10);
        y = TB_GetFontLineHeight();
        hs_SetJust(1, 3);
        TB_DrawTextScrollBox(160, 110, 130, y * 4, -1, 0, gText.fixed[221]);
        hs_SetJust(0, 3);
        y = TB_GetFontLineHeight() * 2 + 110;
        TB_SetTextColor(highscore_gYes ? 22 : 1);
        str = gText.strings[gText.indices->h[24]];
        TB_DrawText(hs_CenterText(str, 145), y, str);
        TB_SetTextColor(highscore_gYes ? 1 : 22);
        str = gText.strings[gText.indices->h[24] + 1];
        TB_DrawText(hs_CenterText(str, 175), y, str);
    } else if (gGhostMenuNotify == 1) {
        gh_SetFont(10);
        TB_Sprintf(str1, gText.fixed[222], gPS_PakName[gGhostPak]);
        hs_SetJust(1, 3);
        TB_DrawTextScrollBox(160, 110, 130, TB_GetFontLineHeight() * 6, -1, 0, str1);
        hs_SetJust(0, 3);
    } else {
        gh_SetFont(10);
        y = 110;
        for (i = 0; i < 4; i++) {
            TB_SetTextColor(i == gGhostMenuSelect ? 22 : 1);
            time[0] = 0;
            if (i == 1) {
                switch (gGhostStatus[gGhostPak]) {
                case -4:
                    str = gText.fixed[165];
                    break;
                case -3:
                    str = gText.fixed[169];
                    break;
                case -2:
                    str = gText.fixed[166];
                    break;
                case -1:
                    str = gText.fixed[168];
                    break;
                case -5:
                    str = gText.fixed[160];
                    break;
                case 0:
                    str = gText.strings[gText.indices->h[46] + i];
                    break;
                case 1:
                case 2:
                    str = (*gPakGhosts[gGhostPak])->name;
                    TB_Sprintf(time, "%01d:%02d.%03d", (s32)((*gPakGhosts[gGhostPak])->time / 60.0f),
                                 (s32)(*gPakGhosts[gGhostPak])->time % 60,
                                 (s32)((*gPakGhosts[gGhostPak])->time * 1000.0f) % 1000);
                    do {} while (0);
                    break;
                }
                if (gGhostStatus[gGhostPak] == 2) TB_SetTextColor(5);
            } else {
                str = gText.strings[gText.indices->h[46] + i];
            }
            if (i == 2 && gGhostStatus[gGhostPak] != 0 && gGhostStatus[gGhostPak] != 1) TB_SetTextColor(4);
            if (i == 1) {
                TB_DrawText(hs_CenterText(str, 160), y, str);
                y += TB_GetFontLineHeight();
                if (time[0]) {
                    gh_SetFont(2);
                    TB_DrawText(hs_CenterText(time, 160), y, time);
                    gh_SetFont(10);
                }
            } else {
                TB_DrawText(95, y, str);
                if (i == 0) TB_DrawText(hs_RightText(gPS_PakName[gGhostPak], 225), y, gPS_PakName[gGhostPak]);
            }
            TB_SetTextColor(1);
            y += TB_GetFontLineHeight();
        }
    }
}

s32 highscore_animText(s32 arg0)
{
    s32 p;

    HAL_SetTextDepth(0.0f);
    switch (gHighScoreState) {
    case 1:
        DrawUnlocked();
        break;
    case 2:
        DrawCircuitCode();
        break;
    case 3:
        for (p = 0; p < gNumPlayers; p++) {
            if (gDrawName[p] == 1) DrawName(p);
        }
        break;
    case 4:
        DrawGhost();
        break;
    case 5:
        DrawScores();
        break;
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
