/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: updatePlayerScreen updatePlayerScreen (MAT3 playerscr.c, GC 0x80079f80) becomes a
 * claimed member, with the four deleted-inline stubs of its switch: controlWait = controlWait (case 1),
 * controlControls = controlControls (case 2), controlPakFull = controlPakFull (case 5), controlPakDamaged =
 * controlPakDamaged (case 6), written as MAT3-style helpers (non-static, not kept) and emitted at their retail slots.
 * The helper form needs no unused locals and no `q` (the root's cfe locals area is 64 = retail: 36 B of root locals
 * plus the four 8-byte inlined-call areas). Disclosed shaping in updatePlayerScreen, both code-free:
 * 1. (installed) `do {} while (0);` between `if (!screenInited) initScreen;` and the player loop:
 *      the extra block boundary lets the gNumPlayers preheader load be coloured a1 as in retail.
 * 2. the loop body starts `p = playerIdx; do {} while (0); switch (gPlayerState[p])`: the switch reads the
 *      state array through a copy that only global copy propagation resolves, so uopt creates the
 *      &gPlayerState[playerIdx] expression late (its itable bit comes from the allReady loop, after the case
 *      expressions); f_spilltemps then gives that walking pointer the third temp home (sp+116) and the reset-loop
 *      pointer the first (sp+124), frame 192 = retail. With the expression created at the switch (bit 53, before
 *      the case webs) the pointer takes the second home (frame 184, 22 words). The cases still pass playerIdx.
 * Every old member is unchanged and still strict MATCH. */
/* superseding revision: claims the two deleted-inline stubs whose helpers this unit already defines:
 * calcSelectMenuMaxSize = calcSelectMenuMaxSize (inlined into initScreen playerscr_initScreen) and claimPlayer, renamed from
 * controlWait to claimPlayer: the compiled object emits its deleted procedure right after controlDeletePlayer
 * controlDeletePlayer and before controlCreatePlayer controlCreatePlayer, i.e. at retail 0x803968B8 (post-order: controlCreatePlayer
 * calls claimPlayer, controlSelect's later calls find it visited). Retail 0x80396ED8/0x80396EE0 are the stubs of
 * controlControls/controlWait, inlined into the unmatched root updatePlayerScreen (not claimed). No body changed. */
/* Image A player-select screen (MAT3 GameSource playerscr.c), one IPA unit rooted at the kept updatePlayerScreen
 * (updatePlayerScreen, GC 0x80079f80). Claimed members (all strict in this group):
 *   playerscr_updateSelectInfo updateSelectInfo, playerscr_updatePakInfo updatePakInfo, setPlayerState setPlayerState, playerscr_initButtons
 * initButtons (the bodies, unchanged); playerscr_initScreen initScreen (GC 0x8007a49c); controlCreatePlayer
 *   controlCreatePlayer (GC 0x8007b464); controlDeletePlayer controlDeletePlayer (GC 0x8007b9f8); playerscr_updateButtons
 *   updateButtons(signed char) (GC 0x8007fc14). Context only: updatePlayerScreen (122/451 words), playerscr_controlSelect
 *   controlSelect (GC 0x8007a854, 262/718), playerscr_uninitScreen uninitScreen, playerClaimed playerClaimed,
 *   playerBeingDeleted / playerBeingCreated (installed exact bodies, copied verbatim).
 * Donor: MAT3 playerscr.c, direct typed ports; N64 differences are from the retail words: 14 menu items and 22
 * buttons (GC 15 / 23), no gCreateMenuNotify, B deletes a character in name entry, a pak-changed test in the
 * wait state, a "controls" arrow button (index 14) next to the selected item, 0.0f lower angle limit, and
 * gFrameSeconds / gNumModels are volatile.
 * Inlined statics (their 8-byte deleted stubs are non-static symbols, not on keep): calcSelectMenuMaxSize =
 * calcSelectMenuMaxSize (inlined into initScreen), claimPlayer = claimPlayer (inlined 4x into controlSelect and 1x
 * into controlCreatePlayer). Their parameters are what the frame needs: defining them as helpers replaced the
 * `screens` local of the earlier research body and two unused locals.
 * Shaping devices (each needed for the exact frame; disclosed per the rule):
 *   playerscr_updateButtons: two unused s32 locals before prevPos (frame 152 -> 160, locals at sp+104/110/119/132).
 * updatePlayerScreen: see the note at the top (no unused locals any more).
 * Float zero in the type-9 button x is `0 * gButtonScale` (int zero; a float zero is folded by uopt), 255.f and
 * 255.0f are deliberately different spellings (one shared 255 constant is wrongly CSE'd).
 * All source names are reconstructions; none is asserted as original.
 * : playerscr_controlSelect = controlSelect@playerscr.c (MAT3 GC 0x8007a854) becomes a claimed member. Its body
 * is unchanged from except one compiled-out statement `do {} while (0);` between the edges chain and the
 * autoFlags chain (SHAPING, disclosed; plausibly an empty release-build macro). The extra block boundary changes the
 * colouring order: the constant-13 web (whole-web save 0.5758 vs the lda gSelectMenuControls web 0.44, both split) no longer
 * reaches a3 first, so the first-loop lda piece takes a3 as in retail (traced: forcing the 13 web to split gives the
 * same object). Own .rodata = its 14-entry jump table (retail 0x803B9484), entries hand-checked equal.
 * Context updatePlayerScreen (kept root, not claimed) gets the same kind of compiled-out `do {} while (0);` before the
 * main player loop: its gNumPlayers preheader load is then coloured a1 as in retail (122 -> 2 words; the last 2 are
 * the spill home of the reset loop's walking pointer, sp+120 vs retail sp+124).
 */
typedef signed char s8;typedef unsigned char u8;
typedef short s16;typedef unsigned short u16;typedef int s32;typedef unsigned int u32;typedef float f32;
extern s16 gNumPlayers;
extern s8 playerscr_gSelectPakIdx[],gSelectPlayerNum[],playerscr_gSelectMenuCount[];
extern intptr_t gPakFull[];
extern s32 playerscr_gNumPakPlayers[], gPlayerState[];
extern s8 playerscr_gSelectMenuValid[][14],playerscr_gSelectMenuVisible[][14];

void playerscr_updateSelectInfo(s32 player)
{
    s32 first = player;
    s32 last = player;
    s32 j;
    if (player == -1) {first = 0;last = gNumPlayers - 1;}
    for (player = first; player <= last; player++) {
        gSelectPlayerNum[player] = playerscr_gNumPakPlayers[playerscr_gSelectPakIdx[player]];
        playerscr_gSelectMenuValid[player][0] = 1;
        playerscr_gSelectMenuValid[player][1] = 1;
        playerscr_gSelectMenuValid[player][2] = 0;
        for (j = 0; j < 8; j++)
            playerscr_gSelectMenuValid[player][j + 3] = j < gSelectPlayerNum[player];
        playerscr_gSelectMenuValid[player][11] = gSelectPlayerNum[player] >= 0 && !gPakFull[playerscr_gSelectPakIdx[player]];
        playerscr_gSelectMenuValid[player][12] = gSelectPlayerNum[player] > 0;
        playerscr_gSelectMenuValid[player][13] = 0;
        if (gPlayerState[player] == 4) {
            playerscr_gSelectMenuValid[player][0] = 0;
            playerscr_gSelectMenuValid[player][1] = 0;
            playerscr_gSelectMenuValid[player][11] = 0;
            playerscr_gSelectMenuValid[player][12] = 0;
        }
        playerscr_gSelectMenuVisible[player][0] = playerscr_gSelectMenuValid[player][0];
        playerscr_gSelectMenuVisible[player][1] = gPlayerState[player] == 4 || playerscr_gSelectMenuValid[player][1];
        playerscr_gSelectMenuVisible[player][2] = gSelectPlayerNum[player] < 1 || (gPakFull[playerscr_gSelectPakIdx[player]] && gPlayerState[player] != 4);
        for (j = 0; j < 8; j++) playerscr_gSelectMenuVisible[player][j + 3] = playerscr_gSelectMenuValid[player][j + 3];
        playerscr_gSelectMenuVisible[player][11] = playerscr_gSelectMenuValid[player][11];
        playerscr_gSelectMenuVisible[player][12] = playerscr_gSelectMenuValid[player][12];
        playerscr_gSelectMenuVisible[player][13] = playerscr_gSelectMenuValid[player][13];
        playerscr_gSelectMenuCount[player] = 0;
        for (j = 0; j < 14; j++) if (playerscr_gSelectMenuVisible[player][j]) playerscr_gSelectMenuCount[player]++;
    }
}

