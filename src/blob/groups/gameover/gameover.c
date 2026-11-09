#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 opaque[6]; } OSMesgQueue;
typedef struct Voice Voice;

typedef struct {
    s16 id;
    u8 unk2, used;
    u32 payload[5];
} SceneMsg;

typedef struct {
    void *unk0;
    void **single;
    void *unk8;
    u16 *tab;
    void **taba;
} TextTable;

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

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32 x1, s32 y1, s32 x2, s32 y2, s32 style, intptr_t word40, s32 word44, s32 own);

SceneMsg *GetCmd(void);

void RemoveBlit(Voice *);

Voice *NewMultiBlit(s16, s16, const void *, s32);

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
    if (gGameOverState == 1) {
        RemovePanes(0);
    }
    gGameOverState = mode;
    if (gGameOverState == 1) {
        HAL_SetFont(13);
        width = HAL_GetTextWidth(gText.single[63], -1);
        x = gameover_titlePos[0];
        y = gameover_titlePos[1];
        h = TB_GetFontLineHeight();
        InitPane(x - width / 2 - 8, y - 4, width / 2 + x + 8, h + y + 4, 176, 0, 0, 0);
        HAL_SetFont(10);
        width = 0;
        count = 0;
        for (i = 0; i < 7; i++) {
            if (gameover_validOption(i) == 1) {
                y = HAL_GetTextWidth(gText.taba[gText.tab[45] + i], -1);
                count++;
                if (width < y) {
                    width = y;
                }
            }
        }
        h = TB_GetFontLineHeight();
        InitPane(x - width / 2 - 8, 114, width / 2 + x + 8, h * count + 122, 176, 0, 0, 0);
    }
}

void gameover_uninitScreen(void)
{
    Voice *voice;
    setGameOverState(0);
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
#define PC_CMD_SEND(q,m) if((m)!=&pc_scratch_msg&&osSendMesg(q,m,0)!=0)(m)->used=0
void controlMain(void)
{
    SceneMsg *msg;

    if (gActiveEdges & 3) {
        PlayStartABSound(gActiveEdges);
        if (gActiveEdges) {}
        switch (gMainSelect) {
        case 0:
            setGameOverState(2);
            break;
        case 1:
            if (gRace == 2) {
                osRecvMesg(&audio_lock_queue, 0, 1);
                (msg = PC_CMD(GetCmd()))->unk2 = 1;
                osSendMesg(&audio_lock_queue, 0, 0);
                PC_CMD_SEND(&audio_command_queue, msg);
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
                gSkipToGameState = 0x40000;
                gNewGameState = 4;
            } else {
                gameover_uninitScreen();
                gRestartGame = 1;
                osRecvMesg(&audio_lock_queue, 0, 1);
                (msg = PC_CMD(GetCmd()))->unk2 = 7;
                osSendMesg(&audio_lock_queue, 0, 0);
                PC_CMD_SEND(&audio_command_queue, msg);
                AbortGame();
            }
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            osRecvMesg(&audio_lock_queue, 0, 1);
            (msg = PC_CMD(GetCmd()))->unk2 = 1;
            osSendMesg(&audio_lock_queue, 0, 0);
            PC_CMD_SEND(&audio_command_queue, msg);
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
            if (gMainSelect == 2) {
                gSkipToGameState = 0x40;
            } else if (gMainSelect == 3) {
                gSkipToGameState = 0x80;
            } else if (gMainSelect == 4) {
                gSkipToGameState = 0x100;
            } else if (gMainSelect == 5) {
                gSkipToGameState = 0x4000;
            } else if (gMainSelect == 6) {
                gSkipToGameState = 0x10;
            }
            gameover_uninitScreen();
            gNewGameState = 4;
            break;
        }
    } else if (gActiveAutoEdges & 0x400) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (--gMainSelect < 0) {
                gMainSelect = 6;
            }
        } while (gameover_validOption(gMainSelect) == 0);
    } else if (gActiveAutoEdges & 0x800) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (++gMainSelect >= 7) {
                gMainSelect = 0;
            }
        } while (gameover_validOption(gMainSelect) == 0);
    }
}

void updateGameOverScreen(void)
{
    SceneMsg *msg;
    void *entity;
    if (gameover_inited == 0) {
        setGameOverState(1);
        gMainSelect = 0;
        if (!gameover_validOption(0)) {
            do {
                gMainSelect++;
                if (gMainSelect >= 7) {
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
        msg = PC_CMD(GetCmd()); msg->unk2 = 1; osSendMesg(&audio_lock_queue, 0, 0);
        PC_CMD_SEND(&audio_command_queue, msg);
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
        gSkipToGameState = 1;
        gameover_uninitScreen();
        gNewGameState = 4;
        return;
    }
    switch (gGameOverState) {
    case 1: controlMain(); break;
    case 2:
        if (gActiveEdges & 7) {
            PlayStartABSound(4);
            setGameOverState(1);
        }
        break;
    }
}
