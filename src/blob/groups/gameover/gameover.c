/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * gameoverscr.c (post-race menu), N64 game code 0x800F857C-0x800F8D9C.  MAT3 provenance:
 * reference GameSource/src/gameoverscr.c (GC 0x8006DB80 uninitScreen, 0x8006DBCC setGameOverState,
 * controlMain, updateGameOverScreen).  N64 names: setGameOverState = setGameOverState(newState)
 * (newState in s0 by private IPA), gameover_uninitScreen = uninitScreen, controlMain = controlMain
 * (private IPA, s0-s2 unsaved), updateGameOverScreen = updateGameOverScreen (initScreen and controlScore
 * inlined; their deleted-procedure stubs are the locked gameover_initScreen_stub / gameover_controlScore_stub).
 * func_800F84B0 = validOption (locked; must be INTERNAL in the unit so its a3-preserving register
 * summary reaches updateGameOverScreen and controlMain: retail keeps &gMainSelect / the constant 6 in a3
 * across validOption calls).
 *
 * Globals: gGameOverState gGameOverState, gMainSelect gMainSelect, gameover_titlePos titlePos, gameover_inited inited,
 * gameover_blit blit, gameover_multiblit multiblit, gActiveEdges gActiveEdges, gActiveAutoEdges gActiveAutoEdges,
 * gSoftReset gSoftReset, gRace gRace (2 = kGhost), gTrackNum gTrackNum, gEnvList environs
 * list, gNumWindows gNumWindows, multiPlayerBlit the multiplayer-HUD blit, gSkipToGameState gSkipToGameState,
 * gNewGameState gGameState, gRestartGame gRestartGame.  Callees: HAL_GetTextWidth HAL_GetTextWidth,
 * TB_GetFontLineHeight TB_GetFontLineHeight, InitPane InitPane, RemovePanes
 * RemovePanes, RemoveBlit RemoveBlit, NewMultiBlit NewMultiBlit, PlayStartABSound
 * PlayStartABSound, PlayUDSound PlayUDSound, BlinkResurrectingCars BlinkResurrectingCars,
 * UnloadTrack UnloadTrack, ZOID_SplitMenuScreen ZOID_SplitScreen, ClearScreen ClearScreen,
 * AbortGame AbortGame, GetCmd the audio-command allocator (AudKillSong = op 1,
 * AudStopAllEffects = op 7 written in place), setGameState(4) written in place.
 * N64 differences from GC: text table indices single[63] / tab[45]; menu panes at y 114/122 (GC 116/124);
 * the Quit option sets gSkipToGameState = 0x10; RaceAgain (non-ghost) clears the screen through
 * uninitScreen + gRestartGame + AudStopAllEffects + AbortGame.
 *
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * uninitScreen, updateGameOverScreen, controlMain rewritten from the MAT3
 * else-if chain on top of (54/242). HAL_SetFont is the inlined
 * non-static helper font_set_go (+ gfx_lock_go / gfx_unlock_go), not on keep.
 *
 * Disclosed shaping: the compiled-out read `if (gActiveEdges) {}` right
 * PlayStartABSound(gActiveEdges) keeps &gActiveEdges live across that call, so uopt colours the
 * address (retail: lui s0 / addiu s0 / lw a0,0(s0) ahead of the frame setup).  Without it the address
 * folds into the load (241 words) or, laundered, lands in the t6 temp (54-62 words: the temp ring
 * shifts by one register through the whole body).  No unused locals, no stand-in callers.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef void *OSMesg;
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

extern TextTable gText;
extern s32 gGameOverState;          /* gGameOverState */
extern s32 gMainSelect;          /* gMainSelect */
extern OSMesgQueue text_lock_queue;  /* font semaphore */
extern u8 gameover_titlePos[2];        /* titlePos */
extern s8 gameover_inited;           /* inited */
extern Voice *gameover_blit;       /* blit */
extern struct pc_multiblit gameover_multiblit[1];   /* multiblit (pc_prelude.h) */
extern s32 gActiveEdges;          /* gActiveEdges */
extern s32 gActiveAutoEdges;          /* gActiveAutoEdges */
extern s8 gSoftReset;           /* gSoftReset */
extern s32 gRace;       /* gRace */
extern OSMesgQueue audio_lock_queue;
extern OSMesgQueue audio_command_queue;
extern s8 gTrackNum;           /* gTrackNum */
extern void *gEnvList;        /* environs list */
extern s16 gNumWindows;          /* gNumWindows */
extern Voice *multiPlayerBlit;       /* multiplayer HUD blit */
extern s32 gSkipToGameState;          /* gSkipToGameState */
extern s32 gNewGameState;        /* gGameState */
extern u8 gRestartGame;           /* gRestartGame */

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
void RemovePanes(s32 arg0);
s32 HAL_GetTextWidth(void *obj, s32 arg1);
s16 TB_GetFontLineHeight(void);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32 x1, s32 y1, s32 x2, s32 y2, s32 style, intptr_t word40, s32 word44, s32 own);
s32 HAL_SetFontInternal(s32 selection);
s32 gameover_validOption(s32);
void PlayStartABSound(s32 arg0);
SceneMsg *GetCmd(void);
void UnloadTrack(s32 arg0);
void ReleaseEnviron(void *entity, s32 arg1);
void InitSkids(void);
s32 ZOID_SplitMenuScreen(s32 a);
void RemoveBlit(Voice *);
void AbortGame(void);
void PlayUDSound(s32);
Voice *NewMultiBlit(s16, s16, const void *, s32);
void BlinkResurrectingCars(void);
void ClearScreen(unsigned short);

void setGameOverState(s32 mode);
void gameover_uninitScreen(void);
void controlMain(void);

void gfx_lock_go(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_go(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_go(s32 font)
{
    s32 old;

    gfx_lock_go();
    old = HAL_SetFontInternal(font);
    gfx_unlock_go();
    return old;
}

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
        font_set_go(13);
        width = HAL_GetTextWidth(gText.single[63], -1);
        x = gameover_titlePos[0];
        y = gameover_titlePos[1];
        h = TB_GetFontLineHeight();
        InitPane(x - width / 2 - 8, y - 4, width / 2 + x + 8, h + y + 4, 176, 0, 0, 0);
        font_set_go(10);
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