typedef struct Owner Owner;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10); player is its pak */
typedef struct Item {void *next,*prev;void (*RemoveFile)(void *),(*MoveFile)(void *,void *);u8 player;} Item;
typedef struct Selection {Item *item;} Selection;
typedef struct OwnerBody {Owner *next,*previous;Selection *selection;u8 unknown0C[8];u8 name[1];} OwnerBody;
struct Owner {OwnerBody *body;};
typedef struct Player {u8 unaccessed0,id;u16 unaccessed2;u32 flags;u32 unaccessed8;u32 autoFlags;u8 unaccessed10[56];Owner *owner;} Player;
typedef struct Blit Blit;typedef struct FiveParts FiveParts;
typedef struct MultiBlit {char *name;s16 x,y,width,height,top,bottom,left,right;u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;} MultiBlit;
extern Player gPlayers[];
extern u8 gCarTeam[];
extern s32 playerscr_gSelectMenuSelect[];
/* Each player's menu rectangle (playerscr_draw.c's Layout), by number of players less one and player. */
typedef struct Layout { s32 unknown0,y,width,height,x,unknown14; } Layout;
extern Layout playerscr_gPlayerRects[][4];
extern s8 gSelectMenuMaxSize,screenInited;
extern s16 gNumWindows;
extern f32 playerscr_gButtonZoom,playerscr_gButtonScale,playerscr_gButtonSpace,playerscr_buttonX,playerscr_maxY,playerscr_minY,playerscr_buttonZ,playerscr_cursorZ;
extern FiveParts *playerscr_pane[];
extern MultiBlit multiBlit[];
extern Blit *playerscr_blit;
extern s32 exitPlayerScreen;
extern void PS_LockFile(void **,s32);
extern void playerscr_updatePakInfo(void);
extern void setPlayerState(s32,s32);
extern void *HAL_SetFont(s32);
extern s16 TB_GetFontLineHeight(void);
extern void playerscr_initButtons(s8);
extern void ZOID_SplitScreen(s32);
extern void particle_lifetime_set(void);
extern FiveParts *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);
extern void ClearRushControls(void);

void calcSelectMenuMaxSize(void);


void playerscr_initScreen(void)
{
    s32 p;
    for (p = 0; p < gNumPlayers; p++) {
        if (gPlayers[p].owner) PS_LockFile((void **)gPlayers[p].owner->body->selection,0);
    }
    playerscr_updatePakInfo();
    for (p = 0; p < gNumPlayers; p++) {
        gCarTeam[p] = p;
        gPlayers[p].id = p;
        playerscr_gSelectMenuSelect[p] = 3;
        setPlayerState(p,0);
        calcSelectMenuMaxSize();
        playerscr_initButtons(p);
    }
    playerscr_updateSelectInfo(-1);
    gNumWindows = gNumPlayers;
    if (gNumWindows == 3) gNumWindows = 4;
    if (gNumWindows == 1) {
        playerscr_gButtonZoom = 1.0f;
        playerscr_gButtonScale = 1.0f;
        playerscr_gButtonSpace = 40.0f;
        playerscr_buttonX = 0.0f;
        playerscr_maxY = 100.0f;
        playerscr_minY = -120.0f;
        playerscr_buttonZ = 300.0f;
        playerscr_cursorZ = 225.0f;
    } else if (gNumWindows == 2) {
        playerscr_gButtonZoom = 1.0f;
        playerscr_gButtonScale = 1.43f;
        playerscr_gButtonSpace = 40.0f;
        playerscr_buttonX = 0.0f;
        playerscr_maxY = 69.0f;
        playerscr_minY = -120.0f;
        playerscr_buttonZ = 300.0f;
        playerscr_cursorZ = 225.0f;
    } else {
        playerscr_gButtonZoom = 1.0f;
        playerscr_gButtonScale = 1.1f;
        playerscr_gButtonSpace = 40.0f;
        playerscr_buttonX = 0.0f;
        playerscr_maxY = 69.0f;
        playerscr_minY = -120.0f;
        playerscr_buttonZ = 300.0f;
        playerscr_cursorZ = 225.0f;
    }
    ZOID_SplitScreen(gNumWindows);
    particle_lifetime_set();
    for (p = 0; p < gNumPlayers; p++) {
        playerscr_pane[p] = InitPane(-20,0,-10,10,192,0,0,0);
    }
    playerscr_blit = NewMultiBlit(0,0,multiBlit,1);
    exitPlayerScreen = 0;
    ClearRushControls();
    screenInited = 1;
}

void calcSelectMenuMaxSize(void)
{
    s32 rectHeight = playerscr_gPlayerRects[gNumPlayers - 1][0].height;
    if (gNumPlayers >= 2) HAL_SetFont(11);
    else HAL_SetFont(13);
    rectHeight -= TB_GetFontLineHeight();
    if (gNumPlayers >= 2) HAL_SetFont(12);
    else HAL_SetFont(10);
    gSelectMenuMaxSize = rectHeight / TB_GetFontLineHeight();
}

extern Owner *gSelectedPlayer[];
extern struct PlayerFiles { u32 unknown0,unknown4; Owner *head,*tail; } gPlayerFiles;   /* the profiles on the paks */
extern Owner gJustPlayers[];
extern s8 gSelectMenuControls[],gDeleteMenuSelect[],gDeleteMenuConfirm[],gDeleteMenuYes[],gDeleteMenuNotify[];
extern u8 gCreatePlayerNames[][13];
extern s16 playerscr_gNameSelect[],playerscr_gNameLen[];
extern s32 playerscr_buttonsJustInited[];
extern void *memset(void *,s32,u32);

void setPlayerState(s32 player,s32 mode)
{
    Owner *node,*other;
    if (mode == 0) {
        gSelectedPlayer[player] = 0;
        node = gPlayerFiles.head;
        while (node) {
            if (node == gPlayers[player].owner) break;
            node = node->body->next;
        }
        gPlayers[player].owner = 0;
        if (!node || !node->body->selection) {
            gSelectedPlayer[player] = &gJustPlayers[player];
            if (gPlayerState[player] != 3 && gPlayerState[player] != 4)
                playerscr_gSelectPakIdx[player] = ((s32)(gPlayers[player].id * sizeof(s32))) / (s32)sizeof(s32);
            if (playerscr_gSelectMenuSelect[player] >= 3 && playerscr_gSelectMenuSelect[player] < 11)
                playerscr_gSelectMenuSelect[player] = 0;
        } else {
            gSelectedPlayer[player] = node;
            playerscr_gSelectPakIdx[player] = node->body->selection->item->player;
            if (playerscr_gSelectMenuSelect[player] >= 3 && playerscr_gSelectMenuSelect[player] < 11) {
                playerscr_gSelectMenuSelect[player] = 3;
                for (other = gPlayerFiles.head; other; other = other->body->next) {
                    if (other == node) break;
                    if (node->body->selection->item->player == other->body->selection->item->player)
                        if (playerscr_gSelectMenuSelect[player] < 10) playerscr_gSelectMenuSelect[player]++;
                }
            }
        }
        gSelectMenuControls[player] = 0;
    } else if (mode == 3) {
        memset(gCreatePlayerNames[player],0,13);
        playerscr_gNameSelect[player] = 0;
        playerscr_gNameLen[player] = 0;
        gPlayers[player].owner = 0;
    } else if (mode == 4) {
        gDeleteMenuSelect[player] = 1;
        gDeleteMenuConfirm[player] = 0;
        gDeleteMenuYes[player] = 0;
        gDeleteMenuNotify[player] = 0;
        gPlayers[player].owner = 0;
    }
    gPlayerState[player] = mode;
    playerscr_buttonsJustInited[player] = 1;
    playerscr_updateSelectInfo(player);
}

