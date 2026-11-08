/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: recordscr_drawSelect (MAT3 recordscr drawSelect) is now a claimed member; the eleven
 * old members are unchanged and still MATCH; the recordscr_animText root stays context (improved: drawView + nested drawStuntTotals). */
/* Image-A aggregate statistics ABEC8: separate race/track/record pointers and
 * scan/display counters follow native lifetimes. The scaled quotient is a
 * shared expression; useful locals retain native declaration order. Buffer
 * text[48] is unchanged. Stock frame152, native text at sp+72 and x/y at
 * sp+146/sp+144. No new padding, unused local, forced register, volatile,
 * helper or call convention. All ten inherited members are unchanged. */
/* Image-A per-track statistics AD0D4: preserve the native zero-initialized
 * average and conditional division, unsigned-halfword bit-count arguments,
 * and local declaration order (x/y at sp+132/128, text at sp+76).
 * Its real text buffer remains 48 bytes; frame remains 144 bytes. No unused
 * local or artificial access is introduced. Original spelling is unknown.
 * All nine earlier exact statistics members remain unchanged and claimed. */
/* Image-A cumulative statistics ACA94: row-count initialization follows the
 * two native setup calls; bit masks retain unsigned-halfword narrowing.
 * Existing declaration order recovers native text/x/y homes without changing
 * the real 48-byte text buffer. One disclosed unused s32 precedes all locals:
 * stock UGEN M88 -> M92, T20/P4 unchanged, aligned frame120 ->128.
 * Without it x/y/text homes are four bytes low. Original local identity is
 * unknown; this is the exact frame residual, not inferred behavior.
 * All eight earlier exact bodies remain unchanged and claimed. */
/* Image-A unlock panel AB848: all six unsigned maximum-result arms retain
 * signed-halfword narrowing. Flag initialization follows profile restoration;
 * the first scan exits early for locked entries. The existing first flag
 * precedes saved_owner in the declaration order. The real 44-byte buffer is
 * safely above the 24-byte maximum across all six authentic languages;
 * original capacity and source types remain unknown. Profile setup writes
 * its pointer before count. No artificial local or helper/signature change.
 * Seven prior exact members from draft stay claimed without new credit. */
/* Image-A extra-car panel AB5FC: two unsigned maximum-result arms retain
 * native signed-halfword narrowing, and profile-pointer setup precedes the
 * temporary one-player count. No signature, local capacity, helper body,
 * private call convention or other function is changed by this continuation.
 * Six prior exact candidates from draft remain in their original group.
 * Their inherited numeric buffers are 40 bytes, safely above the 12/10/6-byte
 * write bounds for AC54C/AC7F4/ACD5C. Capacities and original types are inferred.
 * Only the genuine AD57C root is kept. No ROM or accepted-image claim. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Language { s32 unknown0; u8 **text; s32 unknown8; u16 *indices; u8 **indexed; } Language;
typedef struct Color4 { u32 word; } Color4;
typedef struct Slot { s32 kind,handle; f32 angle,matrix[3][3],position[3]; u8 alpha,unknown3D[3]; } Slot;
typedef struct Camera { f32 uv[3][3],position[3]; u8 unknown30[104]; } Camera;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct FileInfo { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 controller; } FileInfo;
typedef struct Profile Profile;
struct Profile { Profile **next,**previous; FileInfo **file; u8 unknown0C[8]; u8 name[24]; u8 **stats; };
typedef struct Player { u8 unknown00[72]; Profile **owner; } Player;
typedef struct FiveParts FiveParts;
typedef struct TextBox {s16 x,y,width,height;} TextBox;
extern Language gText;
extern Slot recordscr_buttonInfo[];
extern Camera gCamera[];
extern Color4 flashColors,flashColors_at_1;
extern Player gPlayers[];
extern s16 gNumPlayers;
extern Profile **gStatPlayer,**gPlayerFiles_head;
extern s32 gRecordState,recordscr_gSelectMenuSelect,gViewStatType[],recordscr_gNumPakPlayers[],gTrackType;
extern s8 gDeleteConfirm,recordscr_gYes,gDeleteNotify,gRecordTrack,gRecordCircuit,recordscr_gSelectPakIdx,gOptions_metric;
extern u8 gDeleteName[];
extern f32 recordscr_gButtonScale,recordscr_buttonZ,gDefTime;
extern TextBox gScreenRect;
extern FiveParts *recordscr_viewPane;
extern u8 *gPS_PakName[],*gTrackTitles[],*CarTitles[];
extern u8 gTotalStuntStats[],gTotalRaceStats[],gTotalBattleStats[],gTotalObstacleStats[],gTotalCircuitStats[];
extern s8 gUnlockedTracks[],gUnlockedCircuits[],gUnlockedCars[],gUnlockedEngines[],gUnlockedFrames[],gUnlockedTrans[],gUnlockedTires[];
extern void HAL_SetTextDepth(f32);
extern s32 HAL_SetFont(s32);
extern void TB_SetTextColor(s32);
extern void TB_SetJust(u32,u32);
extern void ZOID_ProjectWindow(s32,f32 *,Camera *,void *,s16 *);
extern s16 TB_CenterText(void *,s16);
extern s16 TB_RightText(void *,s16);
extern s16 TB_GetFontLineHeight(void);
extern s32 HAL_GetTextWidth(u8 *,s16);
extern void TB_DrawText(s16,s16,void *);
extern void TB_Sprintf(u8 *,const u8 *,...);
extern void HAL_SetTextTranslucency(f32);
extern void TB_FlashTextColors(Color4,Color4);
extern void HAL_SetMenuTextDepth(f32);
extern void HAL_SetFontSpacing(s32);
extern void TB_DrawTextScrollBox(s16,s16,s16,s16,s16,s16,u8 *);
extern s16 TB_GetTextNumLines(s16,u8 *);
extern void TB_DrawEllipseText(s16,s16,s16,void *);
extern void ChangePaneSize(FiveParts *,s32,s32,s32,s32);
extern u8 *TB_Strcpy(u8 *,u8 *);
extern void CalcUnlockedStuff(void);
extern s32 CountBits(u32,u8);
#define ABS(x) ((x)<0 ? -(x) : (x))
#define U16_AT(p,o) (*(u16 *)((u8 *)(p)+(o)))
#define U32_AT(p,o) (*(u32 *)((u8 *)(p)+(o)))
#define F32_AT(p,o) (*(f32 *)((u8 *)(p)+(o)))
#define NAME(g,i) (gText.indexed[gText.indices[g]+(i)])
#define PROFILE_DATA (*(*gStatPlayer)->stats)
extern u8 recordscr_no_time_text[],recordscr_time_format[],recordscr_battle_matches_format[],recordscr_battle_wins_format[],recordscr_battle_kills_format[],recordscr_battle_deaths_format[],recordscr_stunt_best_format[],recordscr_stunt_best_average_format[],recordscr_stunt_points_format[],recordscr_stunt_time_format[],recordscr_stunt_crashes_format[],recordscr_stunt_count_format[],recordscr_stunt_coins_a_format[],recordscr_stunt_coins_b_format[],recordscr_stunt_kind_label_format[],recordscr_stunt_kind_count_format[],recordscr_obstacle_races_format[],recordscr_obstacle_finished_format[],recordscr_obstacle_crashes_format[],recordscr_obstacle_death_attempted_format[],recordscr_obstacle_death_completed_format[],recordscr_circuit_count_format[],recordscr_circuit_first_format[],recordscr_circuit_second_format[],recordscr_circuit_third_format[],recordscr_circuit_death_attempted_format[],recordscr_circuit_death_completed_format[],recordscr_circuit_best_points_format[],recordscr_circuit_total_points_format[],recordscr_total_km_format[],recordscr_total_miles_format[],recordscr_total_stunt_points_format[],recordscr_total_battle_points_format[],recordscr_silver_race_coins_format[],recordscr_gold_race_coins_format[],recordscr_silver_stunt_coins_format[],recordscr_gold_stunt_coins_format[],recordscr_unlocked_circuit_format[],recordscr_race_laps_format[],recordscr_race_count_format[],recordscr_race_first_format[],recordscr_race_second_format[],recordscr_race_third_format[],recordscr_race_crashes_format[],recordscr_race_death_attempted_format[],recordscr_race_death_completed_format[],recordscr_race_ghosts_format[],recordscr_race_distance_format[],recordscr_race_coins_a_format[],recordscr_race_coins_b_format[],recordscr_circuit_title_format[],recordscr_track_variant_title_format[];

/* Every genuine caller writes zero in outgoing word +8. Its original role is
 * unknown; the observed specialized body does not read it. */
