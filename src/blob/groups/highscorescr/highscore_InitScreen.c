#include "game/game.h"
#define PlayerTail PlayerTailView   /* this file's own PlayerTail: include/game's PlayerTail is another type */

typedef struct {
    u8 pad0[12];
    s8 musicVol;        /* +0x0C */
    u8 pad0D;
    s8 song;            /* +0x0E */
} Options;

typedef struct PlayerRec {
    void *next, *prev;
    void *f8;           /* the profile's pak file handle (pc_prelude.h) */
} PlayerRec;

typedef struct {
    PlayerRec **rec;    /* gPlayers[i] + 0x4C */
    u8 pad[72];
} PlayerTail;           /* 76 bytes */

VIEW(Options, gOptions);
#define gOptions gOptions_view

VIEW(u8[], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view

VIEW(u8[], gUnlockedCircuits);
#define gUnlockedCircuits gUnlockedCircuits_view

VIEW(u8[], gUnlockedTrans);
#define gUnlockedTrans gUnlockedTrans_view

VIEW(u8[], gUnlockedEngines);
#define gUnlockedEngines gUnlockedEngines_view

VIEW(u8[], gUnlockedTires);
#define gUnlockedTires gUnlockedTires_view

VIEW(PlayerTail[], gPlayers_save);
#define gPlayers_save gPlayers_save_view

VIEW(OSMesgQueue, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view

extern s32 osSendMesg(OSMesgQueue *, void *, s32);

extern void *memcpy(void *dst, const void *src, s32 n);

/* GC AudPlaySong(song, loop): N64 wrapper over AudPlaySong (hypothesis) */
void AudPlaySong_TTL(s32 song, s32 loop)
{
    AudPlaySong(song, loop);
}

/* GC AudSetMusicVol(vol, rate): N64 wrapper over AudSetMasterVol (hypothesis) */

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
    AudKillSong();
    if (gOptions.song < 13) {
        AudPlaySong_TTL(5, 1);
        AudSetMusicVol(gOptions.musicVol / 10.0f, 0.05f);
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