typedef struct PlayerPresence { s8 present; u8 unknown1[15]; } PlayerPresence;
extern PlayerPresence gControllerData[];
extern s32 playerscr_buttonsJustInited[];
extern s8 PS_PakInserted(s32),PS_PakDamaged(s32),PS_PakRumble(s32);
extern u32 PS_GetFreeBlocks(s32);
extern s32 PS_GetBlocks(s32),PS_GetFreeFiles(s32);

void playerscr_updatePakInfo(void)
{
    s32 i,j;
    Owner *node;
    u32 available;
    for(i=0;i<4;i++) {
        gPakFull[i]=0;
        if(!gControllerData[i].present)playerscr_gNumPakPlayers[i]=-4;
        else if(!PS_PakInserted(i))playerscr_gNumPakPlayers[i]=-1;
        else if(PS_PakDamaged(i))playerscr_gNumPakPlayers[i]=-2;
        else if(PS_PakRumble(i))playerscr_gNumPakPlayers[i]=-3;
        else {
            if(playerscr_gNumPakPlayers[i]<0) {
                for(j=0;j<gNumPlayers;j++) {
                    if(playerscr_gSelectPakIdx[j]==i)playerscr_buttonsJustInited[j]=1;
                }
            }
            playerscr_gNumPakPlayers[i]=0;
            for(node=gPlayerFiles.head;node;node=node->body->next) {
                if(node->body->selection->item->player==i)playerscr_gNumPakPlayers[i]++;
            }
            if(playerscr_gNumPakPlayers[i]==0)playerscr_gNumPakPlayers[i]=0;
            available=PS_GetFreeBlocks(i);
            gPakFull[i]=available<(u32)PS_GetBlocks(0x810) || !PS_GetFreeFiles(i);
        }
    }
}
typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    u8 alpha,unknown3D[3];
} Slot64;
extern Slot64 playerscr_buttonInfo[][22];
extern s32 playerscr_buttonsInited[];
extern f32 HFOV;
extern void ZOID_RemoveObject(s16),sound_handles_array_clear(FiveParts *);
extern void cleanupControlScreen(void),RemoveBlit(Blit *),removeBG(void);
extern void ZOID_ResetWindows(void),ZOID_Reset(void);

void playerscr_uninitScreen(void);

extern volatile u8 gNumModels;
extern char *playerscr_buttonNames[];
extern char playerscr_title_texture_name[],playerscr_cursor_texture_name[],playerscr_a_button_texture_name[],playerscr_b_button_texture_name[];
extern f32 unitmatrix[3][3];
extern void *memcpy(void *,const void *,u32);
extern void *MBOX_FindObject_WORK(char *,s32,s8,s32);
extern s32 MBOX_NewObject(void *,f32 (*)[3],s32,u32);
extern void ZOID_SetColor(s16,u8 *),MBOX_HideObject(s32,s32,s32),MBOX_ShowObject(s32,s32,s32);
/* PC: ZOID_SetColor reads a colour as one word, so the colours passed to it are word-aligned, as IDO's
 * frame has them (docs/port/SANITIZE.md 3.2) */
#define PC_WORD_ALIGNED __attribute__((aligned(4)))
extern void *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);
extern void ZOID_SetTexture(s16,void *,s32);
extern void WPitchUV(f32,f32 (*)[3]),WRollUV(f32,f32 (*)[3]);
extern void ScaleUV(f32 (*)[3],f32 (*)[3],f32);
extern void ZeroCameras(void);
/* Complete native-led 97AD8 model-slot initialization: strict 327-word match
 * in this genuine context. The consumed scale local preserves the native
 * multiply-by-one operations that literal folding removed, including their
 * floating-point evaluation. Its native-derived declaration position leaves
 * color at sp+76 and the genuine s16 output at sp+70 in the unchanged 80-byte
 * frame. This is not an unused frame filler or a recovered original name.
 * All 40 literal bytes and the complete four-byte white color initializer
 * are authenticated against image A. Slot64 offsets/capacities are unchanged.
 * No fabricated formal or caller: actual input is one signed player byte
 * passed by the sole genuine 97FFC caller at incoming stack+3. Empty claims:
 * the nonmatching caller and cross-group ownership still block installation.
 */
/* Existing matched texture lookup declares this table-count byte volatile. */
void playerscr_initButtons(s8 player)
{
    u8 color[4] PC_WORD_ALIGNED = {255,255,255,255};
    f32 scale=1.0f;
    s16 count;
    s32 j;
    Slot64 *slot;
    void *resource;
    for(j=0;j<22;j++) {
        slot=&playerscr_buttonInfo[player][j];
        if(slot->id==-1) {
            memcpy(slot->matrix,unitmatrix,36);
            resource=MBOX_FindObject_WORK(playerscr_buttonNames[slot->unknown0],0,gNumModels-1,1);
            slot->id=MBOX_NewObject(resource,slot->matrix,-1,
                slot->unknown0==0 || slot->unknown0==1 || slot->unknown0==2 ||
                slot->unknown0==7 || slot->unknown0==8 || slot->unknown0==9 ? 0x42000:0);
            ZOID_SetColor(slot->id,color);
            MBOX_HideObject(slot->id,1,15);
            MBOX_ShowObject(slot->id,0,1<<player);
        }
    }
    resource=MBOX_FindTexture_WORK(playerscr_title_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][16].id,resource,-1);
    memcpy(playerscr_buttonInfo[player][16].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][16].matrix);
    playerscr_buttonInfo[player][16].position[0]=0.0f;
    playerscr_buttonInfo[player][16].position[1]=gNumPlayers>=2?105.0f:115.0f;
    playerscr_buttonInfo[player][16].position[0]*=scale;
    playerscr_buttonInfo[player][16].position[1]*=scale;
    playerscr_buttonInfo[player][16].position[2]=200.0f*scale;
    resource=MBOX_FindTexture_WORK(playerscr_cursor_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][17].id,resource,-1);
    resource=MBOX_FindTexture_WORK(playerscr_a_button_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][18].id,resource,-1);
    memcpy(playerscr_buttonInfo[player][18].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][18].matrix);
    playerscr_buttonInfo[player][18].position[0]=75.1f;
    playerscr_buttonInfo[player][18].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][18].position[2]=100.0f;
    ScaleUV(playerscr_buttonInfo[player][18].matrix,playerscr_buttonInfo[player][18].matrix,.5f);
    memcpy(playerscr_buttonInfo[player][19].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][19].matrix);
    WRollUV(3.14159265359f,playerscr_buttonInfo[player][19].matrix);
    playerscr_buttonInfo[player][19].position[0]=87.1f;
    playerscr_buttonInfo[player][19].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][19].position[2]=100.0f;
    resource=MBOX_FindTexture_WORK(playerscr_b_button_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][20].id,resource,-1);
    memcpy(playerscr_buttonInfo[player][20].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][20].matrix);
    playerscr_buttonInfo[player][20].position[0]=-75.1f;
    playerscr_buttonInfo[player][20].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][20].position[0]*=scale;
    playerscr_buttonInfo[player][20].position[1]*=scale;
    playerscr_buttonInfo[player][20].position[2]=100.0f*scale;
    ScaleUV(playerscr_buttonInfo[player][20].matrix,playerscr_buttonInfo[player][20].matrix,.5f);
    memcpy(playerscr_buttonInfo[player][21].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][21].matrix);
    playerscr_buttonInfo[player][21].position[0]=-87.1f;
    playerscr_buttonInfo[player][21].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][21].position[0]*=scale;
    playerscr_buttonInfo[player][21].position[1]*=scale;
    playerscr_buttonInfo[player][21].position[2]=100.0f*scale;
    playerscr_buttonsInited[player]=1;
    playerscr_buttonsJustInited[player]=1;
    ZeroCameras();
}