void getScoreStr(u8 *destination,f32 time,s32 unused_format)
{
    if(time==0.0f)TB_Strcpy(destination,recordscr_no_time_text);
    else {
        gDefTime=time;
        TB_Sprintf(destination,recordscr_time_format,(u32)(time/60.0f)%100,(u32)time%60,(u32)(time*1000.0f)%1000);
    }
}
/* : recordscr_drawSelect = MAT3 recordscr.c drawSelect (GC 0x8008126C, 0xA0C B), called by the animText root
 * recordscr_animText in kRecordStateSelect.  Direct typed port of the MAT3 body; function-level locals and ORDER from the GC
 * DWARF (i, x, y, cx, str, str1[64], screenPos[2]: str1 is 64 bytes as in GC, which fixes 's 192-vs-176 frame),
 * GC's block-scope `player`/`playerIdx` kept in the player-list arm; GC's dead block-scope tRGBA c1/c2 pairs are NOT
 * present on the N64 (with them the frame is 208).
 * N64 differences (from the retail displacements/constants): screenPos is not halved; gText indices single[47/143/144/
 * 146/147], tab[24]/tab[50] (GC 25/52); no trailing TB_SetJust(0, 3) after the confirm branch or the button loop; the
 * pak-select row draws taba[tab[50] + 1] then gPS_PakName[gSelectPakIdx] at cx (GC: "CHOOSE PLAYER" + sprintf);
 * row 2 picks the pak-status text with a switch on gNumPakPlayers[gSelectPakIdx] (case order -4, -3, 0, -2, -1 =
 * single[165/169/148/166/168], from the jump table at 0x803B98B8) where GC has single[158]; the gStatPlayer list
 * loop tests only `player` (no `*player`), with `player = gStatPlayer` assigned before the `if (!player)` test.
 * Shapes taken from retail: `str = taba[tab[50] + 1]; TB_DrawText(x, y, str)` (evaluation before the narrowing),
 * the list step written after the loop body (as1 line tie-break puts `i - sel` before `i < sel`).
 * Names: ZOID_ProjectWindow = ZOID_ProjectWindow, TB_SetJust = TB_SetJust, TB_SetTextColor = TB_SetTextColor,
 * HAL_SetFont = HAL_SetFont, TB_CenterText = TB_CenterText, TB_RightText = TB_RightText,
 * TB_DrawText = TB_DrawText, TB_GetFontLineHeight = TB_GetFontLineHeight, TB_GetTextNumLines = TB_GetTextNumLines,
 * ChangePaneSize = ChangePaneSize, TB_DrawTextScrollBox = TB_DrawTextScrollBox, HAL_SetTextTranslucency = HAL_SetTextTranslucency,
 * TB_FlashTextColors = TB_FlashTextColors, HAL_GetTextWidth = HAL_GetTextWidth, HAL_SetFontSpacing = HAL_SetFontSpacing,
 * TB_DrawEllipseText = TB_DrawEllipseText, TB_Sprintf = TB_Sprintf; gScreenRect = gScreenRect, gDeleteConfirm/854/850 =
 * gDeleteConfirm/gDeleteNotify/gYes, gStatPlayer = gStatPlayer, gDeleteName = gDeleteName, recordscr_gSelectMenuSelect = gSelectMenuSelect,
 * recordscr_gSelectPakIdx = gSelectPakIdx, recordscr_gNumPakPlayers = gNumPakPlayers, gPlayerFiles_head = gPlayerFiles.head.
 * SHAPING (disclosed residual): `pad` is an unused s32 declared before i.  Frame 176 with or without it (8-byte
 * rounding), every local home is already retail's; it makes the cfe local area 108 instead of 104, which moves uopt's
 * spill homes down one slot so the loop's spilled &buttonInfo[i].uvs[3] lands at retail's sp+52 instead of sp+56
 * (6 words).  An extra 4-byte block-scope local anywhere gives the identical object.  No pad arrays.
 */
