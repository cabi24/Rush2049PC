/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F0914 = GC `_PrintScores` (highscorescr.c, confident pair): fills the five high-score rows
 * of the current track/table. Empty rows (-1) get a random default name and a time derived from the
 * track's default start time (seed = defTime * 6969); otherwise the name comes from the temp-name table
 * (index 0..20) or the player record (+0x14), and the time from gTotalRaceStats[track].scores[table][i].
 * Then the title pane (GC namePanes+0x1b0, N64 BlitPtrs+0x228) is resized around the table's title.
 * Internal (no callee saves); real callers updateHighScoreScreen and func_800F1210. Rand_r (seeded_rand) and
 * HAL_SetFont (font switch) are locked and inlined by umerge. No shaping devices.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

typedef struct Player {                 /* GC struct __anon_3bdf (0x4c) */
    void *next, *prev; void *fileh; u32 id[2];   /* the profile's host layout (pc_prelude.h) */
    char name[13];                      /* +0x14 */
} Player;
typedef struct RaceStats {              /* GC struct __anon_2dfd (0x60) */
    u32 checksum;
    f32 scores[3][5];                   /* +0x04 */
    u8 rest[0x60 - 0x40];
} RaceStats;
typedef struct TextIndices { u8 other[54]; u16 tableNames; } TextIndices;
typedef struct TextState { void *word0; char **fixed; void *word8; TextIndices *indices; char **strings; } TextState;
typedef struct ScoreScreen { u8 other[0x228]; void *titlePane; } ScoreScreen;

extern s8 gBackwards;                   /* gBackwards */
extern s8 gTrackNum;                   /* gTrackNum */
extern s8 gTable;                   /* gTable */
extern s16 gThisTrack;                  /* gThisTrack */
extern Player **gTotalNames[][3][5];     /* gTotalNames */
extern RaceStats gTotalRaceStats[];          /* gTotalRaceStats */
extern f32 gDefStartTimes[];                /* gDefStartTimes */
extern char *gDefaultNames[];              /* gDefaultNames (55 on N64) */
extern char gTempNames[][13];           /* gTempNames */
extern char gScoreTimes[5][10];          /* score strings */
extern char *gScoreNames[5];             /* name pointers */
extern void *scorePanes;              /* GC namePanes title pane */
extern TextState gText;       /* gText */
extern OSMesgQueue text_lock_queue;
extern char default_score_time_format[], score_time_format[];

extern void sprintf(char *, char *, ...);
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
extern s32 HAL_SetFont(s32);          /* font_set wrapper, locked (credits_scroll_grp) */
extern s32 HAL_GetTextWidth(void *, s32);
extern s16 TB_GetFontLineHeight(void);
extern void ChangePaneSize(void *, s32, s32, s32, s32);

extern s32 seeded_rand(u32 *seed);   /* rand_r, locked, inlined by umerge */

void PrintScores(void)
{
    s32 i;
    f32 *s;
    Player ***p;
    f32 defTime;
    u32 seed;
    s32 width;
    s32 height;

    gThisTrack = (gBackwards ? 6 : 0) + gTrackNum;
    p = gTotalNames[gThisTrack][gTable];
    s = gTotalRaceStats[gThisTrack].scores[gTable];
    defTime = gDefStartTimes[(gBackwards ? 19 : 0) + gTrackNum];
    seed = defTime * 6969.0f;
    if (gTable == 1) {
        defTime += 1.0f + (f32)(seeded_rand(&seed) % 2000) / 1000.0f;
    }
    for (i = 0; i < 5; i++) {
        if (*p == (Player **)-1) {
            gScoreNames[i] = gDefaultNames[seeded_rand(&seed) % 55];
            defTime += (f32)(seeded_rand(&seed) % 1000) / 1000.0f;
            sprintf(gScoreTimes[i], default_score_time_format, (u32)defTime / 60, (u32)defTime % 60,
                    (u32)(defTime * 1000.0f) % 1000);
        } else {
            if ((uintptr_t)*p <= 20) {
                gScoreNames[i] = gTempNames[(intptr_t)*p];
            } else {
                gScoreNames[i] = (**p)->name;
            }
            defTime = *s;
            seed = defTime;
            sprintf(gScoreTimes[i], score_time_format, (u32)defTime / 60, (u32)defTime % 60, (u32)(defTime * 1000.0f) % 1000);
        }
        s++;
        p++;
    }
    if (scorePanes != 0) {
        HAL_SetFont(13);
        width = HAL_GetTextWidth(gText.strings[gText.indices->tableNames + gTable], -1);
        height = TB_GetFontLineHeight();
        ChangePaneSize(scorePanes, 160 - width / 2 - 8, 6, width / 2 + 168, height + 14);
    }
}
