/* The game-over screen (MAT3 gameoverscr.c): the menu after a race (score, race again, new mode, track or car,
 * the circuit's next race, quit) and the score page. updateGameOverScreen runs each frame in GS_GAME_OVER; leaving
 * for another screen stops the race's sound, unloads the track and sends the game to the front end with
 * gSkipToGameState saying which screen to open. The menu's and the scores' drawing is results_table.c. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 opaque[6]; } OSMesgQueue;
typedef struct Voice Voice;

/* An audio manager command (GetCmd's message, audiomgr_thread.c's tAudioMsg): only the command byte is set here. */
typedef struct {
    s16 type;
    u8 cmd, used;
    u32 payload[5];
} SceneMsg;

/* The game's texts (gText): single strings, and tables of strings indexed through tab. */
typedef struct {
    void *unk0;
    void **single;
    void *unk8;
    u16 *tab;
    void **taba;
} TextTable;
#define TEXT_GAME_OVER_TITLE 63     /* gText.single[]: the screen's title */

VIEW(TextTable, gText);
#define gText gText_view

VIEW(u8[2], gameover_titlePos);
#define gameover_titlePos gameover_titlePos_view

VIEW(struct pc_multiblit[1], gameover_multiblit);
#define gameover_multiblit gameover_multiblit_view

VIEW(OSMesgQueue, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(OSMesgQueue, audio_command_queue);
#define audio_command_queue audio_command_queue_view

VIEW(void *, gEnvList);
#define gEnvList gEnvList_view

VIEW(Voice *, multiPlayerBlit);
#define multiPlayerBlit multiPlayerBlit_view
VIEW(s32, gSkipToGameState);
#define gSkipToGameState gSkipToGameState_view

VIEW(u8, gRestartGame);
#define gRestartGame gRestartGame_view

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);

/* Switch the screen to state mode (kGameOverState*). Leaving the menu removes its panes; entering it puts up a box
 * for the title and one sized to the widest of the rows gameover_validOption offers. */
void setGameOverState(s32 mode)
{
    s32 width;
    s32 x;
    s32 y;
    s32 h;
    s32 count;
    s32 i;

    if (mode == gGameOverState) {
        return;
    }
    if (gGameOverState == kGameOverStateMain) {
        RemovePanes(0);
    }
    gGameOverState = mode;
    if (gGameOverState == kGameOverStateMain) {
        HAL_SetFont(13);
        width = HAL_GetTextWidth(gText.single[TEXT_GAME_OVER_TITLE], -1);
        x = gameover_titlePos[0];
        y = gameover_titlePos[1];
        h = TB_GetFontLineHeight();
        InitPane(x - width / 2 - 8, y - 4, width / 2 + x + 8, h + y + 4, PANE_STYLE, 0, 0, 0);
        HAL_SetFont(10);
        width = 0;
        count = 0;
        for (i = 0; i < kGameOverMainMenuCount; i++) {
            if (gameover_validOption(i) == 1) {
                y = HAL_GetTextWidth(gText.taba[gText.tab[TEXT_GAME_OVER_MENU] + i], -1);
                count++;
                if (width < y) {
                    width = y;
                }
            }
        }
        h = TB_GetFontLineHeight();
        InitPane(x - width / 2 - 8, 114, width / 2 + x + 8, h * count + 122, PANE_STYLE, 0, 0, 0);
    }
}

/* Take the screen down: no state, its panes and its text blit gone; the next update sets it up again. */
void gameover_uninitScreen(void)
{
    Voice *voice;
    setGameOverState(kGameOverStateNone);
    RemovePanes(1);
    voice = gameover_blit;
    if (voice) {
        RemoveBlit(voice);
        gameover_blit = 0;
    }
    gameover_inited = 0;
}

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song or of every effect posted on the
 * way out of a race is then dropped, but nothing else is: with no message it is written into a scratch message
 * that is never sent, so the cars, the effects' emitters and the next state are set as before; a message the
 * full queue refuses is given back. */
static SceneMsg pc_scratch_msg;
static SceneMsg *pc_cmd(SceneMsg *m) { return m ? m : &pc_scratch_msg; }
#define PC_CMD(m) pc_cmd(m)
#define PC_CMD_SEND_UNLESS_SCRATCH(q,m) if((m)!=&pc_scratch_msg&&osSendMesg(q,m,0)!=0)(m)->used=0

/* The menu's input. Start or A takes the row:
 * - score: the score page;
 * - race again: after a ghost race the game goes back through the race's set-up (GS_GAME_INIT, so the ghost is
 *   loaded again); else the race restarts (gRestartGame) with every effect stopped;
 * - new mode, track, car, the circuit's next race, quit: the music stops, the track and the race's visuals go, and
 *   the front end opens at that screen (gSkipToGameState: GS_MODE, GS_TRACK, GS_CAR, GS_CIRCUIT, GS_START).
 * Up and down (auto-repeating) move between the offered rows, wrapping round. */