void recordscr_drawSelect(void)
{
    s32 pad;
    s32 i, x, y, cx;
    u8 *str;
    u8 str1[64];
    s16 screenPos[2];
    TB_SetJust(0, 1);
    ZOID_ProjectWindow(0, recordscr_buttonInfo[17].position, &gCamera[0], 0, screenPos);
    cx = screenPos[0];
    y = screenPos[1];
    HAL_SetMenuTextDepth(recordscr_buttonInfo[17].position[2] - 10.0f);
    TB_SetTextColor(1);
    HAL_SetFont(13);
    TB_SetTextColor(1);
    str = gText.text[47];
    TB_DrawText(TB_CenterText(str, cx), y, str);
    y += TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    if (gDeleteConfirm == 1) {
        HAL_SetTextDepth(0.0f);
        y += TB_GetFontLineHeight();
        if (gStatPlayer) {
            TB_Sprintf(str1, gText.text[143], gDeleteName);
            str = str1;
        } else str = gText.text[144];
        ChangePaneSize(recordscr_viewPane, cx - gScreenRect.width / 2 - 4, y - 4,
                         cx + gScreenRect.width / 2 + 4, 4 + y + ((TB_GetTextNumLines(gScreenRect.width, str) + 2) * TB_GetFontLineHeight()));
        TB_SetTextColor(1);
        TB_SetJust(1, 3);
        TB_DrawTextScrollBox(cx, y, gScreenRect.width, gScreenRect.height, -1, 0, str);
        TB_SetJust(0, 3);
        y += TB_GetFontLineHeight() * (TB_GetTextNumLines(gScreenRect.width, str) + 1);
        str = NAME(24, 0);
        TB_SetTextColor(recordscr_gYes == 1 ? 22 : 1);
        TB_DrawText(TB_RightText(str, cx - 5), y, str);
        str = NAME(24, 1);
        TB_SetTextColor(recordscr_gYes == 0 ? 22 : 1);
        TB_DrawText(cx + 5, y, str);
    } else if (gDeleteNotify == 1) {
        HAL_SetTextDepth(0.0f);
        y += TB_GetFontLineHeight();
        if (gStatPlayer) {
            TB_Sprintf(str1, gText.text[146], gDeleteName);
            str = str1;
        } else str = gText.text[147];
        ChangePaneSize(recordscr_viewPane, cx - gScreenRect.width / 2 - 4, y - 4,
                         cx + gScreenRect.width / 2 + 4, 4 + y + (TB_GetTextNumLines(gScreenRect.width, str) * TB_GetFontLineHeight()));
        TB_SetTextColor(1);
        TB_SetJust(1, 3);
        TB_DrawTextScrollBox(cx, y, gScreenRect.width, gScreenRect.height, -1, 0, str);
        TB_SetJust(0, 3);
    } else {
        for (i = 0; i < 11; i++) {
            ChangePaneSize(recordscr_viewPane, -20, 0, -10, 10);
            HAL_SetMenuTextDepth(recordscr_buttonInfo[i].position[2] - 10.0f);
            ZOID_ProjectWindow(0, recordscr_buttonInfo[i].position, &gCamera[0], 0, screenPos);
            if (recordscr_buttonInfo[i].kind == 11) x = screenPos[0];
            else if (recordscr_buttonInfo[i].kind == 9) x = screenPos[0] - 65.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            else x = screenPos[0] - 45.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            y = screenPos[1];
            if (i == 1) {
                ZOID_ProjectWindow(0, recordscr_buttonInfo[14].position, &gCamera[0], 0, screenPos);
                cx = screenPos[0] - 25.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            } else {
                ZOID_ProjectWindow(0, recordscr_buttonInfo[13].position, &gCamera[0], 0, screenPos);
                cx = screenPos[0] - 45.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            }
            if (i >= 3 && i < 11 && i == recordscr_gSelectMenuSelect &&
                (recordscr_buttonInfo[13].angle < 0.6981317f || recordscr_buttonInfo[13].angle > 2.443461f) && recordscr_buttonInfo[13].alpha) {
                HAL_SetTextTranslucency(recordscr_buttonInfo[13].alpha);
                if (recordscr_buttonInfo[13].angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(1);
                } else {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(22);
                }
                str = NAME(50, 2);
                HAL_SetFont(11);
                TB_DrawText(cx, y, str);
                HAL_SetFont(10);
            }
            if (recordscr_buttonInfo[i].angle > 0.6981317f && recordscr_buttonInfo[i].angle < 2.443461f) continue;
            if (!recordscr_buttonInfo[i].alpha) continue;
            HAL_SetTextTranslucency(recordscr_buttonInfo[i].alpha);
            if (recordscr_buttonInfo[i].angle < 1.5707964f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(22);
            }
            if (i == 0) {
                str = NAME(50, 0);
                TB_DrawText(x, y, str);
            } else if (i == 1) {
                str = NAME(50, 1);
                TB_DrawText(x, y, str);
                str = gPS_PakName[recordscr_gSelectPakIdx];
                TB_DrawText(cx, y, str);
            } else if (i == 2) {
                switch (recordscr_gNumPakPlayers[recordscr_gSelectPakIdx]) {
                case -4: str = gText.text[165]; break;
                case -3: str = gText.text[169]; break;
                case 0: str = gText.text[148]; break;
                case -2: str = gText.text[166]; break;
                case -1: str = gText.text[168]; break;
                }
                TB_SetTextColor(0);
                TB_DrawText(TB_CenterText(str, x), y, str);
            } else if (i >= 3 && i < 11) {
                Profile **player;
                s32 playerIdx;
                player = gStatPlayer;
                if (!player) {
                    for (playerIdx = 0, player = gPlayerFiles_head; player; player = (*player)->next) {
                        if ((*(*player)->file)->controller == recordscr_gSelectPakIdx) {
                            if (playerIdx == i - 3) str = (*player)->name;
                            playerIdx++;
                        }
                    }
                } else {
                    for (playerIdx = 0; player; ) {
                        if ((*(*player)->file)->controller == recordscr_gSelectPakIdx) {
                            if (playerIdx == (i - recordscr_gSelectMenuSelect >= 0 ? i - recordscr_gSelectMenuSelect : -(i - recordscr_gSelectMenuSelect))) str = (*player)->name;
                            playerIdx++;
                        }
                        player = i < recordscr_gSelectMenuSelect ? (*player)->previous : (*player)->next;
                    }
                }
                if ((u32)HAL_GetTextWidth(str, -1) > 140) HAL_SetFontSpacing(0);
                TB_DrawEllipseText(x, y, 98, str);
                HAL_SetFontSpacing(-1);
            }
            TB_GetFontLineHeight();
        }
    }
}
void drawUnlockedParts(void)
{
    Profile **saved_owner;
    s16 saved_players,i,y,bottom;
    s32 all_empty=1,empty;
    u8 *label;
    if(!gStatPlayer)return;
    TB_SetJust(0,3);
    saved_owner=gPlayers[0].owner;saved_players=gNumPlayers;
    gNumPlayers=1;gPlayers[0].owner=gStatPlayer;
    CalcUnlockedStuff();
    gPlayers[0].owner=saved_owner;gNumPlayers=saved_players;
    TB_SetTextColor(1);
    HAL_SetFont(11);
    y=90;
    label=NAME(34,0);
    TB_DrawText(TB_CenterText(label,80),y,label);
    y+=TB_GetFontLineHeight();HAL_SetFont(12);empty=1;
    for(i=3;i<9;i++)if(gUnlockedEngines[i]) {
        empty=0;label=NAME(6,i);
        TB_DrawText(TB_CenterText(label,80),y,label);y+=TB_GetFontLineHeight();
    }
    if(empty==1) {
        label=NAME(15,0);
        TB_DrawText(TB_CenterText(label,80),y,label);y+=TB_GetFontLineHeight();
    } else all_empty=0;
    y+=TB_GetFontLineHeight();
    HAL_SetFont(11);
    label=NAME(34,1);
    TB_DrawText(TB_CenterText(label,80),y,label);
    y+=TB_GetFontLineHeight();HAL_SetFont(12);empty=1;
    for(i=3;i<6;i++)if(gUnlockedFrames[i]) {
        empty=0;label=NAME(8,i);
        TB_DrawText(TB_CenterText(label,80),y,label);y+=TB_GetFontLineHeight();
    }
    if(empty==1) {
        label=NAME(15,0);
        TB_DrawText(TB_CenterText(label,80),y,label);y+=TB_GetFontLineHeight();
    } else all_empty=0;
    bottom=y+4;
    HAL_SetFont(11);
    y=90;
    label=NAME(34,2);
    TB_DrawText(TB_CenterText(label,240),y,label);
    y+=TB_GetFontLineHeight();HAL_SetFont(12);empty=1;
    for(i=2;i<8;i++)if(gUnlockedTrans[i]) {
        empty=0;label=NAME(4,i);
        TB_DrawText(TB_CenterText(label,240),y,label);y+=TB_GetFontLineHeight();
    }
    if(empty==1) {
        label=NAME(15,0);
        TB_DrawText(TB_CenterText(label,240),y,label);y+=TB_GetFontLineHeight();
    } else all_empty=0;
    y+=TB_GetFontLineHeight();
    HAL_SetFont(11);
    label=NAME(34,3);
    TB_DrawText(TB_CenterText(label,240),y,label);
    y+=TB_GetFontLineHeight();HAL_SetFont(12);empty=1;
    for(i=1;i<5;i++)if(gUnlockedTires[i]) {
        empty=0;label=NAME(7,i);
        TB_DrawText(TB_CenterText(label,240),y,label);y+=TB_GetFontLineHeight();
    }
    if(empty==1) {
        label=NAME(15,0);
        TB_DrawText(TB_CenterText(label,240),y,label);y+=TB_GetFontLineHeight();
    } else all_empty=0;
    if(all_empty) {
        y+=TB_GetFontLineHeight();
        TB_DrawText(TB_CenterText(gText.text[62],160),y,gText.text[62]);
        y+=TB_GetFontLineHeight();
    }
    if(y+4>=bottom)bottom=y+4;
    ChangePaneSize(recordscr_viewPane,12,86,308,bottom);
}
void drawUnlockedTracks(void)
{
    s32 first;
    Profile **saved_owner;
    s16 saved_players,i,x,y,bottom,width=20;
    s32 second;
    u8 text[44];
    if(!gStatPlayer)return;
    TB_SetJust(0,3);
    saved_owner=gPlayers[0].owner;saved_players=gNumPlayers;
    gPlayers[0].owner=gStatPlayer;gNumPlayers=1;
    CalcUnlockedStuff();
    gPlayers[0].owner=saved_owner;gNumPlayers=saved_players;
    first=0;second=0;
    for(i=0;i<19;i++) {
        if(i==0 || i==1 || i==2 || i==3 || i==14 || i==6 || i==7 || i==8 || i==9)continue;
        if(!gUnlockedTracks[i])continue; if(i<14)first=1;else second=1;
    }
    for(i=1;i<4;i++)if(gUnlockedCircuits[i])second=1;
    HAL_SetFont(12);TB_SetTextColor(1);
    y=90;x=first && second ? 80 : 160;bottom=90;
    for(i=0;i<19;i++) {
        if(i==14) {x=first && second ? 240 : 160;if(first && second)y=90;}
        if(i==0 || i==1 || i==2 || i==3 || i==14 || i==6 || i==7 || i==8 || i==9)continue;
        if(gUnlockedTracks[i]) {
            width=(u32)HAL_GetTextWidth(gTrackTitles[i],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gTrackTitles[i],-1);
            TB_DrawText(TB_CenterText(gTrackTitles[i],x),y,gTrackTitles[i]);
            y+=TB_GetFontLineHeight();
        }
        if(y>=bottom)bottom=y;
    }
    for(i=1;i<4;i++)if(gUnlockedCircuits[i]) {
        TB_Sprintf(text,recordscr_unlocked_circuit_format,NAME(44,i),gText.text[42]);
        width=(u32)HAL_GetTextWidth(text,-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(text,-1);
        TB_DrawText(TB_CenterText(text,x),y,text);y+=TB_GetFontLineHeight();
    }
    if(!first && !second) {
        width=(u32)HAL_GetTextWidth(gText.text[57],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.text[57],-1);
        TB_DrawText(TB_CenterText(gText.text[57],160),y,gText.text[57]);y+=TB_GetFontLineHeight();
        width=(u32)HAL_GetTextWidth(gText.text[58],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.text[58],-1);
        TB_DrawText(TB_CenterText(gText.text[58],160),y,gText.text[58]);y+=TB_GetFontLineHeight();
        width=(u32)HAL_GetTextWidth(gText.text[59],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.text[59],-1);
        TB_DrawText(TB_CenterText(gText.text[59],160),y,gText.text[59]);y+=TB_GetFontLineHeight();
        width=(u32)HAL_GetTextWidth(gText.text[60],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.text[60],-1);
        TB_DrawText(TB_CenterText(gText.text[60],160),y,gText.text[60]);y+=TB_GetFontLineHeight();
    } else if(first && second)width+=160;
    if(y>=bottom)bottom=y;
    ChangePaneSize(recordscr_viewPane,152-width/2,86,168+width/2,bottom+4);
}
void getScoreStr(u8 *,f32,s32);
void drawRaceStats(void)
{
    s32 track,i,x,y;
    u8 *label,text[48],*data;
    u32 distance;
    f32 average;
    TB_SetJust(0,3);
    track=gRecordTrack/2;
    if(gRecordTrack&1)track+=6;
    if(gStatPlayer)data=PROFILE_DATA+140+track*96;
    else data=gTotalRaceStats+track*96;
    distance=U32_AT(data,88);
    if(gOptions_metric)distance=distance*10/6;
    HAL_SetFont(11);
    ChangePaneSize(recordscr_viewPane,12,86,308,94+TB_GetFontLineHeight()*8);
    TB_SetTextColor(1);
    for(i=0;i<14;i++) {
        if(i==0) {x=100;y=90;}
        else if(i==3) {x=110;y+=TB_GetFontLineHeight();}
        else if(i==7) {x=270;y=90+TB_GetFontLineHeight()*4;}
        else if(i==10) {x=260;y=90;continue;}
        else if((i==12 || i==13) && !gStatPlayer)continue;
        label=NAME(28,i);
        if(i==11 && gOptions_metric)label=NAME(28,14);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:getScoreStr(text,F32_AT(data,64),0);break;
        case 1:TB_Sprintf(text,recordscr_race_laps_format,U16_AT(data,68));break;
        case 2:
            average=0.0f;
            if(U16_AT(data,68)>0)
                average=F32_AT(data,64)/(f32)(u32)U16_AT(data,68);
            getScoreStr(text,average,0);
            break;
        case 3:TB_Sprintf(text,recordscr_race_count_format,U16_AT(data,70));break;
        case 4:TB_Sprintf(text,recordscr_race_first_format,U16_AT(data,72));break;
        case 5:TB_Sprintf(text,recordscr_race_second_format,U16_AT(data,74));break;
        case 6:TB_Sprintf(text,recordscr_race_third_format,U16_AT(data,76));break;
        case 7:TB_Sprintf(text,recordscr_race_crashes_format,U16_AT(data,78));break;
        case 8:TB_Sprintf(text,recordscr_race_death_attempted_format,U16_AT(data,80));break;
        case 9:TB_Sprintf(text,recordscr_race_death_completed_format,U16_AT(data,82));break;
        case 10:TB_Sprintf(text,recordscr_race_ghosts_format,U16_AT(data,84));break;
        case 11: {u32 divisor=10;TB_Sprintf(text,recordscr_race_distance_format,distance/divisor,distance%divisor);break;}
        case 12:TB_Sprintf(text,recordscr_race_coins_a_format,CountBits((u16)(U16_AT(data,92)&255),16));break;
        case 13:TB_Sprintf(text,recordscr_race_coins_b_format,CountBits((u16)(U16_AT(data,92)&65280),16));break;
        }
        if(i==0 || i==2)HAL_SetFont(2);
        TB_DrawText(x+4,y,text);
        if(i==0 || i==2)HAL_SetFont(11);
        y+=TB_GetFontLineHeight();
    }
}
void drawTotalStats(void)
{
    u32 divisor;
    s16 x,y;
    u32 distance=0;
    u32 total=0;
    u16 found=0,race_a=0,race_b=0,track_a=0,track_b=0;
    s16 i,row;
    u8 text[48],*label,*data,*record,*track;
    if(!gStatPlayer)return;
    TB_SetJust(0,3);
    for(i=0;i<12;i++) {
        data=PROFILE_DATA+140+i*96;
        distance+=U32_AT(data,88);
        if(i<6) {
            race_a+=CountBits(U16_AT(data,92)&255,16);
            race_b+=CountBits(U16_AT(data,92)&65280,16);
        }
    }
    for(i=0;i<4;i++) {
        track=PROFILE_DATA+1292+i*64;
        total+=U32_AT(track,12);
        track_a+=CountBits(U16_AT(track,60)&255,16);
        track_b+=CountBits(U16_AT(track,60)&65280,16);
    }
    for(i=0;i<8;i++) {
        record=PROFILE_DATA+1548+i*12;
        found+=U16_AT(record,8);
    }
    HAL_SetFont(11);TB_SetTextColor(1);
    divisor=10;
    for(row=0;row<7;row++) {
        if(row==0) {x=180;y=90;}
        label=NAME(33,row);
        if(row==0 && gOptions_metric)label=NAME(28,7);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(row) {
        case 0:
            if(gOptions_metric) {
                TB_Sprintf(text,recordscr_total_km_format,(distance*divisor/6)/divisor,(distance*divisor/6)%divisor);
            } else TB_Sprintf(text,recordscr_total_miles_format,distance/divisor,distance%divisor);
            break;
        case 1:TB_Sprintf(text,recordscr_total_stunt_points_format,total);break;
        case 2:TB_Sprintf(text,recordscr_total_battle_points_format,found);break;
        case 3:TB_Sprintf(text,recordscr_silver_race_coins_format,race_a,48);break;
        case 4:TB_Sprintf(text,recordscr_gold_race_coins_format,race_b,48);break;
        case 5:TB_Sprintf(text,recordscr_silver_stunt_coins_format,track_a,32);break;
        case 6:TB_Sprintf(text,recordscr_gold_stunt_coins_format,track_b,32);break;
        }
        TB_DrawText(x+4,y,text);y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,12,86,308,94+TB_GetFontLineHeight()*7);
}
void drawUnlockedCars(void)
{
    Profile **saved_owner;
    s16 saved_players,i,y,width=20;
    if(!gStatPlayer)return;
    TB_SetJust(0,3);
    saved_owner=gPlayers[0].owner;saved_players=gNumPlayers;
    gPlayers[0].owner=gStatPlayer;gNumPlayers=1;
    CalcUnlockedStuff();
    gPlayers[0].owner=saved_owner;gNumPlayers=saved_players;
    HAL_SetFont(12);TB_SetTextColor(1);y=90;
    for(i=6;i<13;i++) {
        if(i==0 || i==1 || i==2 || i==3 || i==4 || i==5)continue;
        if(gUnlockedCars[i]) {
            width=(u32)HAL_GetTextWidth(CarTitles[i],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(CarTitles[i],-1);
            TB_DrawText(TB_CenterText(CarTitles[i],160),y,CarTitles[i]);
            y+=TB_GetFontLineHeight();
        }
    }
    if(y==90) {
        width=(u32)HAL_GetTextWidth(gText.text[61],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.text[61],-1);
        TB_DrawText(TB_CenterText(gText.text[61],160),y,gText.text[61]);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,152-width/2,86,168+width/2,y+4);
}
void getScoreStr(u8 *,f32,s32);
void drawScores(s32 category)
{
    s32 track,y,i;
    u8 *data;
    u8 text[48];
    TB_SetJust(1,3);
    track=gRecordTrack/2;
    if(gRecordTrack&1)track+=6;
    if(gStatPlayer)data=PROFILE_DATA+140+track*96;
    else data=gTotalRaceStats+track*96;
    HAL_SetFont(11);TB_SetTextColor(1);
    TB_DrawText(160,90,NAME(27,category));
    y=90+TB_GetFontLineHeight();
    HAL_SetFont(2);
    for(i=0;i<5;i++) {
        getScoreStr(text,F32_AT(data,4+category*20+i*4),0);
        TB_DrawText(160,y,text);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,100,86,220,94+TB_GetFontLineHeight()*6);
}
void drawBattleStats(void)
{
    u8 *data;
    s32 x,y;
    u8 *label;
    s32 i;
    u8 text[40];
    if(gStatPlayer)data=PROFILE_DATA+1404+gRecordTrack*12;
    else data=gTotalBattleStats+gRecordTrack*12-144;
    HAL_SetFont(11);TB_SetTextColor(1);
    for(i=0;i<3;i++) {
        if(i==0) {x=190;y=90;}
        label=NAME(29,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:TB_Sprintf(text,recordscr_battle_matches_format,U16_AT(data,4));break;
        case 1:TB_Sprintf(text,recordscr_battle_wins_format,U16_AT(data,6));break;
        case 2:TB_Sprintf(text,recordscr_battle_kills_format,U16_AT(data,8));break;
        case 3:TB_Sprintf(text,recordscr_battle_deaths_format,U16_AT(data,10));break;
        }
        TB_DrawText(x+4,y,text);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,80,86,240,y+4);
}
void drawStuntStats(void)
{
    s32 unresolved_local;
    s32 x,y;
    u8 *label,text[48],*data;
    s32 i,rows;
    if(gStatPlayer)data=PROFILE_DATA+12+gRecordTrack*64;
    else data=gTotalStuntStats+gRecordTrack*64-1280;
    HAL_SetFont(11);TB_SetTextColor(1);
    rows=0;
    for(i=0;i<8;i++) {
        if(i==0) {x=190;y=90;}
        else if(i==4 || i==5)continue;
        else if((i==6 || i==7) && !gStatPlayer)continue;
        label=NAME(30,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:TB_Sprintf(text,recordscr_stunt_best_format,U32_AT(data,4));break;
        case 1:TB_Sprintf(text,recordscr_stunt_best_average_format,U32_AT(data,8));break;
        case 2:TB_Sprintf(text,recordscr_stunt_points_format,U32_AT(data,12));break;
        case 3:TB_Sprintf(text,recordscr_stunt_time_format,U16_AT(data,16));break;
        case 4:TB_Sprintf(text,recordscr_stunt_crashes_format,U16_AT(data,18));break;
        case 5:TB_Sprintf(text,recordscr_stunt_count_format,U32_AT(data,20));break;
        case 6:TB_Sprintf(text,recordscr_stunt_coins_a_format,CountBits((u16)(U16_AT(data,60)&255),16));break;
        case 7:TB_Sprintf(text,recordscr_stunt_coins_b_format,CountBits((u16)(U16_AT(data,60)&65280),16));break;
        }
        TB_DrawText(x+4,y,text);y+=TB_GetFontLineHeight();rows++;
    }
    ChangePaneSize(recordscr_viewPane,80,86,240,94+TB_GetFontLineHeight()*rows);
}
void drawCircuitStats(void)
{
    u8 *data;
    s32 x,y;
    u8 *label;
    s32 i;
    u8 text[40];
    if(gStatPlayer)data=PROFILE_DATA+1668+gRecordCircuit*28;
    else data=gTotalCircuitStats+gRecordCircuit*28;
    HAL_SetFont(11);TB_SetTextColor(1);
    for(i=0;i<8;i++) {
        if(i==0) {x=180;y=90;}
        label=NAME(32,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:TB_Sprintf(text,recordscr_circuit_count_format,U16_AT(data,4));break;
        case 1:TB_Sprintf(text,recordscr_circuit_first_format,U16_AT(data,6));break;
        case 2:TB_Sprintf(text,recordscr_circuit_second_format,U16_AT(data,8));break;
        case 3:TB_Sprintf(text,recordscr_circuit_third_format,U16_AT(data,10));break;
        case 4:TB_Sprintf(text,recordscr_circuit_death_attempted_format,U16_AT(data,12));break;
        case 5:TB_Sprintf(text,recordscr_circuit_death_completed_format,U16_AT(data,14));break;
        case 6:TB_Sprintf(text,recordscr_circuit_best_points_format,U16_AT(data,16));break;
        case 7:getScoreStr(text,F32_AT(data,20),0);break;
        case 8:TB_Sprintf(text,recordscr_circuit_total_points_format,U32_AT(data,24));break;
        }
        if(i==7)HAL_SetFont(2);
        TB_DrawText(x+4,y,text);
        if(i==7)HAL_SetFont(11);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,50,86,270,y+4);
}
void drawObstacleStats(void)
{
    u8 *data;
    s32 x,y;
    u8 *label;
    s32 i;
    u8 text[40];
    if(gStatPlayer)data=PROFILE_DATA+1068+gRecordTrack*24;
    else data=gTotalObstacleStats+gRecordTrack*24-576;
    HAL_SetFont(11);TB_SetTextColor(1);
    for(i=0;i<4;i++) {
        if(i==0) {x=190;y=90;}
        else if(i==4)continue;
        label=NAME(31,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:getScoreStr(text,F32_AT(data,4),0);break;
        case 1:getScoreStr(text,F32_AT(data,8),0);break;
        case 2:TB_Sprintf(text,recordscr_obstacle_races_format,U16_AT(data,12));break;
        case 3:TB_Sprintf(text,recordscr_obstacle_finished_format,U16_AT(data,14));break;
        case 4:TB_Sprintf(text,recordscr_obstacle_crashes_format,U16_AT(data,16));break;
        case 5:TB_Sprintf(text,recordscr_obstacle_death_attempted_format,U16_AT(data,18));break;
        case 6:TB_Sprintf(text,recordscr_obstacle_death_completed_format,U16_AT(data,20));break;
        }
        if(i==0 || i==1)HAL_SetFont(2);
        TB_DrawText(x+4,y,text);
        if(i==0 || i==1)HAL_SetFont(11);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,40,86,280,94+TB_GetFontLineHeight()*4);
}
void recordscr_drawSelect(void);
void drawUnlockedParts(void);
void drawUnlockedCars(void);
void drawUnlockedTracks(void);
void drawTotalStats(void);
void drawCircuitStats(void);
void drawObstacleStats(void);
void drawStuntStats(void);
void drawBattleStats(void);
void drawScores(s32);
void drawRaceStats(void);
/* Image-A recordscr animText callback AD57C (GC recordscr.c animText 0x8008451C): HAL_SetTextDepth(0); switch (gRecordState)
 * { case 0: drawSelect() [the call recordscr_drawSelect]; case 1: recordscr_drawView() [inlined by umerge, stub 0x803AD574] }
 * HAL_SetTextDepth(-1); return 1. drawView (GC 0x80081C78) is a direct typed MAT3 port; its kViewStuntTotals arm
 * calls drawStuntTotals (GC 0x80082B30), a REAL function nested-inlined by umerge (its deleted stub is retail's
 * caller-less 0x803ACA8C between drawObstacleStats and drawStuntStats).  Both are NON-static and off `keep`, with unique
 * symbols recordscr_drawView / recordscr_drawStuntTotals.
 * GC's dead block-scope `Color4 c1, c2;` survives in ONE flash-colour arm only: with that the cfe area is 170 (retail
 * <= 172), frame 312 and every local/home/spill slot is retail's (traced the spill homes with tools/trace/spill.sh).
 * NOT A MATCH (context): strict 497/523 from a one-word shift at the state dispatch (as1 hoists the case-1 `li at,1` into
 * the `lw` stall; retail's as1 input had a block boundary there: a label between the lw and the beqz reproduces retail),
 * plus three more as1 placements; 30 non-equal-run words. See */
void recordscr_drawStuntTotals(void);
void recordscr_drawView(void);
void recordscr_drawView(void)
{
    s32 i, x, y, cx;
    u8 *str;
    u8 str1[64];
    s16 screenPos[2];
        TB_SetJust(1,1);
        ZOID_ProjectWindow(0,recordscr_buttonInfo[17].position,&gCamera[0],0,screenPos);
        cx=screenPos[0];y=screenPos[1];
        HAL_SetMenuTextDepth(recordscr_buttonInfo[17].position[2]-10.0f);
        TB_SetTextColor(1);HAL_SetFont(13);TB_SetTextColor(1);
        str=!gStatPlayer ? gText.text[176] : (*gStatPlayer)->name;TB_DrawEllipseText(cx,y,140,str);
        TB_SetJust(0,1);TB_GetFontLineHeight();HAL_SetFont(11);
        for(i=0;i<2;i++) {
            if(recordscr_buttonInfo[i+11].angle>0.6981316804885864f && recordscr_buttonInfo[i+11].angle<2.4434609413146973f)continue;
            if(!recordscr_buttonInfo[i+11].alpha)continue;
            HAL_SetTextTranslucency((f32)(u32)recordscr_buttonInfo[i+11].alpha);
            if(recordscr_buttonInfo[i+11].angle<1.5707963705062866f) {
                Color4 c1,c2;
                TB_FlashTextColors(flashColors,flashColors_at_1);TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors,flashColors_at_1);TB_SetTextColor(22);
            }
            HAL_SetMenuTextDepth(recordscr_buttonInfo[i+11].position[2]-10.0f);
            ZOID_ProjectWindow(0,recordscr_buttonInfo[i+11].position,&gCamera[0],0,screenPos);
            x=screenPos[0]-35.0f*recordscr_gButtonScale*300.0f/recordscr_buttonZ;y=screenPos[1];
            ZOID_ProjectWindow(0,recordscr_buttonInfo[15].position,&gCamera[0],0,screenPos);
            cx=screenPos[0]-55.0f*recordscr_gButtonScale*300.0f/recordscr_buttonZ;
            str=NAME(51,i);TB_DrawText(x,y,str);
            x=cx;
            if(i==0) {
                if(gTrackType==4) {
                    TB_Sprintf(str1,recordscr_circuit_title_format,NAME(44,gRecordCircuit),gText.text[42]);str=str1;
                } else if(gTrackType==5)str=gText.text[176];
                else if(gRecordTrack<12 && (gRecordTrack&1)) {
                    TB_Sprintf(str1,recordscr_track_variant_title_format,gTrackTitles[gRecordTrack>=13 ? gRecordTrack-6 : gRecordTrack/2],gText.text[6]);str=str1;
                } else str=gTrackTitles[gRecordTrack>=13 ? gRecordTrack-6 : gRecordTrack/2];
                TB_DrawText(x,y,str);
            } else if(i==1){str=NAME(52,gViewStatType[gTrackType]);TB_DrawText(x,y,str);}
        }
        TB_SetJust(0,3);HAL_SetTextTranslucency(-1.0f);HAL_SetTextDepth(0.0f);
        switch(gViewStatType[gTrackType]) {
        case 0:drawRaceStats();break;
        case 1:drawScores(0);break;
        case 2:drawScores(1);break;
        case 3:drawScores(2);break;
        case 4:drawBattleStats();break;
        case 5:drawStuntStats();break;
        case 6:recordscr_drawStuntTotals();break;
        case 7:drawObstacleStats();break;
        case 8:drawCircuitStats();break;
        case 9:drawTotalStats();break;
        case 10:drawUnlockedTracks();break;
        case 11:drawUnlockedCars();break;
        case 12:drawUnlockedParts();break;
        }
        TB_SetJust(0,3);
}
void recordscr_drawStuntTotals(void);
void recordscr_drawStuntTotals(void)
{
            s32 statIdx, x, y;
            u8 *stats;
            u8 str[40];
            if(gStatPlayer)stats=PROFILE_DATA+12+gRecordTrack*64;
            else stats=gTotalStuntStats+gRecordTrack*64-1280;
            HAL_SetFont(11);TB_SetTextColor(1);
            for(statIdx=0;statIdx<10;statIdx++) {
                if(statIdx==0) {x=110;y=90;}
                else if(statIdx==5) {x=250;y=90;}
                TB_Sprintf(str,recordscr_stunt_kind_label_format,NAME(37,statIdx));
                TB_DrawText(TB_RightText(str,x-4),y,str);
                TB_Sprintf(str,recordscr_stunt_kind_count_format,U32_AT(stats,20+statIdx*4));
                TB_DrawText(x+4,y,str);y+=TB_GetFontLineHeight();
            }
            ChangePaneSize(recordscr_viewPane,20,86,300,94+TB_GetFontLineHeight()*5);
            }
s32 recordscr_animText(void *callback_context)
{
    HAL_SetTextDepth(0.0f);
    switch(gRecordState) {
    case 0:recordscr_drawSelect();break;
    case 1:recordscr_drawView();break;
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
