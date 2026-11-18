/* AbortGame: a race abandoned from the pause screen, quit or restarted (MAT3 game.c). */
#include "game/game.h"

/* An audio manager command (GetCmd): its operation (7: stop every effect). */
typedef struct MessageNode {s16 id;s8 operation,used;float payload[4];void *entity;} MessageNode;



/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). Abandoning a race then drops its stop of every effect,
 * but nothing else: with no message it is written into a scratch message that is never sent, so the cars, the
 * listeners and the next state are set as before; a message the full queue refuses is given back. */
static MessageNode pc_scratch_msg;
#define PC_CMD_OR_SCRATCH(m) if((m)==0)(m)=&pc_scratch_msg;
#define PC_CMD_SEND_UNLESS_SCRATCH(q,m) if((m)!=&pc_scratch_msg&&!queue_try_send(q, m))(m)->used=0
/* Abandons the race: the countdown, the HUD and the multiplayer blit go, every car's model stops (End_MDrive), the
 * model thread and the listeners stop, the environment visuals and skid marks go and every sound effect stops. A
 * restart sets the race up again (GS_GAME_INIT); a quit ends the ghost, unloads the track and goes back to the
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
    lock_take(&audio_lock_queue);
    node=GetCmd();PC_CMD_OR_SCRATCH(node)
    ((MessageNode *)node)->operation=7;
    message=node;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND_UNLESS_SCRATCH(&audio_command_queue,message);
    if(gRestartGame) gNewGameState=GS_GAME_INIT;
    else {
        GhostEnd();
        UnloadTrack(gTrackNum);
        gNumWindows=1;
        ZOID_SplitMenuScreen(1);
        gNewGameState=GS_TITLE;
        updateGameState();
        updateTitleScreen();
    }
    ClearRushControls();
}
