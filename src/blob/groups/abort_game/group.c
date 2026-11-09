/* AbortGame: a race abandoned from the pause screen, quit or restarted (MAT3 game.c). */
#include "game/game.h"

/* An audio manager command (GetCmd): its operation (7: stop every effect). */
typedef struct MessageNode {s16 id;s8 operation,used;float payload[4];void *entity;} MessageNode;

VIEW(void *, gEnvList);
#define gEnvList gEnvList_view
VIEW(u8[], audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(u8[], audio_command_queue);
#define audio_command_queue audio_command_queue_view

s32 osRecvMesg(void *,void **,s32);
s32 osSendMesg(void *,void *,s32);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). Abandoning a race then drops its stop of every effect,
 * but nothing else: with no message it is written into a scratch message that is never sent, so the cars, the
 * listeners and the next state are set as before; a message the full queue refuses is given back. */
static MessageNode pc_scratch_msg;
#define PC_CMD_OR_SCRATCH(m) if((m)==0)(m)=&pc_scratch_msg;
#define PC_CMD_SEND(q,m) if((m)!=&pc_scratch_msg&&osSendMesg(q,m,0)!=0)(m)->used=0
/* Abandons the race: the countdown, the HUD and the multiplayer blit go, every car's model stops (End_MDrive), the
 * model thread and the listeners stop, the environment visuals and skid marks go and every sound effect stops. A
 * restart sets the race up again (GS_RACE_SETUP); a quit ends the ghost, unloads the track and goes back to the
 * title screen (state 4) with one window. */
void AbortGame(void)
{
    s16 i;
    void *node;
    MessageNode *message;
    ShowCountdown(0);
    RemoveHUD();
    if(multiPlayerBlit) {
        RemoveBlit(multiPlayerBlit);
        multiPlayerBlit=0;
    }
    for(i=0;i<gNumActiveCars;i++) End_MDrive(PC_MODEL_SLOT(i));
    modelstop();
    StopListeners();
    while((node=gEnvList)!=0) ReleaseEnviron(node,1);
    InitSkids();
    osRecvMesg(audio_lock_queue,0,1);
    node=GetCmd();PC_CMD_OR_SCRATCH(node)
    ((MessageNode *)node)->operation=7;
    message=node;
    osSendMesg(audio_lock_queue,0,0);
    PC_CMD_SEND(audio_command_queue,message);
    if(gRestartGame) gNewGameState=GS_RACE_SETUP;
    else {
        GhostEnd();
        UnloadTrack(gTrackNum);
        gNumWindows=1;
        ZOID_SplitMenuScreen(1);
        gNewGameState=4;
        updateGameState();
        updateTitleScreen();
    }
    ClearRushControls();
}
