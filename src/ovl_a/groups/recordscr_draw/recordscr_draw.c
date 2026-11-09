/* The records screen's drawing (MAT3 recordscr.c animText, drawSelect, drawView and its draw* pages):
 * recordscr_animText is the screen's text blit callback. The statistics are a profile's save data (read by
 * ReadPlayerData) or, for the machine's totals, the gTotal*Stats tables AddPlayerStats fills. */
#include "game/game.h"

typedef struct Color4 { u32 word; } Color4;
/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10): pak is
 * the pak it is on. */
typedef struct FileInfo { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak; } FileInfo;
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): the links of gPlayerFiles, its pak
 * file, its id, its name, its save data (datah: a handle to a SaveData). */
typedef struct Profile Profile;
struct Profile { Profile **next,**previous; FileInfo **fileh; u8 id[8]; u8 name[24]; u8 **datah; };
/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct Player { u8 unknown00[72]; Profile **owner; } Player;
typedef struct FiveParts FiveParts;
typedef struct TextBox {s16 x,y,width,height;} TextBox;

/* The statistics a profile keeps (MAT3 rush_types.h's tSaveData parts; no pointers, so the PC has the N64's layout:
 * the N64's records sit 4 bytes earlier than MAT3's, its controls being one shorter). One race track's (MAT3
 * anon_checksum_scores_60), a stunt track's (anon_checksum_bestStunt_40), a battle arena's (anon_checksum_totalMatches_c),
 * the obstacle course's (anon_checksum_bestTime_18) and a circuit's (anon_checksum_numCircuits_1c). */
typedef struct RaceStats {
    u32 checksum;
    f32 scores[3][5];           /* 0x04 the three tables' five times */
    f32 totalTime;              /* 0x40 */
    u16 totalLaps;              /* 0x44 */
    u16 totalRaces;             /* 0x46 */
    u16 total1st, total2nd, total3rd; /* 0x48 */
    u16 totalCrashes;           /* 0x4E */
    u16 totalDeathAttempted;    /* 0x50 */
    u16 totalDeathCompleted;    /* 0x52 */
    u16 totalGhosts;            /* 0x54 */
    u32 totalMiles;             /* 0x58 tenths of a mile */
    u16 coinsFound;             /* 0x5C a bit per coin: silver in the low byte, gold in the high one */
} RaceStats;                    /* 0x60 */
typedef struct StuntStats {
    u32 checksum;
    u32 bestStunt;              /* 0x04 */
    u32 bestAvg;                /* 0x08 */
    u32 totalPoints;            /* 0x0C */
    u16 totalTime;              /* 0x10 */
    u16 totalCrashes;           /* 0x12 */
    u32 totalStunts[10];        /* 0x14 each kind of stunt's count */
    u16 coinsFound;             /* 0x3C silver in the low byte, gold in the high one */
} StuntStats;                   /* 0x40 */
typedef struct BattleStats {
    u32 checksum;
    u16 totalMatches;           /* 0x04 */
    u16 totalWins;              /* 0x06 */
    u16 totalKills;             /* 0x08 */
    u16 totalDeaths;            /* 0x0A */
} BattleStats;                  /* 0x0C */
typedef struct ObstacleStats {
    u32 checksum;
    f32 bestTime;               /* 0x04 */
    f32 totalTime;              /* 0x08 */
    u16 totalRaces;             /* 0x0C */
    u16 totalFinished;          /* 0x0E */
    u16 totalCrashes;           /* 0x10 */
    u16 numDeathAttempted;      /* 0x12 */
    u16 numDeathCompleted;      /* 0x14 */
} ObstacleStats;                /* 0x18 */
typedef struct CircuitStats {
    u32 checksum;
    u16 numCircuits;            /* 0x04 */
    u16 num1st, num2nd, num3rd; /* 0x06 */
    u16 numDeathAttempted;      /* 0x0C */
    u16 numDeathCompleted;      /* 0x0E */
    u16 bestPts;                /* 0x10 */
    f32 bestTime;               /* 0x14 */
    u32 totalPts;               /* 0x18 */
} CircuitStats;                 /* 0x1C */
/* A profile's save data (MAT3 tSaveData): its settings, then these. */
typedef struct SaveData {
    u8 settings[0x8C];
    RaceStats raceStats[12];         /* 0x08C: the six race tracks, forward and backward (the record track / 2, + 6 backward) */
    StuntStats stuntStats[4];        /* 0x50C */
    BattleStats battleStats[8];      /* 0x60C */
    ObstacleStats obstacleStats[1];  /* 0x66C */
    CircuitStats circuitStats[4];    /* 0x684 */
} SaveData;
#define PROFILE_SAVE ((SaveData *)*(*gStatPlayer)->datah)

