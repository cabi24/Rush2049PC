/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Race-to-results transition, 0x800D5A04, 428 bytes. Stops countdown/record
 * voices, resets active car state, flushes entities and sends operation 7 to
 * the loader, then changes state or starts the ghost race.
 *
 * , with the current ShowCountdown
 * symbol name. The genuine internal GetCmd allocator and its complete
 * locked group supply the missing IPA register-reservation information.
 * The transition stays kept. All 13 defined locked/new bodies are members;
 * StopSound is explicitly kept to preserve its native body.
 * No new shaping operation, false return type, filler, or synthetic caller.
 * Existing allocator volatile and helper empty-arm shaping are disclosed
 * in and retained unchanged. This is source/group evidence only.
 */
typedef signed char s8;typedef short s16;typedef unsigned char u8;typedef signed int s32;
typedef struct Vehicle2056 {u8 prefix[1990];s16 resource;u8 tail[64];} Vehicle2056;
typedef struct MessageNode {s16 id;s8 operation,used;float payload[4];void *entity;} MessageNode;
extern MessageNode audiomgr_gCmds[128];
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gNumActiveCars,gRestartGame,gTrackNum;
extern void *multiPlayerBlit,*gEnvList;
extern u8 audio_lock_queue[],audio_command_queue[];
extern s16 gNumWindows;
extern s32 gNewGameState;
void ShowCountdown(s32);
void RemoveHUD(void);
void RemoveBlit(void *);
void End_MDrive(s16);
void modelstop(void);
void StopListeners(void);
void ReleaseEnviron(void *,s32);
void InitSkids(void);
s32 osRecvMesg(void *,void **,s32);
s32 osSendMesg(void *,void *,s32);
MessageNode *GetCmd(void);
void GhostEnd(void);
void UnloadTrack(s32);
void ZOID_SplitMenuScreen(s32);
void updateGameState(void);
void updateTitleScreen();
void ClearRushControls(void);
void StopCarSounds(void *);
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). Abandoning a race then drops its stop of every effect,
 * but nothing else: with no message it is written into a scratch message that is never sent, so the cars, the
 * listeners and the next state are set as before; a message the full queue refuses is given back. */
static MessageNode pc_scratch_msg;
#define PC_CMD_OR_SCRATCH(m) if((m)==0)(m)=&pc_scratch_msg;
#define PC_CMD_SEND(q,m) if((m)!=&pc_scratch_msg&&osSendMesg(q,m,0)!=0)(m)->used=0
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
    if(gRestartGame) gNewGameState=0x40000;
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