extern f32 fabsf(f32);
#pragma intrinsic (fabsf)
extern volatile f32 gFrameSeconds;
extern volatile u8 gNumModels;
extern f32 unitmatrix[3][3];
extern f32 playerscr_gButtonScale,playerscr_gButtonSpace,playerscr_buttonX,playerscr_maxY,playerscr_minY,playerscr_buttonZ,playerscr_cursorZ;
extern s32 playerscr_buttonsJustInited[];
extern f32 playerscr_cursorSpeed[],playerscr_scrollSpeed[];
extern s32 playerscr_buttonsInited[];
extern s8 playerscr_gSelectMenuCount[],playerscr_gSelectMenuVisible[][14];

void playerscr_updateButtons(s8 p)
{
    s32 buttonIdx;
    s32 i;
    s32 unusedA, unusedB; /* disclosed frame shaping, see header */
    f32 prevPos[3];
    s32 whichItem;
    f32 buttonY;
    f32 selectedPos;
    s8 clearScrollSpeed = 1;
    void *tex;
    s16 texIdx;
    u8 buttonColor[4] PC_WORD_ALIGNED = {255,255,255,255};
    if (!playerscr_buttonsInited[p]) return;
    if (gPlayerState[p] == 1) {
        for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) {
            if (buttonIdx == 16 || buttonIdx == 20 || buttonIdx == 21)
                MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].id, 0, 1 << p);
            else MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].id, 1, 15);
        }
        return;
    }
    if (gPlayerState[p] == 3 || (gPlayerState[p] == 4 && (gDeleteMenuNotify[p] == 1 || gDeleteMenuConfirm[p] == 1))) {
        for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) {
            if (buttonIdx == 16 || (buttonIdx == 20 || buttonIdx == 21) || buttonIdx == 18 || buttonIdx == 19)
                MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].id, 0, 1 << p);
            else MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].id, 1, 15);
        }
        return;
    }
    if (gPlayerState[p] == 2 || (gPlayerState[p] == 5 || gPlayerState[p] == 6) || gPlayerState[p] == 7) {
        for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].id, 1, 15);
        return;
    }
    for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].id, 0, 1 << p);
    whichItem = 0;
    if (gPlayerState[p] == 0) {
        for (i = 0; i < playerscr_gSelectMenuSelect[p]; i++) if (playerscr_gSelectMenuVisible[p][i]) whichItem++;
    } else if (gPlayerState[p] == 4) {
        for (i = 0; i < gDeleteMenuSelect[p]; i++) if (playerscr_gSelectMenuVisible[p][i]) whichItem++;
    }
    if (playerscr_gButtonScale * ((playerscr_gSelectMenuCount[p] - 1) * playerscr_gButtonSpace) > playerscr_maxY - playerscr_minY)
        selectedPos = playerscr_maxY - whichItem * (playerscr_maxY - playerscr_minY) / (playerscr_gSelectMenuCount[p] - 1);
    else selectedPos = playerscr_maxY - playerscr_gButtonScale * (whichItem * playerscr_gButtonSpace);
    memcpy(prevPos, playerscr_buttonInfo[p][17].position, 12);
    playerscr_buttonInfo[p][17].angle += 6.2831855f * gFrameSeconds;
    if (playerscr_buttonInfo[p][17].angle > 6.2831855f) playerscr_buttonInfo[p][17].angle -= 6.2831855f;
    memcpy(playerscr_buttonInfo[p][17].matrix, unitmatrix, 36);
    playerscr_buttonInfo[p][17].position[0] = playerscr_buttonX * playerscr_cursorZ / playerscr_buttonZ + -165.0f * playerscr_gButtonScale;
    playerscr_buttonInfo[p][17].position[1] = selectedPos * playerscr_cursorZ / playerscr_buttonZ;
    playerscr_buttonInfo[p][17].position[2] = playerscr_cursorZ;
    if (!playerscr_buttonsJustInited[p]) {
        if (fabsf(playerscr_buttonInfo[p][17].position[1] - prevPos[1]) * 60 * playerscr_gButtonScale / 10 > playerscr_cursorSpeed[p]) {
            playerscr_cursorSpeed[p] = 60 * fabsf(playerscr_buttonInfo[p][17].position[1] - prevPos[1]) * playerscr_gButtonScale / 10;
            playerscr_cursorSpeed[p] = playerscr_cursorSpeed[p] < 540 * playerscr_gButtonScale ? playerscr_cursorSpeed[p] : 540 * playerscr_gButtonScale;
            playerscr_cursorSpeed[p] = playerscr_cursorSpeed[p] > 180 * playerscr_gButtonScale ? playerscr_cursorSpeed[p] : 180 * playerscr_gButtonScale;
        }
        if (playerscr_buttonInfo[p][17].position[1] > prevPos[1] + gFrameSeconds * playerscr_cursorSpeed[p])
            playerscr_buttonInfo[p][17].position[1] = prevPos[1] + gFrameSeconds * playerscr_cursorSpeed[p];
        else if (playerscr_buttonInfo[p][17].position[1] < prevPos[1] - gFrameSeconds * playerscr_cursorSpeed[p])
            playerscr_buttonInfo[p][17].position[1] = prevPos[1] - gFrameSeconds * playerscr_cursorSpeed[p];
        else playerscr_cursorSpeed[p] = 180 * playerscr_gButtonScale;
    }
    WRollUV(playerscr_buttonInfo[p][17].angle, playerscr_buttonInfo[p][17].matrix);
    WPitchUV(1.5707964f, playerscr_buttonInfo[p][17].matrix);
    ScaleUV(playerscr_buttonInfo[p][17].matrix, playerscr_buttonInfo[p][17].matrix, playerscr_gButtonScale * 0.4f);
    buttonY = selectedPos + playerscr_gButtonScale * (whichItem * playerscr_gButtonSpace);
    for (buttonIdx = 0; buttonIdx < 14; buttonIdx++) {
        if (((gPlayerState[p] == 0 && buttonIdx == playerscr_gSelectMenuSelect[p]) ||
             (gPlayerState[p] == 4 && buttonIdx == gDeleteMenuSelect[p])) && !gSelectMenuControls[p]) {
            if (playerscr_buttonInfo[p][buttonIdx].angle < 3.1415927f) playerscr_buttonInfo[p][buttonIdx].angle += 12.566371f * gFrameSeconds;
            if (playerscr_buttonInfo[p][buttonIdx].angle > 3.1415927f) playerscr_buttonInfo[p][buttonIdx].angle = 3.1415927f;
            if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][buttonIdx].angle = 3.1415927f;
        } else {
            if (playerscr_buttonInfo[p][buttonIdx].angle > 0) playerscr_buttonInfo[p][buttonIdx].angle -= 12.566371f * gFrameSeconds;
            if (playerscr_buttonInfo[p][buttonIdx].angle < 0) playerscr_buttonInfo[p][buttonIdx].angle = 0;
            if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][buttonIdx].angle = 0;
        }
        memcpy(prevPos, playerscr_buttonInfo[p][buttonIdx].position, 12);
        if (playerscr_buttonInfo[p][buttonIdx].unknown0 == 9) playerscr_buttonInfo[p][buttonIdx].position[0] = playerscr_buttonX + 0 * playerscr_gButtonScale;
        else if (playerscr_buttonInfo[p][buttonIdx].unknown0 == 7) playerscr_buttonInfo[p][buttonIdx].position[0] = playerscr_buttonX + -80 * playerscr_gButtonScale;
        else playerscr_buttonInfo[p][buttonIdx].position[0] = playerscr_buttonX + -120 * playerscr_gButtonScale;
        playerscr_buttonInfo[p][buttonIdx].position[1] = buttonY;
        playerscr_buttonInfo[p][buttonIdx].position[2] = playerscr_buttonZ;
        if (!playerscr_buttonsJustInited[p]) {
            if (fabsf(playerscr_buttonInfo[p][buttonIdx].position[1] - prevPos[1]) * 60 / 10 > playerscr_scrollSpeed[p]) {
                playerscr_scrollSpeed[p] = fabsf(playerscr_buttonInfo[p][buttonIdx].position[1] - prevPos[1]) * 60 / 10;
                playerscr_scrollSpeed[p] = playerscr_scrollSpeed[p] < 540 * playerscr_gButtonScale ? playerscr_scrollSpeed[p] : 540 * playerscr_gButtonScale;
                playerscr_scrollSpeed[p] = playerscr_scrollSpeed[p] > 180 * playerscr_gButtonScale ? playerscr_scrollSpeed[p] : 180 * playerscr_gButtonScale;
            }
            if (playerscr_buttonInfo[p][buttonIdx].position[1] > prevPos[1] + gFrameSeconds * playerscr_scrollSpeed[p]) {
                playerscr_buttonInfo[p][buttonIdx].position[1] = prevPos[1] + gFrameSeconds * playerscr_scrollSpeed[p];
                clearScrollSpeed = 0;
            } else if (playerscr_buttonInfo[p][buttonIdx].position[1] < prevPos[1] - gFrameSeconds * playerscr_scrollSpeed[p]) {
                playerscr_buttonInfo[p][buttonIdx].position[1] = prevPos[1] - gFrameSeconds * playerscr_scrollSpeed[p];
                clearScrollSpeed = 0;
            }
        }
        ScaleUV(unitmatrix, playerscr_buttonInfo[p][buttonIdx].matrix, playerscr_gButtonScale);
        WPitchUV(playerscr_buttonInfo[p][buttonIdx].angle - 1.5707964f, playerscr_buttonInfo[p][buttonIdx].matrix);
        if (playerscr_buttonInfo[p][buttonIdx].angle > 1.5707964f)
            tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
        else tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
        ZOID_SetTexture(playerscr_buttonInfo[p][buttonIdx].id, tex, -1);
        if (playerscr_buttonInfo[p][buttonIdx].position[1] > playerscr_maxY + playerscr_gButtonSpace * playerscr_gButtonScale ||
            playerscr_buttonInfo[p][buttonIdx].position[1] < playerscr_minY - playerscr_gButtonSpace * playerscr_gButtonScale) playerscr_buttonInfo[p][buttonIdx].alpha = 0;
        else if (playerscr_buttonInfo[p][buttonIdx].position[1] > playerscr_maxY)
            playerscr_buttonInfo[p][buttonIdx].alpha = 255.f - 255.0f * (playerscr_buttonInfo[p][buttonIdx].position[1] - playerscr_maxY) / (playerscr_gButtonSpace * playerscr_gButtonScale);
        else if (playerscr_buttonInfo[p][buttonIdx].position[1] < playerscr_minY)
            playerscr_buttonInfo[p][buttonIdx].alpha = 255.f - 255.0f * (playerscr_minY - playerscr_buttonInfo[p][buttonIdx].position[1]) / (playerscr_gButtonSpace * playerscr_gButtonScale);
        else playerscr_buttonInfo[p][buttonIdx].alpha = 255;
        if (!playerscr_gSelectMenuVisible[p][buttonIdx]) playerscr_buttonInfo[p][buttonIdx].alpha = 0;
        buttonColor[3] = playerscr_buttonInfo[p][buttonIdx].alpha;
        ZOID_SetColor(playerscr_buttonInfo[p][buttonIdx].id, buttonColor);
        if (!playerscr_gSelectMenuVisible[p][buttonIdx] || !playerscr_buttonInfo[p][buttonIdx].alpha) MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].id, 1, 15);
        else MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].id, 0, 1 << p);
        if (buttonIdx == 1) {
            playerscr_buttonInfo[p][15].position[0] = playerscr_buttonX + 160 * playerscr_gButtonScale;
            playerscr_buttonInfo[p][15].position[1] = playerscr_buttonInfo[p][buttonIdx].position[1];
            playerscr_buttonInfo[p][15].position[2] = playerscr_buttonInfo[p][buttonIdx].position[2];
            playerscr_buttonInfo[p][15].angle = playerscr_buttonInfo[p][buttonIdx].angle;
            ScaleUV(unitmatrix, playerscr_buttonInfo[p][15].matrix, playerscr_gButtonScale);
            WPitchUV(playerscr_buttonInfo[p][15].angle - 1.5707964f, playerscr_buttonInfo[p][15].matrix);
            ZOID_SetTexture(playerscr_buttonInfo[p][15].id, tex, -1);
            playerscr_buttonInfo[p][15].alpha = playerscr_buttonInfo[p][buttonIdx].alpha;
            ZOID_SetColor(playerscr_buttonInfo[p][15].id, buttonColor);
            if (!playerscr_gSelectMenuVisible[p][buttonIdx] || !playerscr_buttonInfo[p][buttonIdx].alpha) MBOX_HideObject(playerscr_buttonInfo[p][15].id, 1, 15);
            else MBOX_ShowObject(playerscr_buttonInfo[p][15].id, 0, 1 << p);
        } else {
            if (gPlayerState[p] == 0 && (playerscr_gSelectMenuSelect[p] == 0 || (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11))) {
                if (buttonIdx == playerscr_gSelectMenuSelect[p]) {
                    playerscr_buttonInfo[p][14].position[0] = 120 * playerscr_gButtonScale + playerscr_buttonX;
                    playerscr_buttonInfo[p][14].position[1] = playerscr_buttonInfo[p][buttonIdx].position[1];
                    playerscr_buttonInfo[p][14].position[2] = playerscr_buttonInfo[p][buttonIdx].position[2];
                    if (gSelectMenuControls[p]) {
                        if (playerscr_buttonInfo[p][14].angle < 3.1415927f) playerscr_buttonInfo[p][14].angle += 12.566371f * gFrameSeconds;
                        if (playerscr_buttonInfo[p][14].angle > 3.1415927f) playerscr_buttonInfo[p][14].angle = 3.1415927f;
                        if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][14].angle = 3.1415927f;
                    } else {
                        if (playerscr_buttonInfo[p][14].angle > 0) playerscr_buttonInfo[p][14].angle -= 12.566371f * gFrameSeconds;
                        if (playerscr_buttonInfo[p][14].angle < 0) playerscr_buttonInfo[p][14].angle = 0;
                        if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][14].angle = 0;
                    }
                    ScaleUV(unitmatrix, playerscr_buttonInfo[p][14].matrix, playerscr_gButtonScale);
                    WPitchUV(playerscr_buttonInfo[p][14].angle - 1.5707964f, playerscr_buttonInfo[p][14].matrix);
                    if (playerscr_buttonInfo[p][14].angle > 1.5707964f)
                        tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
                    else tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
                    ZOID_SetTexture(playerscr_buttonInfo[p][14].id, tex, -1);
                    playerscr_buttonInfo[p][14].alpha = playerscr_buttonInfo[p][buttonIdx].alpha;
                    ZOID_SetColor(playerscr_buttonInfo[p][14].id, buttonColor);
                    if (!playerscr_gSelectMenuVisible[p][buttonIdx] || !playerscr_buttonInfo[p][buttonIdx].alpha) {
                        playerscr_buttonInfo[p][14].angle = 0;
                        MBOX_HideObject(playerscr_buttonInfo[p][14].id, 1, 15);
                    } else MBOX_ShowObject(playerscr_buttonInfo[p][14].id, 0, 1 << p);
                }
            } else {
                playerscr_buttonInfo[p][14].angle = 0;
                MBOX_HideObject(playerscr_buttonInfo[p][14].id, 1, 15);
            }
        }
        if (playerscr_gSelectMenuVisible[p][buttonIdx] == 1) buttonY -= playerscr_gButtonSpace * playerscr_gButtonScale;
    }
    if (clearScrollSpeed == 1) playerscr_scrollSpeed[p] = 0;
    playerscr_buttonsJustInited[p] = 0;
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
extern void TB_Strcpy(u8 *,u8 *);
extern void DeletePlayer(Owner *);
extern s8 gDeleteMenuSelect[],gDeleteMenuConfirm[],gDeleteMenuYes[],gDeleteMenuNotify[];
extern u8 gDeleteMenuName[][13];
extern s32 playerscr_buttonsJustInited[];
extern s8 playerscr_gSelectMenuValid[][14];
/* playerClaimed: installed body of group runtime_a_profile_menu (IPA context: owner in a0, skip in a2) */
s32 playerClaimed(s32 skip,Owner *owner);
extern void PlayLRSound(u32),PlayUDSound(u32);
extern void censor(char *);
extern Owner *CreatePlayer(s8,char *);
extern char *gPlayerNameChars;

