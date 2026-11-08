/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* NEAR-MISS (2/917 words, as1 cross-block scheduling only), not a match.
 * carscr_animText = animText@carscr.c (MAT3 reference/repos/mat3/decomp/src/GameSource/src/carscr.c, GC 0x800920EC) with
 * animButtonText@carscr.c (GC 0x800921BC) and the empty drawOptions inlined (their deleted procedures are the caller-less
 * stubs carscr_drawOptions_stub / carscr_animButtonText_stub). Standard ABI (called through the mblit callback), scores standalone.
 * direct typed port. Names: gNumPlayers gNumPlayers, buttonsInited carscr_buttonsInited, gButtonScale carscr_gButtonScale, buttonZ
 * carscr_buttonZ, gNumWindows gNumWindows, gCamera gCamera, buttonInfo carscr_buttonInfo ([4][44] x 64 B; name record 38 = GC 34),
 * gPlayers gPlayers, gRace gRace, gNumGhosts gNumGhosts, gGhosts gGhosts, playerTextColor playerTextColor,
 * flashColors flashColors/C0 (tRGBA by value), gCarLocked gCarLocked, gNumOptions carscr_gNumOptions, gOpts carscr_gOpts, gText
 * gText (text = single, indices = tab, indexed = taba), CarTitles CarTitles, gThisCar gThisCar, per-player
 * byte tables [p+1] rows, gCarTeam gCarTeam, gCarSheen gCarSheen, ZOID_ProjectWindow ZOID_ProjectWindow, HAL_SetFont
 * HAL_SetFont, HAL_SetTextDepth HAL_SetTextDepth, TB_SetJust TB_SetJust, TB_SetTextColor TB_SetTextColor,
 * TB_DrawEllipseText TB_DrawEllipseText, TB_CenterText TB_CenterText, TB_DrawText TB_DrawText, TB_Sprintf TB_Sprintf,
 * TB_GetFontLineHeight TB_GetFontLineHeight, HAL_SetTextTranslucency HAL_SetTextTranslucency, TB_FlashTextColors TB_FlashTextColors,
 * HAL_SetMenuTextDepth HAL_SetMenuTextDepth, TB_Strcpy TB_Strcpy.
 * N64 differences (all from the retail words): no player-file check; screen coordinates unscaled (no /2); options 0..18
 * (19 right-hand records at +19), text tables 2/3 by gNumWindows, cases 7-9/13-14 have no value text; ghost slots use
 * TB_Strcpy of the name or tab[15]; the ghost-less ghost row prints text[224]/[225] by ghosts_enabled; ready string literal
 * "GET READY TO RACE"; ghost time "TIME %01d:%02d.%03d"; label x offset 45*scale*300/buttonZ (mul order), y not converted.
 * Levers found: the `continue` conditions as one || expression (shares one latch copy, as retail); the ?: text pointer kept
 * in `str`; drawOptions(i) in the else arm (its inlined parameter slot is the last retail local word); animText defined
 * BEFORE animButtonText (as in the MAT3 file): the inlined parameter store then wins as1's line-number ties.
 * Own data checked against image A (tools/owncheck2.py): 3 floats 0x803B97BC..C4, jump table 0x803B97C8 (19/19), strings,
 * msgPos .data 0x803B3048.
 * Residual (+0x094/+0x098): retail `li at,1; bne t6,at; sll t7,s3,2` (sll in the delay slot, load stall kept), ours
 * `li at,1; sll t7,s3,2; bne; addu` -- as1's xbb pass (debug as1, AS1DBG=8) MOVEs two instructions of the inlined body's
 * first block (bb 6, sll and addu) into the loop-header block bb 5 (weight 1, stalls 2); retail evidently moved none. Not
 * changed by: line layout or `.loc` values (listing edits), labels or alias directives after the branch, the parameter
 * store position, the loop/if spellings tried. A listing with the IV increment inside the header block reproduces retail's
 * delay slot, i.e. the cause is what as1 sees in the header block, not the inlined body.
 * PC (compiled into the PC build, CMakeLists.txt): no direct callers and no code takes its address. As animText it is
 * a callback of the car-select screen's MultiBlit table (carscr_mb, 38 entries, which initScreen@carscr.c =
 * carscr_InitScreen hands to NewMultiBlit; the table is ROM data): NewMultiBlit calls an AnimFunc once and
 * DrawBlits every frame after, or the sprite pass calls a draw callback every frame, with the blit. The
 * views are cleanup_colortrace's (the carscr.c group), which have their PC layouts: the input records 80 bytes with
 * the profile handle at +0x48, the text bank's string tables at +8, +0x18 and +0x20, and the ghost records' pointer
 * fields host-sized. The inlined helpers' deleted bodies are nonmatching/ovl_a/stubs.c's empty functions. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Color4 { u32 word; } Color4;
