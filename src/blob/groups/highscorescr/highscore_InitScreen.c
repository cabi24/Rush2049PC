/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * track_texture_load (0x800F5F90, 1,348 B) = GC highscorescr.c `static void _InitScreen(void)` (high-score screen
 * init; found it from the unlock script).
 * Whole-program unit: EQUAL (337 words, frame 120/120) with `--internal track_texture_load --internal AudKillSong_TTL
 * --internal AudSetMusicVol_TTL --internal AudPlaySong_TTL --internal SaveFlush_TTL`; `the scorer` MATCH inside
 * updateHighScoreScreen (its only caller updateHighScoreScreen is there; IPA-internal: retail saves none of s0-s8).
 * Inlined helpers (non-static, force_internal, not kept; every inlined call reserves 8 B of frame here):
 *   - AudKillSong_TTL = the body of the locked -O2 single AudKillSong (GC AudKillSong; audio msg cmd 1),
 *     which retail inlines (msg kept in s0); `msg = GetCmd(); msg->cmd = 1;` on one line (as1 then puts
 *     `move s0,v0` after the store, as retail) and defined AFTER track_texture_load (its .loc lines put `li a2,1`
 *     in the osRecvMesg delay slot, as retail).
 *   - AudSetMusicVol_TTL(vol, rate) -> AudSetMasterVol(vol, rate, 1, 0): the same body as the LOCKED AudSetMusicVol
 * (vsync; = GC AudSetMusicVol). Calling AudSetMusicVol itself is EQUAL in the unit too, but the
 *     group compile then schedules the 0.05f lwc1 into the jal delay slot (4 words); the in-file copy matches both.
 *   - AudPlaySong_TTL(song, loop) -> AudPlaySong(song, loop): HYPOTHESIS (GC AudPlaySong; the locked
 *     AudPlaySong is an -O2 single, retail calls it by jal through this 8-B inline level).
 * SHAPING DEVICES (disclosed): (1) SaveFlush_TTL(0): an EMPTY inlined helper after ClearRushControls (= GC
 *   ClearRushControls; GC then calls SaveServer(1, 0); SaveFlush(0), the N64 has no code there) - identity
 *   unknown, it supplies one 8-B inline area; (2) unused local `j`: exact frame residual (retail 120; without it
 *   112; 1 named slot above stuffUnlocked, +8 with alignment); (3) `(s8)circuitCode` in the circuitCode test:
 *   circuitCode/inited are declared u8 so their `= 1` stores share one constant web (retail s3) apart from the
 *   s32 1 arguments, while retail reads circuitCode with lb.
 * Other levers: memcmp (game_memcmp) size parameter u32 (separates the size constants 13/9/5/6 from the s32
 * multipliers kept in s5-s8); a separate counter `i` for the 6-entry ranks loop (IDO unrolls it 2 + 4).
 * GC-derived names: AudKillSong, AudPlaySong, AudSetMusicVol =
 * AudSetMusicVol, gPrevUnlockedTracks.. = namePanes+0x90.. copies of gUnlockedTracks/Cars/Circuits/Trans/Engines/Tires/
 * Frames (gUnlockedTracks/E30/E88/EB8/ED8/F00/F40), RecordGameOver, highscore_inited = inited,
 * circuitCode, gCircuitPlayer, gCircuitTrack, gCircuitSize,
 * gGameCar = gGameCar (+0xEE place, +0xEF finished, +0xF0 time), gPlayers_save = gPlayers[].+0x4C (76-B stride),
 * UpdateCircuit = UpdateCircuit(ranks, time), GetCircuitCode = GetCircuitCode(namePanes+0x190 = highscore_gCircuitCode),
 * CalcUnlockedStuff, game_memcmp = memcmp, gStuffUnlocked, gDrawName =
 * gDrawName, gThisTrack, RankScores = _RankScores, ClearRushControls,
 * highscore_UninitScreen = _UninitScreen, gSavedGhost, gOptions = gOptions (+0xC musicVol, +0xE song).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    u8 pad0[12];
    s8 musicVol;        /* +0x0C */
    u8 pad0D;
    s8 song;            /* +0x0E */
} Options;