/* Drop one navigation step to the next/previous profile on the same pak (inlined statics in the original; written out). */
void controlDeletePlayer(s32 p)
{
    s8 origSelect;
    s32 pak;
    s32 pIdx;
    Owner *player;
    pak = playerscr_gSelectPakIdx[p];
    origSelect = gDeleteMenuSelect[p];
    if (playerscr_gNumPakPlayers[pak] <= 0) {
        setPlayerState(p, 0);
        return;
    }
    while (!playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1) {
        gDeleteMenuSelect[p]++;
        if (gDeleteMenuSelect[p] > 13) gDeleteMenuSelect[p] = 0;
        if (gDeleteMenuSelect[p] == origSelect) {
            setPlayerState(p, 0);
            return;
        }
        if (gDeleteMenuSelect[p] >= 3 && gDeleteMenuSelect[p] < 11 && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
            if (gSelectedPlayer[p]) {
                do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->next; }
                while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
            }
            if (!gSelectedPlayer[p]) {
                gSelectedPlayer[p] = gPlayerFiles.head;
                while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p])
                    gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
            }
        } else gSelectedPlayer[p] = 0;
        playerscr_buttonsJustInited[p] = 1;
    }
    if (gPlayers[p].flags & 1) {
        PlayStartABSound(gPlayers[p].flags);
        if (gDeleteMenuNotify[p] == 1) {
            gDeleteMenuNotify[p] = 0;
            setPlayerState(p, 0);
        } else if (gDeleteMenuConfirm[p] == 1) {
            gDeleteMenuConfirm[p] = 0;
            gDeleteMenuYes[p] = 0;
        } else setPlayerState(p, 0);
    } else if (gPlayers[p].flags & 2) {
        if (gDeleteMenuNotify[p] == 1) {
            PlayStartABSound(gPlayers[p].flags);
            gDeleteMenuNotify[p] = 0;
            setPlayerState(p, 0);
        } else if (gDeleteMenuConfirm[p] == 1) {
            if (gDeleteMenuYes[p] == 1) {
                PlayStartABSound(gPlayers[p].flags);
                DeletePlayer(gSelectedPlayer[p]);
                for (player = gPlayerFiles.head; player; player = player->body->next)
                    if (gSelectedPlayer[p] == player) break;
                if (!player) {
                    for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
                        if (pIdx != p && playerscr_gSelectPakIdx[pIdx] == playerscr_gSelectPakIdx[p] &&
                            playerscr_gSelectMenuSelect[pIdx] >= 3 && playerscr_gSelectMenuSelect[pIdx] < 11) {
                            playerscr_gSelectMenuSelect[pIdx] = 1;
                            gSelectedPlayer[pIdx] = 0;
                        }
                    }
                    gSelectedPlayer[p] = 0;
                    playerscr_updatePakInfo();
                    playerscr_updateSelectInfo(-1);
                    gDeleteMenuSelect[p] = 0;
                    gDeleteMenuNotify[p] = 1;
                }
            } else PlayStartABSound(4);
            gDeleteMenuConfirm[p] = 0;
            gDeleteMenuYes[p] = 0;
        } else if (gDeleteMenuSelect[p] >= 3 && gDeleteMenuSelect[p] < 11) {
            PlayStartABSound(gPlayers[p].flags);
            TB_Strcpy(gDeleteMenuName[p], gSelectedPlayer[p]->body->name);
            gDeleteMenuConfirm[p] = 1;
            gDeleteMenuYes[p] = 0;
        }
    } else if (gPlayers[p].flags & 4) {
        PlayStartABSound(gPlayers[p].flags);
        if (gDeleteMenuNotify[p] == 1) {
            gDeleteMenuNotify[p] = 0;
            setPlayerState(p, 0);
        } else if (gDeleteMenuConfirm[p] == 1) {
            gDeleteMenuConfirm[p] = 0;
            gDeleteMenuYes[p] = 0;
        } else setPlayerState(p, 0);
    }
    if (gPlayers[p].autoFlags & 0x2000) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) {
            PlayLRSound(gPlayers[p].flags);
            gDeleteMenuYes[p] = !gDeleteMenuYes[p];
        } else if (gDeleteMenuSelect[p] == 1) {
            playerscr_gSelectPakIdx[p]++;
            if (playerscr_gSelectPakIdx[p] > 3) playerscr_gSelectPakIdx[p] = 0;
            playerscr_updateSelectInfo(p);
            PlayLRSound(gPlayers[p].flags);
        }
    } else if (gPlayers[p].autoFlags & 0x1000) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) {
            PlayLRSound(gPlayers[p].flags);
            gDeleteMenuYes[p] = !gDeleteMenuYes[p];
        } else if (gDeleteMenuSelect[p] == 1) {
            playerscr_gSelectPakIdx[p]--;
            if (playerscr_gSelectPakIdx[p] < 0) playerscr_gSelectPakIdx[p] = 3;
            playerscr_updateSelectInfo(p);
            PlayLRSound(gPlayers[p].flags);
        }
    } else if (gPlayers[p].autoFlags & 0x400) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) return;
        PlayUDSound(gPlayers[p].flags);
        do {
            if (gDeleteMenuSelect[p] == 3 && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            gDeleteMenuSelect[p]--;
            if (gDeleteMenuSelect[p] < 0) gDeleteMenuSelect[p] = 13;
            if (gDeleteMenuSelect[p] >= 3 && gDeleteMenuSelect[p] < 11 && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.tail;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1);
    } else if (gPlayers[p].autoFlags & 0x800) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) return;
        PlayUDSound(gPlayers[p].flags);
        do {
            if (gDeleteMenuSelect[p] == 10 && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->next; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            gDeleteMenuSelect[p]++;
            if (gDeleteMenuSelect[p] > 13) gDeleteMenuSelect[p] = 0;
            if (gDeleteMenuSelect[p] >= 3 && gDeleteMenuSelect[p] < 11 && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->next; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.head;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1);
    }
}