typedef struct FileInfo FileInfo;
typedef struct Profile Profile;
struct Profile { Profile **next, **previous; FileInfo **file; u32 id0, id1; u8 name[24]; u8 **stats; };
typedef struct Player { u32 held, pressed, unknown08, repeated; u8 unknown10[50]; u8 paint, unknown43[5]; Profile **owner; } Player;
typedef struct Record Record;
/* PC: as cleanup_colortrace's Record: unknown24 is pointer-sized. */
struct Record { Record **next; FileInfo **file; s8 track; u8 mode, name[14]; u32 id0, id1; f32 time; void *unknown24; void **active; };
typedef struct Language { s32 unknown0; u8 **text; s32 unknown8; u16 *indices; u8 **indexed; } Language;
typedef struct Camera { f32 uv[3][3], position[3]; u8 unknown30[104]; } Camera;
typedef struct Slot { s32 kind, handle; f32 angle, matrix[3][3], position[3]; u8 alpha, unknown3D[3]; } Slot;
typedef struct Blit Blit;

extern Language gText;
extern s16 gNumPlayers, gNumWindows;
extern s32 gRace;
extern Player gPlayers[];
extern Camera gCamera[];
extern Slot carscr_buttonInfo[][44];
extern f32 carscr_gButtonScale, carscr_buttonZ;
extern s8 gCarLocked[], gNumGhosts, gThisCar[], gCarTeam[];
extern s32 carscr_buttonsInited[], carscr_gNumOptions[], carscr_gOpts[][19], ghosts_enabled, playerTextColor[];
extern s16 gCarSheen[];
extern Record **gGhosts[4];
extern Color4 flashColors, flashColors_at_1;
extern u8 *CarTitles[];
extern s8 gCarTrans[][13], gCarHandling[][13], gCarEngines[][13], gCarTires[][13], gCarFrame[][13], gCarWings[][13];
extern s8 gTireRims[][13], gCarHorn[][13];
extern void HAL_SetTextDepth(f32);
extern void *HAL_SetFont(s32);
extern void TB_SetTextColor(s32);
extern void TB_SetJust(s32, s32);
extern void ZOID_ProjectWindow(s32, f32 *, Camera *, void *, s16 *);
extern s16 TB_CenterText(void *, s16);
extern s16 TB_GetFontLineHeight(void);
extern void TB_DrawText(s16, s16, void *);
extern void TB_Sprintf(u8 *, char *, ...);
extern void TB_DrawEllipseText(s16, s16, s16, void *);
extern void HAL_SetTextTranslucency(f32);
extern void HAL_SetMenuTextDepth(f32);
extern void TB_FlashTextColors(Color4, Color4);
extern u8 *TB_Strcpy(u8 *, u8 *);

static void animButtonText(s32 p);
static void drawOptions(s32 p);

s32 carscr_animText(Blit *blt)
{
    s32 i;
    for (i = 0; i < gNumPlayers; i++) {
        if (carscr_buttonsInited[i] == 1) animButtonText(i);
        else drawOptions(i);
    }
    return 1;
}