typedef struct {
    u8 pad0[0xEE];
    s8 place;           /* +0xEE */
    s8 finished;        /* +0xEF */
    f32 time;           /* +0xF0 */
    u8 padF4[0x110 - 0xF4]; struct pc_visual visuals[21]; u8 padF4_308[0x380 - 0x308]; void *input; u8 padF4_384[0x3B8 - 0x384];
} GameCar;              /* 952 bytes */

typedef struct {
    void *next, *prev;
    void *f8;           /* the profile's pak file handle (pc_prelude.h) */
} PlayerRec;

typedef struct {
    PlayerRec **rec;    /* gPlayers[i] + 0x4C */
    u8 pad[72];
} PlayerTail;           /* 76 bytes */

extern void *gSavedGhost;
extern Options gOptions;
extern u8 gPrevUnlockedTracks[];
extern u8 gUnlockedTracks[];
extern u8 gPrevUnlockedCars[];
extern u8 gUnlockedCars[];
extern u8 gPrevUnlockedCircuits[];
extern u8 gUnlockedCircuits[];
extern u8 gPrevUnlockedTrans[];
extern u8 gUnlockedTrans[];
extern u8 gPrevUnlockedEngines[];
extern u8 gUnlockedEngines[];
extern u8 gPrevUnlockedTires[];
extern u8 gUnlockedTires[];
extern u8 gPrevUnlockedFrames[];
extern u8 gUnlockedFrames[];
extern s32 gHighScoreState;
extern u8 highscore_inited;               /* inited */
extern u8 circuitCode;
extern s32 gRace;
extern u8 gCircuitPlayer;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gCircuitTrack;
extern PlayerTail gPlayers_save[];
extern s8 gCircuitSize;
extern u8 highscore_gCircuitCode[];
extern s16 gNumPlayers;
extern s8 gStuffUnlocked[];             /* gStuffUnlocked */
extern s8 gDrawName[];             /* gDrawName */
extern s8 gBackwards;
extern s8 gTrackNum;
extern s16 gThisTrack;

typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { u8 pad0[2]; s8 cmd; } AudMsg;
extern OSMesgQueue audio_lock_queue;
extern OSMesgQueue audio_command_queue;
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern AudMsg *GetCmd(void);

void AudKillSong_TTL(void);
extern void AudPlaySong(s32 song, s32 loop);
extern void AudSetMasterVol(f32 blend, f32 amount, s32 first, s32 second);
extern void *memcpy(void *dst, const void *src, s32 n);
extern s32 RecordGameOver();
extern void UpdateCircuit(u8 *ranks, f32 time);
extern void GetCircuitCode(u8 *code);
extern void CalcUnlockedStuff(void);
extern s32 game_memcmp(void *a, void *b, u32 n);
extern void RankScores(void);
extern void ClearRushControls(void);
extern void SetHighScoreState(s32);
extern void highscore_UninitScreen(void);

/* GC AudPlaySong(song, loop): N64 wrapper over AudPlaySong (hypothesis) */
void AudPlaySong_TTL(s32 song, s32 loop)
{
    AudPlaySong(song, loop);
}

/* GC AudSetMusicVol(vol, rate): N64 wrapper over AudSetMasterVol (hypothesis) */
void AudSetMusicVol_TTL(f32 vol, f32 rate)
{
    AudSetMasterVol(vol, rate, 1, 0);
}

/* GC SaveFlush(0) after ClearRushControls: empty on the N64? (frame hypothesis) */
void SaveFlush_TTL(s32 x)
{
}