/* gRecordTrack's kinds (recordscr.c) */
#define RECORD_FIRST_BATTLE 12
#define RECORD_FIRST_STUNT 20
#define RECORD_FIRST_OBSTACLE 24
#define kTrackTypeCircuit 4
#define kTrackTypeTotals 5

/* recordscr_buttonInfo[]: the select menu's rows 0-10, the view's rows 11 and 12, then (recordscr.c) */
#define RECORD_VIEW_ROW_BUTTON 11
#define RECORD_BUTTON_CLEAR 13
#define RECORD_BUTTON_PAK_VALUE 14
#define RECORD_BUTTON_TRACK_VALUE 15
#define RECORD_BUTTON_TITLE 17

/* The select menu's rows (recordscr.c) */
#define kSelectMenuTotals 0
#define kSelectMenuSelectPak 1
#define kSelectMenuPakStatus 2
#define kSelectMenuSelectPlayer 3
#define RECORD_SELECT_ROWS 11

/* recordscr_gNumPakPlayers[pak]: why a pak has no profiles (recordscr.c) */
#define PAK_NOT_INSERTED (-1)
#define PAK_DAMAGED (-2)
#define PAK_RUMBLE (-3)
#define PAK_NO_CONTROLLER (-4)

/* A button's text is hidden while the button is edge-on (between 40 and 140 degrees of its turn). */
#define EDGE_ON_FROM 0.6981316804885864f
#define EDGE_ON_TO 2.4434609413146973f

VIEW(Color4, flashColors);
#define flashColors flashColors_view
VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view
extern Player gPlayers[];

VIEW(Profile **, gStatPlayer);
#define gStatPlayer gStatPlayer_view