/* playerBeingDeleted/playerBeingCreated (playerBeingDeleted/Created) take `skip` in a1 under IDO's IPA: their owners
 * (src/ovl_a/playerBeingDeleted.c, playerBeingCreated.c) read it as a second parameter after a dummy, and these calls
 * pass it alone. On the PC the dummy is explicit (docs/port/PROTOTYPES.md). */
#define PC_IPA_A0 s32 pc_a0,
/* playerBeingDeleted / playerBeingCreated: installed bodies, IPA context */
s32 playerBeingDeleted(PC_IPA_A0 s32 skip);
s32 playerBeingCreated(PC_IPA_A0 s32 skip);
#define playerBeingDeleted(skip) playerBeingDeleted(0, skip)
#define playerBeingCreated(skip) playerBeingCreated(0, skip)
void claimPlayer(s32,Owner *);
void playerscr_controlSelect(s32 p)
{
    while (!playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]] ||
           playerClaimed(p, gSelectedPlayer[p]) == 1 ||
           (playerscr_gSelectMenuSelect[p] == 11 && playerBeingCreated(p) == 1) ||
           (playerscr_gSelectMenuSelect[p] == 12 && playerBeingDeleted(p) == 1)) {
        gSelectMenuControls[p] = 0;
        playerscr_gSelectMenuSelect[p]--;
        if (playerscr_gSelectMenuSelect[p] < 0) playerscr_gSelectMenuSelect[p] = 13;
        if (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11 && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
            if (gSelectedPlayer[p]) {
                do {
                    gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
            }
            if (!gSelectedPlayer[p]) {
                gSelectedPlayer[p] = gPlayerFiles.tail;
                while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p])
                    gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
            }
        } else gSelectedPlayer[p] = 0;
    }
    if (gPlayers[p].flags & 1) {
        PlayStartABSound(gPlayers[p].flags);
        if (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11)
            claimPlayer(p, gSelectedPlayer[p]);
        else
            claimPlayer(p, &gJustPlayers[p]);
        setPlayerState(p, 1);
    } else if (gPlayers[p].flags & 2) {
        PlayStartABSound(gPlayers[p].flags);
        if (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11) {
            claimPlayer(p, gSelectedPlayer[p]);
            if (gSelectMenuControls[p] == 1) {
                if (ReadPlayerData(gPlayers[p].owner)) {
                    controlScrActive[p] = 1;
                    PS_LockFile((void **)gPlayers[p].owner->body->selection, 1);
                    setPlayerState(p, 2);
                }
            } else setPlayerState(p, 1);
        } else switch (playerscr_gSelectMenuSelect[p]) {
        case 0:
            claimPlayer(p, &gJustPlayers[p]);
            if (gSelectMenuControls[p] == 1) {
                controlScrActive[p] = 1;
                PS_LockFile((void **)gPlayers[p].owner->body->selection, 1);
                setPlayerState(p, 2);
            } else setPlayerState(p, 1);
            break;
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
            break;
        case 11: setPlayerState(p, 3); break;
        case 12: gDeleteMenuSelect[p] = 0; setPlayerState(p, 4); break;
        case 13: setPlayerState(p, 6); break;
        }
    } else if (gPlayers[p].flags & 4) {
        PlayStartABSound(gPlayers[p].flags);
        exitPlayerScreen = 2;
    }
    do {} while (0); /* SHAPING: compiled-out block boundary, see header */
    if (gPlayers[p].autoFlags & 0x2000) {
        if (playerscr_gSelectMenuSelect[p] == 0 || (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11)) {
            gSelectMenuControls[p] = !gSelectMenuControls[p];
            PlayLRSound(gPlayers[p].flags);
        } else if (playerscr_gSelectMenuSelect[p] == 1) {
            playerscr_gSelectPakIdx[p]++;
            if (playerscr_gSelectPakIdx[p] >= 4) playerscr_gSelectPakIdx[p] = 0;
            playerscr_updateSelectInfo(p);
            playerscr_buttonsJustInited[p] = 1;
            PlayLRSound(gPlayers[p].flags);
        }
    } else if (gPlayers[p].autoFlags & 0x1000) {
        if (playerscr_gSelectMenuSelect[p] == 0 || (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11)) {
            gSelectMenuControls[p] = !gSelectMenuControls[p];
            PlayLRSound(gPlayers[p].flags);
        } else if (playerscr_gSelectMenuSelect[p] == 1) {
            playerscr_gSelectPakIdx[p]--;
            if (playerscr_gSelectPakIdx[p] < 0) playerscr_gSelectPakIdx[p] = 3;
            playerscr_updateSelectInfo(p);
            playerscr_buttonsJustInited[p] = 1;
            PlayLRSound(gPlayers[p].flags);
        }
    } else if (gPlayers[p].autoFlags & 0x400) {
        PlayUDSound(gPlayers[p].flags);
        gSelectMenuControls[p] = 0;
        do {
            if (playerscr_gSelectMenuSelect[p] == 3 && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            playerscr_gSelectMenuSelect[p]--;
            if (playerscr_gSelectMenuSelect[p] < 0) playerscr_gSelectMenuSelect[p] = 13;
            if (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11 && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.tail;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1 ||
                 (playerscr_gSelectMenuSelect[p] == 11 && playerBeingCreated(p) == 1) ||
                 (playerscr_gSelectMenuSelect[p] == 12 && playerBeingDeleted(p) == 1));
    } else if (gPlayers[p].autoFlags & 0x800) {
        PlayUDSound(gPlayers[p].flags);
        gSelectMenuControls[p] = 0;
        do {
            if (playerscr_gSelectMenuSelect[p] == 10 && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            playerscr_gSelectMenuSelect[p]++;
            if (playerscr_gSelectMenuSelect[p] >= 14) playerscr_gSelectMenuSelect[p] = 0;
            if (playerscr_gSelectMenuSelect[p] >= 3 && playerscr_gSelectMenuSelect[p] < 11 && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.head;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->selection->item->player != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1 ||
                 (playerscr_gSelectMenuSelect[p] == 11 && playerBeingCreated(p) == 1) ||
                 (playerscr_gSelectMenuSelect[p] == 12 && playerBeingDeleted(p) == 1));
    }
}
void claimPlayer(s32 playerIdx, Owner *player)
{
    gPlayers[playerIdx].owner = player;
    playerscr_updateSelectInfo(-1);
}

void controlCreatePlayer(s32 p)
{
    Owner *player;
    s32 pIdx;
    if (playerscr_gNumPakPlayers[playerscr_gSelectPakIdx[p]] < 0 || gPakFull[playerscr_gSelectPakIdx[p]])
        setPlayerState(p, 0);
    if (gPlayers[p].autoFlags & 0x400) {
        PlayUDSound(gPlayers[p].autoFlags);
        if (playerscr_gNameSelect[p] == -1) playerscr_gNameSelect[p] = 52;
        else if (playerscr_gNameSelect[p] == -2) playerscr_gNameSelect[p] = 49;
        else if (playerscr_gNameSelect[p] == -3) playerscr_gNameSelect[p] = 46;
        else if (playerscr_gNameSelect[p] < 3) playerscr_gNameSelect[p] = -3;
        else if (playerscr_gNameSelect[p] < 6) playerscr_gNameSelect[p] = -2;
        else if (playerscr_gNameSelect[p] < 9) playerscr_gNameSelect[p] = -1;
        else playerscr_gNameSelect[p] -= 9;
    } else if (gPlayers[p].autoFlags & 0x800) {
        PlayUDSound(gPlayers[p].autoFlags);
        if (playerscr_gNameSelect[p] == -1) playerscr_gNameSelect[p] = 7;
        else if (playerscr_gNameSelect[p] == -2) playerscr_gNameSelect[p] = 4;
        else if (playerscr_gNameSelect[p] == -3) playerscr_gNameSelect[p] = 1;
        else if (playerscr_gNameSelect[p] < 45) playerscr_gNameSelect[p] += 9;
        else if (playerscr_gNameSelect[p] < 48) playerscr_gNameSelect[p] = -3;
        else if (playerscr_gNameSelect[p] < 51) playerscr_gNameSelect[p] = -2;
        else playerscr_gNameSelect[p] = -1;
    } else if (gPlayers[p].autoFlags & 0x1000) {
        PlayLRSound(gPlayers[p].autoFlags);
        if (playerscr_gNameSelect[p] == -1) playerscr_gNameSelect[p] = -2;
        else if (playerscr_gNameSelect[p] == -2) playerscr_gNameSelect[p] = -3;
        else if (playerscr_gNameSelect[p] == -3) playerscr_gNameSelect[p] = -1;
        else if (playerscr_gNameSelect[p] % 9 == 0) playerscr_gNameSelect[p] += 8;
        else playerscr_gNameSelect[p]--;
    } else if (gPlayers[p].autoFlags & 0x2000) {
        PlayLRSound(gPlayers[p].autoFlags);
        if (playerscr_gNameSelect[p] == -1) playerscr_gNameSelect[p] = -3;
        else if (playerscr_gNameSelect[p] == -2) playerscr_gNameSelect[p] = -1;
        else if (playerscr_gNameSelect[p] == -3) playerscr_gNameSelect[p] = -2;
        else if (playerscr_gNameSelect[p] % 9 == 8) playerscr_gNameSelect[p] -= 8;
        else playerscr_gNameSelect[p]++;
    }
    if (gPlayers[p].flags & 3) {
        PlayStartABSound(gPlayers[p].flags);
        if (gPlayers[p].flags & 1) playerscr_gNameSelect[p] = -1;
        if (playerscr_gNameSelect[p] == -2) {
            if (playerscr_gNameLen[p] > 0) {
                playerscr_gNameLen[p]--;
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
            }
        } else if (playerscr_gNameSelect[p] == -1) {
            while (gCreatePlayerNames[p][playerscr_gNameLen[p] - 1] == ' ') {
                playerscr_gNameLen[p]--;
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
            }
            censor(gCreatePlayerNames[p]);
            if (gCreatePlayerNames[p][0]) {
                player = CreatePlayer(playerscr_gSelectPakIdx[p], gCreatePlayerNames[p]);
                if (player) {
                    claimPlayer(p, player);
                    playerscr_gSelectMenuSelect[p] = 3;
                    gSelectedPlayer[p] = player;
                    for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
                        if (pIdx != p && playerscr_gSelectPakIdx[pIdx] == playerscr_gSelectPakIdx[p] &&
                            playerscr_gSelectMenuSelect[pIdx] >= 3 && playerscr_gSelectMenuSelect[pIdx] < 11) {
                            playerscr_gSelectMenuSelect[pIdx] = 1;
                            gSelectedPlayer[pIdx] = 0;
                        }
                    }
                }
                playerscr_updatePakInfo();
                playerscr_updateSelectInfo(-1);
            }
            setPlayerState(p, 0);
        } else if (playerscr_gNameLen[p] < 12 && (playerscr_gNameSelect[p] != -3 || playerscr_gNameLen[p] > 0)) {
            if (playerscr_gNameSelect[p] == -3)
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = ' ';
            else
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = gPlayerNameChars[playerscr_gNameSelect[p]];
            playerscr_gNameLen[p]++;
            gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
            if (playerscr_gNameLen[p] == 12) playerscr_gNameSelect[p] = -1;
        }
    } else if (gPlayers[p].flags & 4) {
        PlayStartABSound(gPlayers[p].flags);
        if (playerscr_gNameLen[p] > 0) {
            playerscr_gNameLen[p]--;
            gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
        } else setPlayerState(p, 0);
    }
}

void controlWait(s32);
void controlControls(s32);
void controlPakFull(s32);
void controlPakDamaged(s32);
void updatePlayerScreen(void)
{
    s32 playerIdx;
    s32 p;
    while (1) {
        if (gSkipToGameState != 1) {
            if (gSkipToGameState != gGameState) {
                s32 i;
                Owner *player;
                for (i = 0; i < gNumPlayers; i++) {
                    if (gPlayers[i].owner != &gJustPlayers[i]) {
                        for (player = gPlayerFiles.head; player; player = player->body->next)
                            if (gPlayers[i].owner == player) break;
                        if (!player) {
                            gSkipToGameState = 1;
                            goto restart;
                        }
                    }
                    if (i > 1) player_mode_set(i, 1);
                    FindPlayerNum(i, 1);
                }
                for (; i < 4; i++)
                    if (i > 1) player_mode_set(i, 0);
                CalcUnlockedStuff();
                setGameState(64);
                return;
            }
            gSkipToGameState = 1;
        }
        break;
restart:;
    }
    if (!screenInited) playerscr_initScreen();
    do {} while (0); /* SHAPING (context): compiled-out block boundary, see header */
    for (playerIdx = 0; playerIdx < gNumPlayers; playerIdx++) {
        p = playerIdx;
        do {} while (0); /* SHAPING: the switch reads the state through the copy p; see header */
        switch (gPlayerState[p]) {
        case 0:
            playerscr_controlSelect(playerIdx);
            break;
        case 1:
            controlWait(playerIdx);
            break;
        case 2:
            controlControls(playerIdx);
            break;
        case 3:
            controlCreatePlayer(playerIdx);
            break;
        case 4:
            controlDeletePlayer(playerIdx);
            break;
        case 5:
            controlPakFull(playerIdx);
            break;
        case 6:
            controlPakDamaged(playerIdx);
            break;
        default:
            break;
        }
        playerscr_updateButtons(playerIdx);
    }
    updateControlScreen();
    {
        s8 allReady = 1;
        for (playerIdx = 0; playerIdx < gNumPlayers; playerIdx++)
            if (gPlayerState[playerIdx] != 1) allReady = 0;
        if (allReady == 1) exitPlayerScreen = 1;
    }
    if (gSoftReset) {
        playerscr_uninitScreen();
        setGameState(4);
    } else if (exitPlayerScreen) {
        switch (exitPlayerScreen) {
        case 1: {
            Owner *player;
            for (player = gPlayerFiles.head; player; player = player->body->next) {
                for (p = 0; p < gNumPlayers; p++)
                    if (player == gPlayers[p].owner) break;
                if (p >= gNumPlayers) PurgePlayerData(player);
            }
            for (p = 0; p < gNumPlayers; p++) {
                if (!ReadPlayerData(gPlayers[p].owner)) break;
                SaveSelectPlayer(gPlayers[p].owner);
                PS_LockFile((void **)gPlayers[p].owner->body->selection, 1);
                if (p > 1) player_mode_set(p, 1);
                FindPlayerNum(p, 1);
            }
            if (p < gNumPlayers) {
                s32 i;
                for (i = 0; i < gNumPlayers; i++) {
                    playerscr_gSelectMenuSelect[i] = 3;
                    setPlayerState(i, 0);
                }
                break;
            }
            for (; p < 4; p++)
                if (p > 1) player_mode_set(p, 0);
            CalcUnlockedStuff();
            playerscr_uninitScreen();
            setGameState(64);
            break;
        }
        case 2: {
            s32 i;
            playerscr_uninitScreen();
            setGameState(16);
            for (i = 0; i < gNumPlayers; i++) gPlayers[i].owner = 0;
            break;
        }
        }
        exitPlayerScreen = 0;
    }
    playerscr_updatePakInfo();
    playerscr_updateSelectInfo(-1);
}

/* controlWait@playerscr.c (MAT3): inlined into updatePlayerScreen (probe) */
void controlWait(s32 p)
{
    if (!gPlayers[p].owner ||
        (gPlayers[p].owner->body->selection &&
         PS_PakChanged(gPlayers[p].owner->body->selection->item->player)) ||
        (gPlayers[p].flags & 4)) {
        if (gPlayers[p].flags & 4) PlayStartABSound(gPlayers[p].flags);
        playerscr_updateSelectInfo(-1);
        setPlayerState(p, 0);
    }
}

/* controlControls@playerscr.c (MAT3): inlined into updatePlayerScreen (probe) */
void controlControls(s32 p)
{
    if (!controlScrActive[p]) {
        SaveFlush(1);
        PS_LockFile((void **)gPlayers[p].owner->body->selection, 0);
        PurgePlayerData(gPlayers[p].owner);
        playerscr_updateSelectInfo(-1);
        setPlayerState(p, 0);
        playerscr_updateButtons(p);
    }
}

/* controlPakDamaged@playerscr.c (MAT3): inlined into updatePlayerScreen (probe) */
void controlPakDamaged(s32 p)
{
    if (gPlayers[p].flags & 7) {
        PlayStartABSound(gPlayers[p].flags);
        setPlayerState(p, 0);
    }
}

/* controlPakFull@playerscr.c (MAT3): inlined into updatePlayerScreen (probe) */
void controlPakFull(s32 p)
{
    if (gPlayers[p].flags & 7) {
        PlayStartABSound(gPlayers[p].flags);
        setPlayerState(p, 0);
    }
}