void highscore_InitScreen(void)
{
    s32 p;
    s32 i;
    s32 j;
    s32 stuffUnlocked;
    s32 oldCircuitTrack;
    u8 ranks[6];

    stuffUnlocked = 0;
    gSavedGhost = 0;
    AudKillSong_TTL();
    if (gOptions.song < 13) {
        AudPlaySong_TTL(5, 1);
        AudSetMusicVol_TTL(gOptions.musicVol / 10.0f, 0.05f);
    }
    memcpy(gPrevUnlockedTracks, gUnlockedTracks, 76);
    memcpy(gPrevUnlockedCars, gUnlockedCars, 52);
    memcpy(gPrevUnlockedCircuits, gUnlockedCircuits, 16);
    memcpy(gPrevUnlockedTrans, gUnlockedTrans, 32);
    memcpy(gPrevUnlockedEngines, gUnlockedEngines, 36);
    memcpy(gPrevUnlockedTires, gUnlockedTires, 20);
    memcpy(gPrevUnlockedFrames, gUnlockedFrames, 24);
    RecordGameOver();
    gHighScoreState = 0;
    highscore_inited = 1;
    circuitCode = 0;
    if (gRace == 3 && PC_CAR_FINISHED(gCircuitPlayer)) {
        for (i = 0; i < 6; i++) {
            ranks[i] = PC_CAR_PLACE(i);
        }
        oldCircuitTrack = gCircuitTrack;
        UpdateCircuit(ranks, PC_CAR_TIME(gCircuitPlayer));
        gCircuitTrack = oldCircuitTrack;
        if ((*gPlayers_save[gCircuitPlayer].rec)->f8 == 0 && gCircuitTrack < gCircuitSize - 1) {
            circuitCode = 1;
            GetCircuitCode(highscore_gCircuitCode);
        }
    }
    CalcUnlockedStuff();
    for (p = 0; p < gNumPlayers; p++) {
        gStuffUnlocked[p] = game_memcmp(&gPrevUnlockedTracks[p * 19], &gUnlockedTracks[p * 19], 19) != 0
                     || game_memcmp(&gPrevUnlockedCars[p * 13], &gUnlockedCars[p * 13], 13) != 0
                     || game_memcmp(&gPrevUnlockedCircuits[p * 4], &gUnlockedCircuits[p * 4], 4) != 0
                     || game_memcmp(&gPrevUnlockedTrans[p * 8], &gUnlockedTrans[p * 8], 8) != 0
                     || game_memcmp(&gPrevUnlockedEngines[p * 9], &gUnlockedEngines[p * 9], 9) != 0
                     || game_memcmp(&gPrevUnlockedTires[p * 5], &gUnlockedTires[p * 5], 5) != 0
                     || game_memcmp(&gPrevUnlockedFrames[p * 6], &gUnlockedFrames[p * 6], 6) != 0;
        if (gStuffUnlocked[p]) {
            stuffUnlocked = 1;
        }
        gDrawName[p] = 0;
    }
    gThisTrack = (gBackwards ? 6 : 0) + gTrackNum;
    if (gRace == 0 || gRace == 2 || gRace == 3) {
        RankScores();
    }
    ClearRushControls();
    SaveFlush_TTL(0);
    if (stuffUnlocked) {
        SetHighScoreState(1);
    } else if ((s8)circuitCode) {
        SetHighScoreState(2);
    } else if (gRace == 0 || gRace == 3 || gRace == 2) {
        SetHighScoreState(3);
    } else {
        highscore_UninitScreen();
    }
}

/* = AudKillSong (GC AudKillSong), inlined by umerge in retail (its locked copy is an -O2 single) */
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song (command 1) posted here
 * is then dropped: with no message the lock is given back and nothing is sent, and a message the full queue
 * refuses is given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){osSendMesg(&audio_lock_queue,(void *)0,0);return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)((s8 *)(m))[3]=0
void AudKillSong_TTL(void)
{
    AudMsg *msg;

    osRecvMesg(&audio_lock_queue, (void **)0, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg) msg->cmd = 1;
    osSendMesg(&audio_lock_queue, (void *)0, 0);
    PC_CMD_SEND(&audio_command_queue, msg);
}
