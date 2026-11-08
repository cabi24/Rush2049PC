/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: playerscr_uninitScreen = uninitScreen@playerscr.c (MAT3 GC uninitScreen) now calls
 * uninitButtons(signed char p) (MAT3 playerscr.c) as the non-kept helper playerscr_uninitButtons, defined after its caller (MAT3
 * file order); its deleted procedure is the retail stub 0x803949C4 right before uninitScreen. The button loop moved
 * verbatim apart from MAT3's direct indexing (the earlier `player`/`slot`/`j` carriers are gone; the s8 parameter is
 * MAT3's `signed char p` and is what keeps the code identical: an s32 parameter gives 71/77). Context unchanged. */
typedef signed char s8;typedef unsigned char u8;
typedef short s16;typedef unsigned short u16;typedef int s32;typedef unsigned int u32;typedef float f32;
extern s16 gNumPlayers;
extern s8 playerscr_gSelectPakIdx[],gSelectPlayerNum[],playerscr_gSelectMenuCount[];
extern intptr_t gPakFull[];
extern s32 playerscr_gNumPakPlayers[], gPlayerState[];
extern s8 playerscr_gSelectMenuValid[][14],playerscr_gSelectMenuVisible[][14];

typedef struct Owner Owner;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Item {void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 player;} Item;
typedef struct Selection {Item *item;} Selection;
typedef struct OwnerBody {Owner *next,*previous;Selection *selection;} OwnerBody;
struct Owner {OwnerBody *body;};
typedef struct Player {u8 unaccessed0,id;u16 unaccessed2;u32 flags;u8 unaccessed8[64];Owner *owner;} Player;
typedef struct Blit Blit;typedef struct FiveParts FiveParts;
typedef struct MultiBlit {char *name;s16 x,y,width,height,top,bottom,left,right;u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;} MultiBlit;
extern Player gPlayers[];
extern u8 gCarTeam[];
extern s32 playerscr_gSelectMenuSelect[],gPlayerRects_biased[][24];
extern s8 gSelectMenuMaxSize,screenInited;
extern s16 gNumWindows;
extern f32 playerscr_gButtonZoom,playerscr_gButtonScale,playerscr_gButtonSpace,playerscr_buttonX,playerscr_maxY,playerscr_minY,playerscr_buttonZ,playerscr_cursorZ;
extern FiveParts *playerscr_pane[];
extern MultiBlit multiBlit[];
extern Blit *playerscr_blit;
extern s32 exitPlayerScreen;
extern void PS_LockFile(void **,s8);
extern void playerscr_updatePakInfo(void);
extern void setPlayerState(s32,s32);
extern void *HAL_SetFont(s32);
extern s16 TB_GetFontLineHeight(void);
extern void playerscr_initButtons(s32);
extern void ZOID_SplitScreen(void);
extern void particle_lifetime_set(void);
extern FiveParts *InitPane(s32,s32,s32,s32,s32,s32,s32,s32);
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);
extern void ClearRushControls(void);

extern Owner *gPlayerFiles_head,*gSelectedPlayer[];
extern Owner gJustPlayers[];
extern s8 gSelectMenuControls[],gDeleteMenuSelect[],gDeleteMenuConfirm[],gDeleteMenuYes[],gDeleteMenuNotify[];
extern u8 gCreatePlayerNames[][13];
extern s16 playerscr_gNameSelect[],playerscr_gNameLen[];
extern s32 playerscr_buttonsJustInited[];
extern void *memset(void *,s32,u32);

void setPlayerState(s32 player,s32 mode);

typedef struct PlayerPresence { s8 present; u8 unknown1[15]; } PlayerPresence;
extern PlayerPresence gControllerData[];
extern Owner *gPlayerFiles_head;
extern s32 playerscr_buttonsJustInited[];
extern s8 PS_PakInserted(s32),PS_PakDamaged(s32),PS_PakRumble(s32);
extern u32 PS_GetFreeBlocks(s32);
extern s32 PS_GetBlocks(s32),PS_GetFreeFiles(s32);

void playerscr_updatePakInfo(void);


typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
extern Slot64 playerscr_buttonInfo[][22];
extern s32 playerscr_buttonsInited[];
extern f32 HFOV;
extern void ZOID_RemoveObject(s16),sound_handles_array_clear(FiveParts *);
extern void cleanupControlScreen(void),RemoveBlit(Blit *),removeBG(void);
extern void ZOID_ResetWindows(void),ZOID_Reset(void);

void playerscr_uninitButtons(s8);
void playerscr_uninitScreen(void)
{
    s32 i;
    for(i=0;i<gNumPlayers;i++) {
        playerscr_uninitButtons(i);
        if(playerscr_pane[i]) {
            sound_handles_array_clear(playerscr_pane[i]);
            playerscr_pane[i]=0;
        }
    }
    cleanupControlScreen();
    if(playerscr_blit) {
        RemoveBlit(playerscr_blit);
        playerscr_blit=0;
    }
    removeBG();
    HFOV=90.0f;
    gNumWindows=1;
    ZOID_ResetWindows();
    ZOID_Reset();
    screenInited=0;
}

/* uninitButtons@playerscr.c (MAT3 GC uninitButtons(signed char p)); deleted stub 0x803949C4 */
void playerscr_uninitButtons(s8 p)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) {
        if (playerscr_buttonInfo[p][buttonIdx].id != -1) {
            ZOID_RemoveObject(playerscr_buttonInfo[p][buttonIdx].id);
            playerscr_buttonInfo[p][buttonIdx].id = -1;
        }
    }
    playerscr_buttonsInited[p] = 0;
}

extern u32 gSkipToGameState,gGameState;
extern s8 controlScrActive[],gSoftReset;
extern void player_mode_set(s32,s32),FindPlayerNum(s32,s32);
extern void CalcUnlockedStuff(void),setGameState(s32);
extern s8 PS_PakChanged(s32);
extern void PlayStartABSound(u32),SaveFlush(s32);
extern void PurgePlayerData(Owner *),updateControlScreen(void);
extern s32 ReadPlayerData(Owner *);
extern void SaveSelectPlayer(Owner *);
extern void playerscr_controlSelect(s32),playerscr_updateButtons(s8),controlCreatePlayer(s32),controlDeletePlayer(s32);