void controlMain(void)
{
    SceneMsg *msg;

    if (gActiveEdges & BUTTON_ACCEPT) {
        PlayStartABSound(gActiveEdges);
        switch (gMainSelect) {
        case kGameOverMainMenuScore:
            setGameOverState(kGameOverStateScore);
            break;
        case kGameOverMainMenuRaceAgain:
            if (gRace == kGhost) {
                osRecvMesg(&audio_lock_queue, 0, 1);
                (msg = PC_CMD(GetCmd()))->cmd = kAudioCmdKillSong;
                osSendMesg(&audio_lock_queue, 0, 0);
                PC_CMD_SEND_UNLESS_SCRATCH(&audio_command_queue, msg);
                UnloadTrack(gTrackNum);
                while (gEnvList != 0) {
                    ReleaseEnviron(gEnvList, 1);
                }
                InitSkids();
                gNumWindows = 1;
                ZOID_SplitMenuScreen(1);
                if (multiPlayerBlit != 0) {
                    RemoveBlit(multiPlayerBlit);
                    multiPlayerBlit = 0;
                }
                gameover_uninitScreen();
                gSkipToGameState = GS_GAME_INIT;
                gNewGameState = GS_TITLE;
            } else {
                gameover_uninitScreen();
                gRestartGame = 1;
                osRecvMesg(&audio_lock_queue, 0, 1);
                (msg = PC_CMD(GetCmd()))->cmd = kAudioCmdStopAllEffects;
                osSendMesg(&audio_lock_queue, 0, 0);
                PC_CMD_SEND_UNLESS_SCRATCH(&audio_command_queue, msg);
                AbortGame();
            }
            break;
        case kGameOverMainMenuNewMode:
        case kGameOverMainMenuNewTrack:
        case kGameOverMainMenuNewCar:
        case kGameOverMainMenuNextRace:
        case kGameOverMainMenuQuit:
            osRecvMesg(&audio_lock_queue, 0, 1);
            (msg = PC_CMD(GetCmd()))->cmd = kAudioCmdKillSong;
            osSendMesg(&audio_lock_queue, 0, 0);
            PC_CMD_SEND_UNLESS_SCRATCH(&audio_command_queue, msg);
            UnloadTrack(gTrackNum);
            while (gEnvList != 0) {
                ReleaseEnviron(gEnvList, 1);
            }
            InitSkids();
            gNumWindows = 1;
            ZOID_SplitMenuScreen(1);
            if (multiPlayerBlit != 0) {
                RemoveBlit(multiPlayerBlit);
                multiPlayerBlit = 0;
            }
            if (gMainSelect == kGameOverMainMenuNewMode) {
                gSkipToGameState = GS_MODE;
            } else if (gMainSelect == kGameOverMainMenuNewTrack) {
                gSkipToGameState = GS_TRACK;
            } else if (gMainSelect == kGameOverMainMenuNewCar) {
                gSkipToGameState = GS_CAR;
            } else if (gMainSelect == kGameOverMainMenuNextRace) {
                gSkipToGameState = GS_CIRCUIT;
            } else if (gMainSelect == kGameOverMainMenuQuit) {
                gSkipToGameState = GS_START;
            }
            gameover_uninitScreen();
            gNewGameState = GS_TITLE;
            break;
        }
    } else if (gActiveAutoEdges & BUTTON_UP) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (--gMainSelect < 0) {
                gMainSelect = kGameOverMainMenuCount - 1;
            }
        } while (gameover_validOption(gMainSelect) == 0);
    } else if (gActiveAutoEdges & BUTTON_DOWN) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (++gMainSelect >= kGameOverMainMenuCount) {
                gMainSelect = 0;
            }
        } while (gameover_validOption(gMainSelect) == 0);
    }
}

/* The screen's frame (GS_GAME_OVER). The first frame puts up the menu on its first offered row and the text blit.
 * The resurrecting cars keep blinking behind it. A soft reset stops the music, unloads the track and restarts the
 * game from the title (SKIP_NONE: no screen to skip to). Then the menu's input, or on the score page any
 * of Start, A or B goes back to the menu. */
void updateGameOverScreen(void)
{
    SceneMsg *msg;
    void *entity;
    if (gameover_inited == 0) {
        setGameOverState(kGameOverStateMain);
        gMainSelect = 0;
        if (!gameover_validOption(0)) {
            do {
                gMainSelect++;
                if (gMainSelect >= kGameOverMainMenuCount) {
                    gMainSelect = 0;
                    break;
                }
            } while (!gameover_validOption(gMainSelect));
        }
        gameover_blit = NewMultiBlit(0, 0, gameover_multiblit, 1);
        gameover_inited = 1;
    }
    BlinkResurrectingCars();
    if (gSoftReset) {
        osRecvMesg(&audio_lock_queue, 0, 1);
        msg = PC_CMD(GetCmd()); msg->cmd = kAudioCmdKillSong; osSendMesg(&audio_lock_queue, 0, 0);
        PC_CMD_SEND_UNLESS_SCRATCH(&audio_command_queue, msg);
        UnloadTrack(gTrackNum);
        entity = gEnvList;
        if (entity) {
            do {
                ReleaseEnviron(entity, 1);
                entity = gEnvList;
            } while (entity);
        }
        InitSkids();
        gNumWindows = 1;
        ZOID_SplitMenuScreen(1);
        if (multiPlayerBlit) {
            RemoveBlit(multiPlayerBlit);
            multiPlayerBlit = 0;
        }
        ClearScreen(1);
        gSkipToGameState = SKIP_NONE;
        gameover_uninitScreen();
        gNewGameState = GS_TITLE;
        return;
    }
    switch (gGameOverState) {
    case kGameOverStateMain: controlMain(); break;
    case kGameOverStateScore:
        if (gActiveEdges & (BUTTON_ACCEPT | BUTTON_B)) {
            PlayStartABSound(BUTTON_B);
            setGameOverState(kGameOverStateMain);
        }
        break;
    }
}