VIEW(TextBox, gScreenRect);
#define gScreenRect gScreenRect_view
VIEW(FiveParts *, recordscr_viewPane);
#define recordscr_viewPane recordscr_viewPane_view
VIEW(u8 *[], gPS_PakName);
#define gPS_PakName gPS_PakName_view
VIEW(u8 *[], gTrackTitles);
#define gTrackTitles gTrackTitles_view
VIEW(u8 *[], CarTitles);
#define CarTitles CarTitles_view
VIEW(StuntStats[], gTotalStuntStats);
#define gTotalStuntStats gTotalStuntStats_view
VIEW(RaceStats[], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view
VIEW(BattleStats[], gTotalBattleStats);
#define gTotalBattleStats gTotalBattleStats_view
VIEW(ObstacleStats[], gTotalObstacleStats);
#define gTotalObstacleStats gTotalObstacleStats_view
VIEW(CircuitStats[], gTotalCircuitStats);
#define gTotalCircuitStats gTotalCircuitStats_view
VIEW(s8[], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view
VIEW(s8[], gUnlockedCircuits);
#define gUnlockedCircuits gUnlockedCircuits_view
VIEW(s8[], gUnlockedCars);
#define gUnlockedCars gUnlockedCars_view
VIEW(s8[], gUnlockedEngines);
#define gUnlockedEngines gUnlockedEngines_view
VIEW(s8[], gUnlockedFrames);
#define gUnlockedFrames gUnlockedFrames_view
VIEW(s8[], gUnlockedTrans);
#define gUnlockedTrans gUnlockedTrans_view
VIEW(s8[], gUnlockedTires);
#define gUnlockedTires gUnlockedTires_view

extern void TB_FlashTextColors(Color4,Color4);

extern void ChangePaneSize(FiveParts *,s32,s32,s32,s32);

/* gText.tab[g]'s entry i: one of a table of texts. */
#define NAME(g,i) (gText.taba[gText.tab[g]+(i)])

/* A time as text (minutes, seconds, thousandths), or the "no time" text for 0, into destination. The last
 * argument (0 from every caller) is not used. */
void getScoreStr(u8 *destination,f32 time,s32 unused_format)
{
    if(time==0.0f)TB_Strcpy(destination,recordscr_no_time_text);
    else {
        gDefTime=time;
        TB_Sprintf(destination,recordscr_time_format,(u32)(time/60.0f)%100,(u32)time%60,(u32)(time*1000.0f)%1000);
    }
}
/* The select menu's text: the title over the title button; then the "clear records" question about the profile
 * (or the guests) with yes / no (the answer flashing), or its "cleared" notice, each in a box; or each row's name
 * beside its button (skipped while the button is edge-on, at its alpha, flashing when it faces front): the totals,
 * the pak row with the pak's name, the pak's status line (why it has no profiles), and the pak's profiles (the
 * profile that many rows from the selected one), with the clear button's label beside the selected profile. */
void recordscr_drawSelect(void)
{
    s32 i, x, y, cx;
    u8 *str;
    u8 str1[64];
    s16 screenPos[2];
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    ZOID_ProjectWindow(0, recordscr_buttonInfo[17].position, &gCamera[0], 0, screenPos);
    cx = screenPos[0];
    y = screenPos[1];
    HAL_SetMenuTextDepth(recordscr_buttonInfo[17].position[2] - 10.0f);
    TB_SetTextColor(kTB_WhiteText);
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    str = gText.single[47];
    TB_DrawText(TB_CenterText(str, cx), y, str);
    y += TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    if (gDeleteConfirm == 1) {
        HAL_SetTextDepth(0.0f);
        y += TB_GetFontLineHeight();
        if (gStatPlayer) {
            TB_Sprintf(str1, gText.single[143], gDeleteName);
            str = str1;
        } else str = gText.single[144];
        ChangePaneSize(recordscr_viewPane, cx - gScreenRect.width / 2 - 4, y - 4,
                         cx + gScreenRect.width / 2 + 4, 4 + y + ((TB_GetTextNumLines(gScreenRect.width, str) + 2) * TB_GetFontLineHeight()));
        TB_SetTextColor(kTB_WhiteText);
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(cx, y, gScreenRect.width, gScreenRect.height, -1, 0, str);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        y += TB_GetFontLineHeight() * (TB_GetTextNumLines(gScreenRect.width, str) + 1);
        str = NAME(24, 0);
        TB_SetTextColor(recordscr_gYes == 1 ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(TB_RightText(str, cx - 5), y, str);
        str = NAME(24, 1);
        TB_SetTextColor(recordscr_gYes == 0 ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(cx + 5, y, str);
    } else if (gDeleteNotify == 1) {
        HAL_SetTextDepth(0.0f);
        y += TB_GetFontLineHeight();
        if (gStatPlayer) {
            TB_Sprintf(str1, gText.single[146], gDeleteName);
            str = str1;
        } else str = gText.single[147];
        ChangePaneSize(recordscr_viewPane, cx - gScreenRect.width / 2 - 4, y - 4,
                         cx + gScreenRect.width / 2 + 4, 4 + y + (TB_GetTextNumLines(gScreenRect.width, str) * TB_GetFontLineHeight()));
        TB_SetTextColor(kTB_WhiteText);
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(cx, y, gScreenRect.width, gScreenRect.height, -1, 0, str);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
    } else {
        for (i = 0; i < RECORD_SELECT_ROWS; i++) {
            ChangePaneSize(recordscr_viewPane, -20, 0, -10, 10);
            HAL_SetMenuTextDepth(recordscr_buttonInfo[i].position[2] - 10.0f);
            ZOID_ProjectWindow(0, recordscr_buttonInfo[i].position, &gCamera[0], 0, screenPos);
            if (recordscr_buttonInfo[i].type == 11) x = screenPos[0];
            else if (recordscr_buttonInfo[i].type == 9) x = screenPos[0] - 65.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            else x = screenPos[0] - 45.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            y = screenPos[1];
            if (i == 1) {
                ZOID_ProjectWindow(0, recordscr_buttonInfo[14].position, &gCamera[0], 0, screenPos);
                cx = screenPos[0] - 25.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            } else {
                ZOID_ProjectWindow(0, recordscr_buttonInfo[13].position, &gCamera[0], 0, screenPos);
                cx = screenPos[0] - 45.0f * recordscr_gButtonScale * 300.0f / recordscr_buttonZ;
            }
            if (i >= kSelectMenuSelectPlayer && i < RECORD_SELECT_ROWS && i == recordscr_gSelectMenuSelect &&
                (recordscr_buttonInfo[13].angle < 0.6981317f || recordscr_buttonInfo[13].angle > 2.443461f) && recordscr_buttonInfo[13].alpha) {
                HAL_SetTextTranslucency(recordscr_buttonInfo[13].alpha);
                if (recordscr_buttonInfo[13].angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(kTB_WhiteText);
                } else {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(kTB_FlashText);
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
                TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(kTB_FlashText);
            }
            if (i == kSelectMenuTotals) {
                str = NAME(50, 0);
                TB_DrawText(x, y, str);
            } else if (i == kSelectMenuSelectPak) {
                str = NAME(50, 1);
                TB_DrawText(x, y, str);
                str = gPS_PakName[recordscr_gSelectPakIdx];
                TB_DrawText(cx, y, str);
            } else if (i == kSelectMenuPakStatus) {
                switch (recordscr_gNumPakPlayers[recordscr_gSelectPakIdx]) {
                case PAK_NO_CONTROLLER: str = gText.single[165]; break;
                case PAK_RUMBLE: str = gText.single[169]; break;
                case 0: str = gText.single[148]; break;
                case PAK_DAMAGED: str = gText.single[166]; break;
                case PAK_NOT_INSERTED: str = gText.single[168]; break;
                }
                TB_SetTextColor(0);
                TB_DrawText(TB_CenterText(str, x), y, str);
            } else if (i >= kSelectMenuSelectPlayer && i < RECORD_SELECT_ROWS) {
                Profile **player;
                s32 playerIdx;
                player = gStatPlayer;
                if (!player) {
                    for (playerIdx = 0, player = gPlayerFiles.head; player; player = (*player)->next) {
                        if ((*(*player)->fileh)->pak == recordscr_gSelectPakIdx) {
                            if (playerIdx == i - kSelectMenuSelectPlayer) str = (*player)->name;
                            playerIdx++;
                        }
                    }
                } else {
                    for (playerIdx = 0; player; ) {
                        if ((*(*player)->fileh)->pak == recordscr_gSelectPakIdx) {
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
/* The unlocked parts page (a profile's only): the engines and frames (left column), transmissions and tyres (right)
 * the profile has unlocked past the ones every car starts with, a "none" line under a heading with none, and a line
 * when nothing at all is unlocked. CalcUnlockedStuff works it out as if the profile were the only player. */
void drawUnlockedParts(void)
{
    Profile **saved_owner;
    s16 saved_players,i,y,bottom;
    s32 all_empty=1,empty;
    u8 *label;
    if(!gStatPlayer)return;
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    saved_owner=gPlayers[0].owner;saved_players=gNumPlayers;
    gNumPlayers=1;gPlayers[0].owner=gStatPlayer;
    CalcUnlockedStuff();
    gPlayers[0].owner=saved_owner;gNumPlayers=saved_players;
    TB_SetTextColor(kTB_WhiteText);
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
        TB_DrawText(TB_CenterText(gText.single[62],160),y,gText.single[62]);
        y+=TB_GetFontLineHeight();
    }
    if(y+4>=bottom)bottom=y+4;
    ChangePaneSize(recordscr_viewPane,12,86,308,bottom);
}
/* The unlocked tracks page (a profile's only): the tracks and circuits the profile has unlocked, past the ones open
 * from the start, in one or two columns (tracks before 14, then the rest and the circuits), or four lines of text
 * when there are none. */
void drawUnlockedTracks(void)
{
    s32 first;
    Profile **saved_owner;
    s16 saved_players,i,x,y,bottom,width=20;
    s32 second;
    u8 text[44];
    if(!gStatPlayer)return;
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
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
    HAL_SetFont(12);TB_SetTextColor(kTB_WhiteText);
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
        TB_Sprintf(text,recordscr_unlocked_circuit_format,NAME(44,i),gText.single[42]);
        width=(u32)HAL_GetTextWidth(text,-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(text,-1);
        TB_DrawText(TB_CenterText(text,x),y,text);y+=TB_GetFontLineHeight();
    }
    if(!first && !second) {
        width=(u32)HAL_GetTextWidth(gText.single[57],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.single[57],-1);
        TB_DrawText(TB_CenterText(gText.single[57],160),y,gText.single[57]);y+=TB_GetFontLineHeight();
        width=(u32)HAL_GetTextWidth(gText.single[58],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.single[58],-1);
        TB_DrawText(TB_CenterText(gText.single[58],160),y,gText.single[58]);y+=TB_GetFontLineHeight();
        width=(u32)HAL_GetTextWidth(gText.single[59],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.single[59],-1);
        TB_DrawText(TB_CenterText(gText.single[59],160),y,gText.single[59]);y+=TB_GetFontLineHeight();
        width=(u32)HAL_GetTextWidth(gText.single[60],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.single[60],-1);
        TB_DrawText(TB_CenterText(gText.single[60],160),y,gText.single[60]);y+=TB_GetFontLineHeight();
    } else if(first && second)width+=160;
    if(y>=bottom)bottom=y;
    ChangePaneSize(recordscr_viewPane,152-width/2,86,168+width/2,bottom+4);
}

/* A race track's statistics page: total time, laps and the average lap; races and their places; crashes, death
 * races tried and finished; ghosts; the distance (in km with gOptions[9], the metric option); and for a profile the
 * coins found. The record track gRecordTrack is the track times two, plus one backward; the stats keep the six
 * forward tracks first. */
void drawRaceStats(void)
{
    s32 track,i,x,y;
    u8 *label,text[48];
    RaceStats *stats;
    u32 distance;
    f32 average;
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    track=gRecordTrack/2;
    if(gRecordTrack&1)track+=6;
    if(gStatPlayer)stats=&PROFILE_SAVE->raceStats[track];
    else stats=&gTotalRaceStats[track];
    distance=stats->totalMiles;
    if(gOptions[9])distance=distance*10/6;
    HAL_SetFont(11);
    ChangePaneSize(recordscr_viewPane,12,86,308,94+TB_GetFontLineHeight()*8);
    TB_SetTextColor(kTB_WhiteText);
    for(i=0;i<14;i++) {
        if(i==0) {x=100;y=90;}
        else if(i==3) {x=110;y+=TB_GetFontLineHeight();}
        else if(i==7) {x=270;y=90+TB_GetFontLineHeight()*4;}
        else if(i==10) {x=260;y=90;continue;}
        else if((i==12 || i==13) && !gStatPlayer)continue;
        label=NAME(28,i);
        if(i==11 && gOptions[9])label=NAME(28,14);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:getScoreStr(text,stats->totalTime,0);break;
        case 1:TB_Sprintf(text,recordscr_race_laps_format,stats->totalLaps);break;
        case 2:
            average=0.0f;
            if(stats->totalLaps>0)
                average=stats->totalTime/(f32)(u32)stats->totalLaps;
            getScoreStr(text,average,0);
            break;
        case 3:TB_Sprintf(text,recordscr_race_count_format,stats->totalRaces);break;
        case 4:TB_Sprintf(text,recordscr_race_first_format,stats->total1st);break;
        case 5:TB_Sprintf(text,recordscr_race_second_format,stats->total2nd);break;
        case 6:TB_Sprintf(text,recordscr_race_third_format,stats->total3rd);break;
        case 7:TB_Sprintf(text,recordscr_race_crashes_format,stats->totalCrashes);break;
        case 8:TB_Sprintf(text,recordscr_race_death_attempted_format,stats->totalDeathAttempted);break;
        case 9:TB_Sprintf(text,recordscr_race_death_completed_format,stats->totalDeathCompleted);break;
        case 10:TB_Sprintf(text,recordscr_race_ghosts_format,stats->totalGhosts);break;
        case 11: {u32 divisor=10;TB_Sprintf(text,recordscr_race_distance_format,distance/divisor,distance%divisor);break;}
        case 12:TB_Sprintf(text,recordscr_race_coins_a_format,CountBits((u16)(stats->coinsFound&255),16));break;
        case 13:TB_Sprintf(text,recordscr_race_coins_b_format,CountBits((u16)(stats->coinsFound&65280),16));break;
        }
        if(i==0 || i==2)HAL_SetFont(2);
        TB_DrawText(x+4,y,text);
        if(i==0 || i==2)HAL_SetFont(11);
        y+=TB_GetFontLineHeight();
    }
}
/* A profile's totals page: its distance (in km with the metric option), its stunt points, its battle kills, and the
 * silver and gold coins it has found on the race tracks (forward only, 48 each) and the stunt tracks (32 each). */
void drawTotalStats(void)
{
    u32 divisor;
    s16 x,y;
    u32 distance=0;
    u32 total=0;
    u16 found=0,race_a=0,race_b=0,track_a=0,track_b=0;
    s16 i,row;
    u8 text[48],*label;
    RaceStats *race;
    StuntStats *stunt;
    BattleStats *battle;
    if(!gStatPlayer)return;
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    for(i=0;i<12;i++) {
        race=&PROFILE_SAVE->raceStats[i];
        distance+=race->totalMiles;
        if(i<6) {
            race_a+=CountBits(race->coinsFound&255,16);
            race_b+=CountBits(race->coinsFound&65280,16);
        }
    }
    for(i=0;i<4;i++) {
        stunt=&PROFILE_SAVE->stuntStats[i];
        total+=stunt->totalPoints;
        track_a+=CountBits(stunt->coinsFound&255,16);
        track_b+=CountBits(stunt->coinsFound&65280,16);
    }
    for(i=0;i<8;i++) {
        battle=&PROFILE_SAVE->battleStats[i];
        found+=battle->totalKills;
    }
    HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
    divisor=10;
    for(row=0;row<7;row++) {
        if(row==0) {x=180;y=90;}
        label=NAME(33,row);
        if(row==0 && gOptions[9])label=NAME(28,7);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(row) {
        case 0:
            if(gOptions[9]) {
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
/* The unlocked cars page (a profile's only): the cars past the first six the profile has unlocked, or a line saying
 * none are. */
void drawUnlockedCars(void)
{
    Profile **saved_owner;
    s16 saved_players,i,y,width=20;
    if(!gStatPlayer)return;
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    saved_owner=gPlayers[0].owner;saved_players=gNumPlayers;
    gPlayers[0].owner=gStatPlayer;gNumPlayers=1;
    CalcUnlockedStuff();
    gPlayers[0].owner=saved_owner;gNumPlayers=saved_players;
    HAL_SetFont(12);TB_SetTextColor(kTB_WhiteText);y=90;
    for(i=6;i<13;i++) {
        if(i==0 || i==1 || i==2 || i==3 || i==4 || i==5)continue;
        if(gUnlockedCars[i]) {
            width=(u32)HAL_GetTextWidth(CarTitles[i],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(CarTitles[i],-1);
            TB_DrawText(TB_CenterText(CarTitles[i],160),y,CarTitles[i]);
            y+=TB_GetFontLineHeight();
        }
    }
    if(y==90) {
        width=(u32)HAL_GetTextWidth(gText.single[61],-1)<(u32)width ? (u32)width : (u32)HAL_GetTextWidth(gText.single[61],-1);
        TB_DrawText(TB_CenterText(gText.single[61],160),y,gText.single[61]);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,152-width/2,86,168+width/2,y+4);
}

/* A race track's high-score table `category` (the high-score screen's tables 0-2): its title and five times. */
void drawScores(s32 category)
{
    s32 track,y,i;
    RaceStats *stats;
    u8 text[48];
    TB_SetJust(kTB_JustCenter, kTB_JustTop);
    track=gRecordTrack/2;
    if(gRecordTrack&1)track+=6;
    if(gStatPlayer)stats=&PROFILE_SAVE->raceStats[track];
    else stats=&gTotalRaceStats[track];
    HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(160,90,NAME(27,category));
    y=90+TB_GetFontLineHeight();
    HAL_SetFont(2);
    for(i=0;i<5;i++) {
        getScoreStr(text,stats->scores[category][i],0);
        TB_DrawText(160,y,text);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,100,86,220,94+TB_GetFontLineHeight()*6);
}
/* A battle arena's statistics page: matches, wins and kills (the deaths' row is never reached: three rows). */
void drawBattleStats(void)
{
    BattleStats *stats;
    s32 x,y;
    u8 *label;
    s32 i;
    u8 text[40];
    if(gStatPlayer)stats=&PROFILE_SAVE->battleStats[gRecordTrack-RECORD_FIRST_BATTLE];
    else stats=&gTotalBattleStats[gRecordTrack-RECORD_FIRST_BATTLE];
    HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
    for(i=0;i<3;i++) {
        if(i==0) {x=190;y=90;}
        label=NAME(29,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:TB_Sprintf(text,recordscr_battle_matches_format,stats->totalMatches);break;
        case 1:TB_Sprintf(text,recordscr_battle_wins_format,stats->totalWins);break;
        case 2:TB_Sprintf(text,recordscr_battle_kills_format,stats->totalKills);break;
        case 3:TB_Sprintf(text,recordscr_battle_deaths_format,stats->totalDeaths);break;
        }
        TB_DrawText(x+4,y,text);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,80,86,240,y+4);
}
/* A stunt track's statistics page: the best stunt, the best average, the points and the time; for a profile the
 * coins found. Rows 4 and 5 (crashes, the stunt count) are skipped. */
void drawStuntStats(void)
{
    s32 x,y;
    u8 *label,text[48];
    StuntStats *stats;
    s32 i,rows;
    if(gStatPlayer)stats=&PROFILE_SAVE->stuntStats[gRecordTrack-RECORD_FIRST_STUNT];
    else stats=&gTotalStuntStats[gRecordTrack-RECORD_FIRST_STUNT];
    HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
    rows=0;
    for(i=0;i<8;i++) {
        if(i==0) {x=190;y=90;}
        else if(i==4 || i==5)continue;
        else if((i==6 || i==7) && !gStatPlayer)continue;
        label=NAME(30,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:TB_Sprintf(text,recordscr_stunt_best_format,stats->bestStunt);break;
        case 1:TB_Sprintf(text,recordscr_stunt_best_average_format,stats->bestAvg);break;
        case 2:TB_Sprintf(text,recordscr_stunt_points_format,stats->totalPoints);break;
        case 3:TB_Sprintf(text,recordscr_stunt_time_format,stats->totalTime);break;
        case 4:TB_Sprintf(text,recordscr_stunt_crashes_format,stats->totalCrashes);break;
        case 5:TB_Sprintf(text,recordscr_stunt_count_format,stats->totalStunts[0]);break;
        case 6:TB_Sprintf(text,recordscr_stunt_coins_a_format,CountBits((u16)(stats->coinsFound&255),16));break;
        case 7:TB_Sprintf(text,recordscr_stunt_coins_b_format,CountBits((u16)(stats->coinsFound&65280),16));break;
        }
        TB_DrawText(x+4,y,text);y+=TB_GetFontLineHeight();rows++;
    }
    ChangePaneSize(recordscr_viewPane,80,86,240,94+TB_GetFontLineHeight()*rows);
}
/* A circuit's statistics page: circuits run and their places, death circuits tried and finished, the best points
 * and the best time (the total points' row is never reached: eight rows). */
void drawCircuitStats(void)
{
    CircuitStats *stats;
    s32 x,y;
    u8 *label;
    s32 i;
    u8 text[40];
    if(gStatPlayer)stats=&PROFILE_SAVE->circuitStats[gRecordCircuit];
    else stats=&gTotalCircuitStats[gRecordCircuit];
    HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
    for(i=0;i<8;i++) {
        if(i==0) {x=180;y=90;}
        label=NAME(32,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:TB_Sprintf(text,recordscr_circuit_count_format,stats->numCircuits);break;
        case 1:TB_Sprintf(text,recordscr_circuit_first_format,stats->num1st);break;
        case 2:TB_Sprintf(text,recordscr_circuit_second_format,stats->num2nd);break;
        case 3:TB_Sprintf(text,recordscr_circuit_third_format,stats->num3rd);break;
        case 4:TB_Sprintf(text,recordscr_circuit_death_attempted_format,stats->numDeathAttempted);break;
        case 5:TB_Sprintf(text,recordscr_circuit_death_completed_format,stats->numDeathCompleted);break;
        case 6:TB_Sprintf(text,recordscr_circuit_best_points_format,stats->bestPts);break;
        case 7:getScoreStr(text,stats->bestTime,0);break;
        case 8:TB_Sprintf(text,recordscr_circuit_total_points_format,stats->totalPts);break;
        }
        if(i==7)HAL_SetFont(2);
        TB_DrawText(x+4,y,text);
        if(i==7)HAL_SetFont(11);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,50,86,270,y+4);
}
/* The obstacle course's statistics page: the best and total times, runs and finishes (four rows of the seven). */
void drawObstacleStats(void)
{
    ObstacleStats *stats;
    s32 x,y;
    u8 *label;
    s32 i;
    u8 text[40];
    if(gStatPlayer)stats=&PROFILE_SAVE->obstacleStats[gRecordTrack-RECORD_FIRST_OBSTACLE];
    else stats=&gTotalObstacleStats[gRecordTrack-RECORD_FIRST_OBSTACLE];
    HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
    for(i=0;i<4;i++) {
        if(i==0) {x=190;y=90;}
        else if(i==4)continue;
        label=NAME(31,i);
        TB_DrawText(TB_RightText(label,x-4),y,label);
        switch(i) {
        case 0:getScoreStr(text,stats->bestTime,0);break;
        case 1:getScoreStr(text,stats->totalTime,0);break;
        case 2:TB_Sprintf(text,recordscr_obstacle_races_format,stats->totalRaces);break;
        case 3:TB_Sprintf(text,recordscr_obstacle_finished_format,stats->totalFinished);break;
        case 4:TB_Sprintf(text,recordscr_obstacle_crashes_format,stats->totalCrashes);break;
        case 5:TB_Sprintf(text,recordscr_obstacle_death_attempted_format,stats->numDeathAttempted);break;
        case 6:TB_Sprintf(text,recordscr_obstacle_death_completed_format,stats->numDeathCompleted);break;
        }
        if(i==0 || i==1)HAL_SetFont(2);
        TB_DrawText(x+4,y,text);
        if(i==0 || i==1)HAL_SetFont(11);
        y+=TB_GetFontLineHeight();
    }
    ChangePaneSize(recordscr_viewPane,40,86,280,94+TB_GetFontLineHeight()*4);
}

/* The view's text: the profile's name (or the totals' title) over the title button; the two rows' names and values
 * (the track: a circuit by its name, the totals, a race track with "backward" for its odd records; the statistics
 * page's name) beside their buttons; then the page itself. */
void recordscr_drawView(void)
{
    s32 i, x, y, cx;
    u8 *str;
    u8 str1[64];
    s16 screenPos[2];
        TB_SetJust(kTB_JustCenter, kTB_JustCenter);
        ZOID_ProjectWindow(0,recordscr_buttonInfo[17].position,&gCamera[0],0,screenPos);
        cx=screenPos[0];y=screenPos[1];
        HAL_SetMenuTextDepth(recordscr_buttonInfo[17].position[2]-10.0f);
        TB_SetTextColor(kTB_WhiteText);HAL_SetFont(13);TB_SetTextColor(kTB_WhiteText);
        str=!gStatPlayer ? gText.single[176] : (*gStatPlayer)->name;TB_DrawEllipseText(cx,y,140,str);
        TB_SetJust(kTB_JustLeft, kTB_JustCenter);TB_GetFontLineHeight();HAL_SetFont(11);
        for(i=0;i<2;i++) {
            if(recordscr_buttonInfo[i+11].angle>EDGE_ON_FROM && recordscr_buttonInfo[i+11].angle<EDGE_ON_TO)continue;
            if(!recordscr_buttonInfo[i+11].alpha)continue;
            HAL_SetTextTranslucency((f32)(u32)recordscr_buttonInfo[i+11].alpha);
            if(recordscr_buttonInfo[i+11].angle<1.5707963705062866f) {
                TB_FlashTextColors(flashColors,flashColors_at_1);TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors,flashColors_at_1);TB_SetTextColor(kTB_FlashText);
            }
            HAL_SetMenuTextDepth(recordscr_buttonInfo[i+11].position[2]-10.0f);
            ZOID_ProjectWindow(0,recordscr_buttonInfo[i+11].position,&gCamera[0],0,screenPos);
            x=screenPos[0]-35.0f*recordscr_gButtonScale*300.0f/recordscr_buttonZ;y=screenPos[1];
            ZOID_ProjectWindow(0,recordscr_buttonInfo[15].position,&gCamera[0],0,screenPos);
            cx=screenPos[0]-55.0f*recordscr_gButtonScale*300.0f/recordscr_buttonZ;
            str=NAME(51,i);TB_DrawText(x,y,str);
            x=cx;
            if(i==0) {
                if(gTrackType==kTrackTypeCircuit) {
                    TB_Sprintf(str1,recordscr_circuit_title_format,NAME(44,gRecordCircuit),gText.single[42]);str=str1;
                } else if(gTrackType==kTrackTypeTotals)str=gText.single[176];
                else if(gRecordTrack<12 && (gRecordTrack&1)) {
                    TB_Sprintf(str1,recordscr_track_variant_title_format,gTrackTitles[gRecordTrack>=13 ? gRecordTrack-6 : gRecordTrack/2],gText.single[6]);str=str1;
                } else str=gTrackTitles[gRecordTrack>=13 ? gRecordTrack-6 : gRecordTrack/2];
                TB_DrawText(x,y,str);
            } else if(i==1){str=NAME(52,gViewStatType[gTrackType]);TB_DrawText(x,y,str);}
        }
        TB_SetJust(kTB_JustLeft, kTB_JustTop);HAL_SetTextTranslucency(-1.0f);HAL_SetTextDepth(0.0f);
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
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
}

/* A stunt track's stunt totals page: each of the ten kinds of stunt and how many were done, in two columns. */
void recordscr_drawStuntTotals(void)
{
            s32 statIdx, x, y;
            StuntStats *stats;
            u8 str[40];
            if(gStatPlayer)stats=&PROFILE_SAVE->stuntStats[gRecordTrack-RECORD_FIRST_STUNT];
            else stats=&gTotalStuntStats[gRecordTrack-RECORD_FIRST_STUNT];
            HAL_SetFont(11);TB_SetTextColor(kTB_WhiteText);
            for(statIdx=0;statIdx<10;statIdx++) {
                if(statIdx==0) {x=110;y=90;}
                else if(statIdx==5) {x=250;y=90;}
                TB_Sprintf(str,recordscr_stunt_kind_label_format,NAME(37,statIdx));
                TB_DrawText(TB_RightText(str,x-4),y,str);
                TB_Sprintf(str,recordscr_stunt_kind_count_format,stats->totalStunts[statIdx]);
                TB_DrawText(x+4,y,str);y+=TB_GetFontLineHeight();
            }
            ChangePaneSize(recordscr_viewPane,20,86,300,94+TB_GetFontLineHeight()*5);
            }
/* The screen's text blit callback: the select menu's or the view's text. Always 1. */
s32 recordscr_animText(void *callback_context)
{
    HAL_SetTextDepth(0.0f);
    switch(gRecordState) {
    case 0:recordscr_drawSelect();break;     /* kRecordStateSelect */
    case 1:recordscr_drawView();break;       /* kRecordStateView */
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