static void animButtonText(s32 p)
{
    s32 i;
    s32 x;
    s32 y;
    s32 w;
    u8 *str;
    u8 str1[64];
    s16 screenPos[2];
    f32 gTextPosScale = carscr_gButtonScale;
    if (gNumWindows > 1) gTextPosScale *= 0.5f;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    if (gNumWindows == 1) { w = 140; HAL_SetFont(13); }
    else { w = 69; HAL_SetFont(12); }
    ZOID_ProjectWindow(p, carscr_buttonInfo[p][38].position, &gCamera[p], 0, screenPos);
    x = screenPos[0];
    y = screenPos[1];
    str = (*gPlayers[p].owner)->name;
    TB_SetTextColor(1);
    TB_DrawEllipseText(x, y, w, str);
    TB_SetJust(0, 1);
    if (gRace == 2) {
        s32 ghostIdx;
        for (ghostIdx = 1; ghostIdx < gNumGhosts + 1; ghostIdx++) {
            f32 pos[3];
            ZOID_ProjectWindow(ghostIdx, carscr_buttonInfo[0][38].position, &gCamera[ghostIdx], 0, screenPos);
            x = screenPos[0];
            y = screenPos[1];
            TB_Sprintf(str1, "GHOST %d", ghostIdx);
            TB_SetTextColor(1);
            TB_DrawText(TB_CenterText(str1, x), y, str1);
            pos[0] = carscr_buttonInfo[0][38].position[0];
            pos[1] = carscr_buttonInfo[0][38].position[1];
            pos[2] = carscr_buttonInfo[0][38].position[2];
            pos[1] = -pos[1] / 2.0f;
            ZOID_ProjectWindow(ghostIdx, pos, &gCamera[ghostIdx], 0, screenPos);
            x = screenPos[0];
            y = screenPos[1];
            if (!gGhosts[ghostIdx]) {
                str = !ghosts_enabled ? gText.text[225] : gText.text[224];
                TB_SetTextColor(1);
                TB_DrawText(TB_CenterText(str, x), y, str);
            } else {
                str = (*gGhosts[ghostIdx])->name;
                TB_SetTextColor(playerTextColor[ghostIdx]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
                TB_Sprintf(str1, "TIME %01d:%02d.%03d", (s32)(*gGhosts[ghostIdx])->time / 60,
                             (s32)(*gGhosts[ghostIdx])->time % 60, (s32)(1000.0f * (*gGhosts[ghostIdx])->time) % 1000);
                TB_SetTextColor(1);
                TB_DrawText(TB_CenterText(str1, x), y, str1);
            }
        }
    }
    if (gCarLocked[p]) {
        s32 notReadyCount;
        s32 notReadyIdx;
        s32 pIdx;
        f32 msgPos[3] = {0.0f, -120.0f, 300.0f};
        ZOID_ProjectWindow(p, msgPos, &gCamera[p], 0, screenPos);
        x = screenPos[0];
        y = screenPos[1] - TB_GetFontLineHeight();
        notReadyCount = 0;
        for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
            if (p != pIdx && !gCarLocked[pIdx]) { notReadyIdx = pIdx; notReadyCount++; }
        }
        if (!notReadyCount) {
            y += TB_GetFontLineHeight();
            str = (u8 *)"GET READY TO RACE";
            TB_SetTextColor(1);
        } else {
            str = gText.text[151];
            TB_SetTextColor(1);
            TB_DrawText(TB_CenterText(str, x), y, str);
            y += TB_GetFontLineHeight();
            if (notReadyCount == 1) {
                TB_SetTextColor(playerTextColor[notReadyIdx]);
                str = (*gPlayers[notReadyIdx].owner)->name;
            } else str = gText.text[31];
        }
        TB_DrawText(TB_CenterText(str, x), y, str);
        TB_SetJust(0, 3);
        HAL_SetTextTranslucency(-1.0f);
        return;
    }
    if (gNumWindows == 1) HAL_SetFont(11); else HAL_SetFont(12);
    for (i = 0; i < carscr_gNumOptions[p]; i++) {
        s32 item = carscr_gOpts[p][i];
        if ((carscr_buttonInfo[p][item].angle > 0.6981317f && carscr_buttonInfo[p][item].angle < 2.443461f) || !carscr_buttonInfo[p][item].alpha) continue;
        HAL_SetTextTranslucency(carscr_buttonInfo[p][item].alpha);
        if (carscr_buttonInfo[p][item].angle < 1.5707964f) {
            TB_FlashTextColors(flashColors, flashColors_at_1);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors(flashColors, flashColors_at_1);
            TB_SetTextColor(22);
        }
        HAL_SetMenuTextDepth(carscr_buttonInfo[p][item].position[2] - 10.0f);
        ZOID_ProjectWindow(p, carscr_buttonInfo[p][item].position, &gCamera[p], 0, screenPos);
        x = screenPos[0] - 45.0f * gTextPosScale * 300.0f / carscr_buttonZ;
        y = screenPos[1];
        str = gNumWindows == 1 ? gText.indexed[gText.indices[2] + item]
                              : gText.indexed[gText.indices[3] + item];
        TB_DrawText(x, y, str);
        switch (item) {
        case 0: str = CarTitles[gThisCar[p]]; break;
        case 1: str = gText.indexed[gText.indices[4] + gCarTrans[p+1][gThisCar[p]]]; break;
        case 2: str = gText.indexed[gText.indices[5] + gCarHandling[p+1][gThisCar[p]]]; break;
        case 3: str = gText.indexed[gText.indices[6] + gCarEngines[p+1][gThisCar[p]]]; break;
        case 4: str = gText.indexed[gText.indices[7] + gCarTires[p+1][gThisCar[p]]]; break;
        case 5: str = gText.indexed[gText.indices[8] + gCarFrame[p+1][gThisCar[p]]]; break;
        case 6: str = gText.indexed[gText.indices[9] + gCarWings[p+1][gThisCar[p]]]; break;
        case 10: str = gText.indexed[gText.indices[12] + gCarTeam[p]]; break;
        case 11: str = gText.indexed[gText.indices[10] + gTireRims[p+1][gThisCar[p]]]; break;
        case 12: str = gText.indexed[gText.indices[11] + gCarHorn[p+1][gThisCar[p]]]; break;
        case 15: TB_Sprintf(str1, "%d", gCarSheen[p] + 1); str = str1; break;
        case 16:
            if (gGhosts[1]) TB_Strcpy(str1, (*gGhosts[1])->name);
            else TB_Strcpy(str1, gText.indexed[gText.indices[15]]);
            str = str1;
            break;
        case 17:
            if (gGhosts[2]) TB_Strcpy(str1, (*gGhosts[2])->name);
            else TB_Strcpy(str1, gText.indexed[gText.indices[15]]);
            str = str1;
            break;
        case 18:
            if (gGhosts[3]) TB_Strcpy(str1, (*gGhosts[3])->name);
            else TB_Strcpy(str1, gText.indexed[gText.indices[15]]);
            str = str1;
            break;
        default: str = 0; break;
        }
        if (str) {
            ZOID_ProjectWindow(p, carscr_buttonInfo[p][item+19].position, &gCamera[p], 0, screenPos);
            x = screenPos[0] - 50.0f * gTextPosScale * 300.0f / carscr_buttonZ;
            y = screenPos[1];
            TB_DrawText(x, y, str);
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
}

static void drawOptions(s32 p)
{
}

