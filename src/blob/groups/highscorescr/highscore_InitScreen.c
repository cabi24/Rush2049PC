/* The high score screen's start (MAT3 highscorescr.c _InitScreen): the music, the unlocks this race earned, the
 * circuit's progress, and the first page to show. */
#include "game/game.h"

extern s32 osSendMesg(OSMesgQueue *, void *, s32);

extern void *memcpy(void *dst, const void *src, s32 n);

/* AudPlaySong. */
void AudPlaySong_TTL(s32 song, s32 loop)
{
    AudPlaySong(song, loop);
}

/* Where MAT3 calls SaveFlush(0) after ClearRushControls: empty on the N64. */
void SaveFlush_TTL(s32 x)
{
}

/* Starts the high score screen: the high score music (song 5) unless the player chose no music, a copy of the
 * unlock tables to compare with, the game over records (RecordGameOver); for a finished circuit race, the
 * circuit's standings and, for a player without a profile, a circuit password; then the unlocks (each player's
 * gStuffUnlocked when any table changed), the scores ranked for a race, ghost or circuit, the controls cleared, and
 * the first page: the unlocks, the circuit password, the scores, or straight out. */
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
    if (gOptions[kOptSong] < 13) {
        AudPlaySong_TTL(5, 1);
        AudSetMusicVol(gOptions[kOptMusicVolume] / 10.0f, 0.05f);
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
    if (gRace == kCircuit && PC_CAR_PLACE_LOCKED(gCircuitPlayer)) {
        for (i = 0; i < 6; i++) {
            ranks[i] = PC_CAR_PLACE(i);
        }
        oldCircuitTrack = gCircuitTrack;
        UpdateCircuit(ranks, PC_CAR_SCORE(gCircuitPlayer));
        gCircuitTrack = oldCircuitTrack;
        if ((*gPlayers_save[gCircuitPlayer].profile)->fileh == NULL && gCircuitTrack < gCircuitSize - 1) {
            circuitCode = 1;
            GetCircuitCode(highscore_gCircuitCode);
        }
    }
    CalcUnlockedStuff();
    for (p = 0; p < gNumPlayers; p++) {
        gStuffUnlocked[p] = game_memcmp(gPrevUnlockedTracks[p], gUnlockedTracks[p], 19) != 0
                     || game_memcmp(gPrevUnlockedCars[p], &gUnlockedCars[p * 13], 13) != 0
                     || game_memcmp(gPrevUnlockedCircuits[p], gUnlockedCircuits[p], 4) != 0
                     || game_memcmp(gPrevUnlockedTrans[p], gUnlockedTrans[p], 8) != 0
                     || game_memcmp(gPrevUnlockedEngines[p], gUnlockedEngines[p], 9) != 0
                     || game_memcmp(&gPrevUnlockedTires[p * 5], gUnlockedTires[p], 5) != 0
                     || game_memcmp(gPrevUnlockedFrames[p], &gUnlockedFrames[p * 6], 6) != 0;
        if (gStuffUnlocked[p]) {
            stuffUnlocked = 1;
        }
        gDrawName[p] = 0;
    }
    gThisTrack = (gBackwards ? 6 : 0) + gTrackNum;
    if (gRace == kOneRace || gRace == kGhost || gRace == kCircuit) {
        RankScores();
    }
    ClearRushControls();
    SaveFlush_TTL(0);
    if (stuffUnlocked) {
        SetHighScoreState(1);
    } else if ((s8)circuitCode) {
        SetHighScoreState(2);
    } else if (gRace == kOneRace || gRace == kCircuit || gRace == kGhost) {
        SetHighScoreState(3);
    } else {
        highscore_UninitScreen();
    }
}

