/* The player (profile select) screen (MAT3 playerscr.c): one window per player, each choosing a profile on a
 * controller pak (or playing without one), creating or deleting profiles, or setting its controls. This file has the
 * screen's logic: its set-up, the per-window states and their input, the 3D buttons; playerscr_draw.c draws its text.
 * updatePlayerScreen runs each frame in GS_PLAYER. */
#include "game/game.h"

VIEW(intptr_t[], gPakFull);
#define gPakFull gPakFull_view

/* The select menu's rows (playerscr_gSelectMenuSelect[], MAT3 enum_kSelectMenuCount; the N64 has no "format" row). */
#define kSelectMenuJustPlay 0           /* play without a profile */
#define kSelectMenuSelectPak 1          /* left and right pick the pak */
#define kSelectMenuPakStatus 2          /* the pak's status line: never selectable */
#define kSelectMenuSelectPlayer 3       /* 3-10: the pak's profiles */
#define kSelectMenuLastPlayer 10
#define kSelectMenuCreatePlayer 11
#define kSelectMenuDeletePlayer 12
#define kSelectMenuResetPak 13          /* never offered on the N64 */
#define kSelectMenuCount 14
#define IS_PROFILE_ROW(row) ((row) >= kSelectMenuSelectPlayer && (row) < kSelectMenuCreatePlayer)

/* Work out which rows player's select menu (or, for -1, every player's) offers (gSelectMenuValid) and shows
 * (gSelectMenuVisible, some shown greyed), and how many show. "Just play" and the pak row always; the pak status
 * line when the pak has no profiles or is full; a row per profile on the pak; create when the pak isn't full; delete
 * when it has a profile. While the player deletes a profile only the profile rows are offered. */
void playerscr_updateSelectInfo(s32 player)
{
    s32 first = player;
    s32 last = player;
    s32 j;
    if (player == -1) {first = 0;last = gNumPlayers - 1;}
    for (player = first; player <= last; player++) {
        gSelectPlayerNum[player] = playerscr_gNumPakPlayers[playerscr_gSelectPakIdx[player]];
        playerscr_gSelectMenuValid[player][kSelectMenuJustPlay] = 1;
        playerscr_gSelectMenuValid[player][kSelectMenuSelectPak] = 1;
        playerscr_gSelectMenuValid[player][kSelectMenuPakStatus] = 0;
        for (j = 0; j < 8; j++)
            playerscr_gSelectMenuValid[player][j + kSelectMenuSelectPlayer] = j < gSelectPlayerNum[player];
        playerscr_gSelectMenuValid[player][kSelectMenuCreatePlayer] = gSelectPlayerNum[player] >= 0 && !gPakFull[playerscr_gSelectPakIdx[player]];
        playerscr_gSelectMenuValid[player][kSelectMenuDeletePlayer] = gSelectPlayerNum[player] > 0;
        playerscr_gSelectMenuValid[player][kSelectMenuResetPak] = 0;
        if (gPlayerState[player] == kPlayerStateDeletePlayer) {
            playerscr_gSelectMenuValid[player][kSelectMenuJustPlay] = 0;
            playerscr_gSelectMenuValid[player][kSelectMenuSelectPak] = 0;
            playerscr_gSelectMenuValid[player][kSelectMenuCreatePlayer] = 0;
            playerscr_gSelectMenuValid[player][kSelectMenuDeletePlayer] = 0;
        }
        playerscr_gSelectMenuVisible[player][kSelectMenuJustPlay] = playerscr_gSelectMenuValid[player][kSelectMenuJustPlay];
        playerscr_gSelectMenuVisible[player][kSelectMenuSelectPak] = gPlayerState[player] == kPlayerStateDeletePlayer || playerscr_gSelectMenuValid[player][kSelectMenuSelectPak];
        playerscr_gSelectMenuVisible[player][kSelectMenuPakStatus] = gSelectPlayerNum[player] < 1 || (gPakFull[playerscr_gSelectPakIdx[player]] && gPlayerState[player] != kPlayerStateDeletePlayer);
        for (j = 0; j < 8; j++) playerscr_gSelectMenuVisible[player][j + kSelectMenuSelectPlayer] = playerscr_gSelectMenuValid[player][j + kSelectMenuSelectPlayer];
        playerscr_gSelectMenuVisible[player][kSelectMenuCreatePlayer] = playerscr_gSelectMenuValid[player][kSelectMenuCreatePlayer];
        playerscr_gSelectMenuVisible[player][kSelectMenuDeletePlayer] = playerscr_gSelectMenuValid[player][kSelectMenuDeletePlayer];
        playerscr_gSelectMenuVisible[player][kSelectMenuResetPak] = playerscr_gSelectMenuValid[player][kSelectMenuResetPak];
        playerscr_gSelectMenuCount[player] = 0;
        for (j = 0; j < kSelectMenuCount; j++) if (playerscr_gSelectMenuVisible[player][j]) playerscr_gSelectMenuCount[player]++;
    }
}

typedef struct Owner Owner;
/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10): pak is
 * the pak it is on. */
typedef struct PakFile {void *next,*prev;void (*RemoveFile)(void *),(*MoveFile)(void *,void *);u8 pak;} PakFile;
typedef struct PakFileHandle {PakFile *file;} PakFileHandle;
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): the links of gPlayerFiles (handles),
 * its pak file (NULL for a guest), its id, its name. */
typedef struct PlayerProfile {Owner *next,*previous;PakFileHandle *fileh;u8 id[8];u8 name[1];} PlayerProfile;
/* A profile handle (MAT3 tPlayerProfile **): the profiles are reached and listed through these. */
struct Owner {PlayerProfile *body;};
/* A player's pad record (MAT3 tPadState, gPlayers): 76 bytes, 80 on the PC. */
typedef struct Player {u8 slot,controller;u16 unaccessed2;u32 edges;u32 levels;u32 autoEdges;u8 unaccessed10[56];Owner *owner;} Player;
typedef struct Blit Blit;typedef struct FiveParts FiveParts;
typedef struct MultiBlit {char *name;s16 x,y,width,height,top,bottom,left,right;u32 z,alpha;s32 (*callback)(Blit *);u32 descriptor;} MultiBlit;
extern Player gPlayers[];
VIEW(u8[], gCarTeam);
#define gCarTeam gCarTeam_view

VIEW(MultiBlit[], multiBlit);
#define multiBlit multiBlit_view

extern void particle_lifetime_set(void);
extern FiveParts *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);

/* Set the screen up: lock the players' profile files, read the paks, start every window on the select menu (on
 * the first profile row) with its buttons, split the screen (three players get four windows), set the buttons'
 * layout by window count, and put up a pane per player and the text blit. */
void playerscr_initScreen(void)
{
    s32 p;
    for (p = 0; p < gNumPlayers; p++) {
        if (gPlayers[p].owner) PS_LockFile((void **)gPlayers[p].owner->body->fileh,0);
    }
    playerscr_updatePakInfo();
    for (p = 0; p < gNumPlayers; p++) {
        gCarTeam[p] = p;
        gPlayers[p].controller = p;
        playerscr_gSelectMenuSelect[p] = kSelectMenuSelectPlayer;
        setPlayerState(p,kPlayerStateSelect);
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

/* gSelectMenuMaxSize: the menu rows that fit in a window's text rectangle under its title line. */
void calcSelectMenuMaxSize(void)
{
    s32 rectHeight = playerscr_gPlayerRects[gNumPlayers - 1][0].h;
    if (gNumPlayers >= 2) HAL_SetFont(11);
    else HAL_SetFont(13);
    rectHeight -= TB_GetFontLineHeight();
    if (gNumPlayers >= 2) HAL_SetFont(12);
    else HAL_SetFont(10);
    gSelectMenuMaxSize = rectHeight / TB_GetFontLineHeight();
}

struct PlayerFiles { u32 unknown0,  unknown4 ; Owner * head,  * tail ; };
VIEW(struct PlayerFiles, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(Owner[], gJustPlayers);
#define gJustPlayers gJustPlayers_view

extern void *memset(void *,s32,u32);

/* Put player's window in state mode (kPlayerState*).
 * - Select: the player gives up its profile; if it had one still on a pak, the menu selects it (its place among
 *   the pak's profiles), else the player is a guest (gJustPlayers) on its own controller's pak and a profile row
 *   selection goes back to "just play".
 * - Create: an empty name.  Delete: the menu on the pak row, no question or notice up.
 * The buttons jump to their places, and the menu's rows are worked out again. */
void setPlayerState(s32 player,s32 mode)
{
    Owner *node,*other;
    if (mode == kPlayerStateSelect) {
        gSelectedPlayer[player] = 0;
        node = gPlayerFiles.head;
        while (node) {
            if (node == gPlayers[player].owner) break;
            node = node->body->next;
        }
        gPlayers[player].owner = 0;
        if (!node || !node->body->fileh) {
            gSelectedPlayer[player] = &gJustPlayers[player];
            if (gPlayerState[player] != kPlayerStateCreatePlayer && gPlayerState[player] != kPlayerStateDeletePlayer)
                playerscr_gSelectPakIdx[player] = gPlayers[player].controller;
            if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[player]))
                playerscr_gSelectMenuSelect[player] = kSelectMenuJustPlay;
        } else {
            gSelectedPlayer[player] = node;
            playerscr_gSelectPakIdx[player] = node->body->fileh->file->pak;
            if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[player])) {
                playerscr_gSelectMenuSelect[player] = kSelectMenuSelectPlayer;
                for (other = gPlayerFiles.head; other; other = other->body->next) {
                    if (other == node) break;
                    if (node->body->fileh->file->pak == other->body->fileh->file->pak)
                        if (playerscr_gSelectMenuSelect[player] < kSelectMenuLastPlayer) playerscr_gSelectMenuSelect[player]++;
                }
            }
        }
        gSelectMenuControls[player] = 0;
    } else if (mode == kPlayerStateCreatePlayer) {
        memset(gCreatePlayerNames[player],0,13);
        playerscr_gNameSelect[player] = 0;
        playerscr_gNameLen[player] = 0;
        gPlayers[player].owner = 0;
    } else if (mode == kPlayerStateDeletePlayer) {
        gDeleteMenuSelect[player] = kSelectMenuSelectPak;
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
VIEW(PlayerPresence[], gControllerData);
#define gControllerData gControllerData_view

/* Read the four paks: each one's profile count (gNumPakPlayers, or why it has none: no controller, no pak, damaged,
 * a Rumble Pak) and whether it is too full for a new profile (gPakFull: fewer free pages than a profile takes, or no
 * free note). A pak that has just become usable makes the windows on it put their buttons back in place. */
void playerscr_updatePakInfo(void)
{
    s32 i,j;
    Owner *node;
    u32 available;
    for(i=0;i<4;i++) {
        gPakFull[i]=0;
        if(!gControllerData[i].present)playerscr_gNumPakPlayers[i]=PAK_NO_CONTROLLER;
        else if(!PS_PakInserted(i))playerscr_gNumPakPlayers[i]=PAK_NOT_INSERTED;
        else if(PS_PakDamaged(i))playerscr_gNumPakPlayers[i]=PAK_DAMAGED;
        else if(PS_PakRumble(i))playerscr_gNumPakPlayers[i]=PAK_RUMBLE;
        else {
            if(playerscr_gNumPakPlayers[i]<0) {
                for(j=0;j<gNumPlayers;j++) {
                    if(playerscr_gSelectPakIdx[j]==i)playerscr_buttonsJustInited[j]=1;
                }
            }
            playerscr_gNumPakPlayers[i]=0;
            for(node=gPlayerFiles.head;node;node=node->body->next) {
                if(node->body->fileh->file->pak==i)playerscr_gNumPakPlayers[i]++;
            }
            if(playerscr_gNumPakPlayers[i]==0)playerscr_gNumPakPlayers[i]=0;
            available=PS_GetFreeBlocks(i);
            gPakFull[i]=available<(u32)PS_GetBlocks(PROFILE_BYTES) || !PS_GetFreeFiles(i);
        }
    }
}
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view
extern void *memcpy(void *,const void *,u32);

/* PC: ZOID_SetColor reads a colour as one word, so the colours passed to it are word-aligned (docs/port/SANITIZE.md
 * 3.2) */
extern void *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);
extern void ZOID_SetTexture(s16,void *,s32);

/* Make player's 22 buttons that don't exist yet, each a model by its type (playerscr_buttonNames; types 0-2 and 7-9
 * draw see-through), white, shown only in that player's window; then texture and place the title, the cursor and the
 * A and B icons (a little lower with more than one player). scale is 1: the multiplications by it are the
 * original's. */
void playerscr_initButtons(s8 player)
{
    u8 color[4] PC_WORD_ALIGNED = {255,255,255,255};
    f32 scale=1.0f;
    s16 count;
    s32 j;
    MenuButton *slot;
    void *resource;
    for(j=0;j<PLAYER_BUTTONS;j++) {
        slot=&playerscr_buttonInfo[player][j];
        if(slot->handle==-1) {
            memcpy(slot->matrix,unitmatrix,36);
            resource=MBOX_FindObject_WORK(playerscr_buttonNames[slot->type],0,gNumModels-1,1);
            slot->handle=MBOX_NewObject(resource,slot->matrix,-1,
                slot->type==0 || slot->type==1 || slot->type==2 ||
                slot->type==7 || slot->type==8 || slot->type==9 ? 0x42000:0);
            ZOID_SetColor(slot->handle,color);
            MBOX_HideObject(slot->handle,1,15);
            MBOX_ShowObject(slot->handle,0,1<<player);
        }
    }
    resource=MBOX_FindTexture_WORK(playerscr_title_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].handle,resource,-1);
    memcpy(playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].matrix);
    playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[0]=0.0f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[1]=gNumPlayers>=2?105.0f:115.0f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[0]*=scale;
    playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[1]*=scale;
    playerscr_buttonInfo[player][PLAYER_BUTTON_TITLE].position[2]=200.0f*scale;
    resource=MBOX_FindTexture_WORK(playerscr_cursor_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][PLAYER_BUTTON_CURSOR].handle,resource,-1);
    resource=MBOX_FindTexture_WORK(playerscr_a_button_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].handle,resource,-1);
    memcpy(playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].matrix);
    playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].position[0]=75.1f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].position[2]=100.0f;
    ScaleUV(playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].matrix,playerscr_buttonInfo[player][PLAYER_BUTTON_A_ICON].matrix,.5f);
    memcpy(playerscr_buttonInfo[player][PLAYER_BUTTON_A_BACK].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][PLAYER_BUTTON_A_BACK].matrix);
    WRollUV(3.14159265359f,playerscr_buttonInfo[player][PLAYER_BUTTON_A_BACK].matrix);
    playerscr_buttonInfo[player][PLAYER_BUTTON_A_BACK].position[0]=87.1f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_A_BACK].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_A_BACK].position[2]=100.0f;
    resource=MBOX_FindTexture_WORK(playerscr_b_button_texture_name,&count,0,gNumModels-1,1);
    ZOID_SetTexture(playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].handle,resource,-1);
    memcpy(playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].matrix);
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].position[0]=-75.1f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].position[0]*=scale;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].position[1]*=scale;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].position[2]=100.0f*scale;
    ScaleUV(playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].matrix,playerscr_buttonInfo[player][PLAYER_BUTTON_B_ICON].matrix,.5f);
    memcpy(playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].matrix,unitmatrix,36);
    WPitchUV(-1.57079632679f,playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].matrix);
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].position[0]=-87.1f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].position[1]=gNumPlayers>=2?50.2f:55.2f;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].position[0]*=scale;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].position[1]*=scale;
    playerscr_buttonInfo[player][PLAYER_BUTTON_B_BACK].position[2]=100.0f*scale;
    playerscr_buttonsInited[player]=1;
    playerscr_buttonsJustInited[player]=1;
    ZeroCameras();
}

extern f32 fabsf(f32);
#pragma intrinsic (fabsf)
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* Lay player p's buttons out for this frame. Waiting: only the title and the B icon show. Creating, or deleting with
 * a question or notice up: the title and the A and B icons. Controls, a full or damaged pak: nothing (the controls
 * screen or a message box has the window). Otherwise the menu: the cursor spins beside the selected row and eases
 * towards it; each row's button turns to its selected face when selected (not while the controls button is), the
 * list scrolls to keep it in view and fades at the window's top and bottom, hidden rows have no button; the pak row
 * has a second button for its value, and the selected profile (or "just play") the controls button beside it. */
void playerscr_updateButtons(s8 p)
{
    s32 buttonIdx;
    s32 i;
    f32 prevPos[3];
    s32 whichItem;
    f32 buttonY;
    f32 selectedPos;
    s8 clearScrollSpeed = 1;
    void *tex;
    s16 texIdx;
    u8 buttonColor[4] PC_WORD_ALIGNED = {255,255,255,255};
    if (!playerscr_buttonsInited[p]) return;
    if (gPlayerState[p] == kPlayerStateWait) {
        for (buttonIdx = 0; buttonIdx < PLAYER_BUTTONS; buttonIdx++) {
            if (buttonIdx == PLAYER_BUTTON_TITLE || buttonIdx == PLAYER_BUTTON_B_ICON || buttonIdx == PLAYER_BUTTON_B_BACK)
                MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].handle, 0, 1 << p);
            else MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].handle, 1, 15);
        }
        return;
    }
    if (gPlayerState[p] == kPlayerStateCreatePlayer || (gPlayerState[p] == kPlayerStateDeletePlayer && (gDeleteMenuNotify[p] == 1 || gDeleteMenuConfirm[p] == 1))) {
        for (buttonIdx = 0; buttonIdx < PLAYER_BUTTONS; buttonIdx++) {
            if (buttonIdx == PLAYER_BUTTON_TITLE || (buttonIdx == PLAYER_BUTTON_B_ICON || buttonIdx == PLAYER_BUTTON_B_BACK) || buttonIdx == PLAYER_BUTTON_A_ICON || buttonIdx == PLAYER_BUTTON_A_BACK)
                MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].handle, 0, 1 << p);
            else MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].handle, 1, 15);
        }
        return;
    }
    if (gPlayerState[p] == kPlayerStateControls || (gPlayerState[p] == kPlayerStatePakFull || gPlayerState[p] == kPlayerStatePakDamaged) || gPlayerState[p] == kPlayerStatePaks) {
        for (buttonIdx = 0; buttonIdx < PLAYER_BUTTONS; buttonIdx++) MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].handle, 1, 15);
        return;
    }
    for (buttonIdx = 0; buttonIdx < PLAYER_BUTTONS; buttonIdx++) MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].handle, 0, 1 << p);
    whichItem = 0;
    if (gPlayerState[p] == kPlayerStateSelect) {
        for (i = 0; i < playerscr_gSelectMenuSelect[p]; i++) if (playerscr_gSelectMenuVisible[p][i]) whichItem++;
    } else if (gPlayerState[p] == kPlayerStateDeletePlayer) {
        for (i = 0; i < gDeleteMenuSelect[p]; i++) if (playerscr_gSelectMenuVisible[p][i]) whichItem++;
    }
    if (playerscr_gButtonScale * ((playerscr_gSelectMenuCount[p] - 1) * playerscr_gButtonSpace) > playerscr_maxY - playerscr_minY)
        selectedPos = playerscr_maxY - whichItem * (playerscr_maxY - playerscr_minY) / (playerscr_gSelectMenuCount[p] - 1);
    else selectedPos = playerscr_maxY - playerscr_gButtonScale * (whichItem * playerscr_gButtonSpace);
    memcpy(prevPos, playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position, 12);
    playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].angle += 6.2831855f * gFrameSeconds;
    if (playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].angle > 6.2831855f) playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].angle -= 6.2831855f;
    memcpy(playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].matrix, unitmatrix, 36);
    playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[0] = playerscr_buttonX * playerscr_cursorZ / playerscr_buttonZ + -165.0f * playerscr_gButtonScale;
    playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] = selectedPos * playerscr_cursorZ / playerscr_buttonZ;
    playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[2] = playerscr_cursorZ;
    if (!playerscr_buttonsJustInited[p]) {
        if (fabsf(playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] - prevPos[1]) * 60 * playerscr_gButtonScale / 10 > playerscr_cursorSpeed[p]) {
            playerscr_cursorSpeed[p] = 60 * fabsf(playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] - prevPos[1]) * playerscr_gButtonScale / 10;
            playerscr_cursorSpeed[p] = playerscr_cursorSpeed[p] < 540 * playerscr_gButtonScale ? playerscr_cursorSpeed[p] : 540 * playerscr_gButtonScale;
            playerscr_cursorSpeed[p] = playerscr_cursorSpeed[p] > 180 * playerscr_gButtonScale ? playerscr_cursorSpeed[p] : 180 * playerscr_gButtonScale;
        }
        if (playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] > prevPos[1] + gFrameSeconds * playerscr_cursorSpeed[p])
            playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] = prevPos[1] + gFrameSeconds * playerscr_cursorSpeed[p];
        else if (playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] < prevPos[1] - gFrameSeconds * playerscr_cursorSpeed[p])
            playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].position[1] = prevPos[1] - gFrameSeconds * playerscr_cursorSpeed[p];
        else playerscr_cursorSpeed[p] = 180 * playerscr_gButtonScale;
    }
    WRollUV(playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].angle, playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].matrix);
    WPitchUV(1.5707964f, playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].matrix);
    ScaleUV(playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].matrix, playerscr_buttonInfo[p][PLAYER_BUTTON_CURSOR].matrix, playerscr_gButtonScale * 0.4f);
    buttonY = selectedPos + playerscr_gButtonScale * (whichItem * playerscr_gButtonSpace);
    for (buttonIdx = 0; buttonIdx < kSelectMenuCount; buttonIdx++) {
        if (((gPlayerState[p] == kPlayerStateSelect && buttonIdx == playerscr_gSelectMenuSelect[p]) ||
             (gPlayerState[p] == kPlayerStateDeletePlayer && buttonIdx == gDeleteMenuSelect[p])) && !gSelectMenuControls[p]) {
            if (playerscr_buttonInfo[p][buttonIdx].angle < 3.1415927f) playerscr_buttonInfo[p][buttonIdx].angle += 12.566371f * gFrameSeconds;
            if (playerscr_buttonInfo[p][buttonIdx].angle > 3.1415927f) playerscr_buttonInfo[p][buttonIdx].angle = 3.1415927f;
            if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][buttonIdx].angle = 3.1415927f;
        } else {
            if (playerscr_buttonInfo[p][buttonIdx].angle > 0) playerscr_buttonInfo[p][buttonIdx].angle -= 12.566371f * gFrameSeconds;
            if (playerscr_buttonInfo[p][buttonIdx].angle < 0) playerscr_buttonInfo[p][buttonIdx].angle = 0;
            if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][buttonIdx].angle = 0;
        }
        memcpy(prevPos, playerscr_buttonInfo[p][buttonIdx].position, 12);
        if (playerscr_buttonInfo[p][buttonIdx].type == 9) playerscr_buttonInfo[p][buttonIdx].position[0] = playerscr_buttonX + 0 * playerscr_gButtonScale;
        else if (playerscr_buttonInfo[p][buttonIdx].type == 7) playerscr_buttonInfo[p][buttonIdx].position[0] = playerscr_buttonX + -80 * playerscr_gButtonScale;
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
        ZOID_SetTexture(playerscr_buttonInfo[p][buttonIdx].handle, tex, -1);
        if (playerscr_buttonInfo[p][buttonIdx].position[1] > playerscr_maxY + playerscr_gButtonSpace * playerscr_gButtonScale ||
            playerscr_buttonInfo[p][buttonIdx].position[1] < playerscr_minY - playerscr_gButtonSpace * playerscr_gButtonScale) playerscr_buttonInfo[p][buttonIdx].alpha = 0;
        else if (playerscr_buttonInfo[p][buttonIdx].position[1] > playerscr_maxY)
            playerscr_buttonInfo[p][buttonIdx].alpha = 255.f - 255.0f * (playerscr_buttonInfo[p][buttonIdx].position[1] - playerscr_maxY) / (playerscr_gButtonSpace * playerscr_gButtonScale);
        else if (playerscr_buttonInfo[p][buttonIdx].position[1] < playerscr_minY)
            playerscr_buttonInfo[p][buttonIdx].alpha = 255.f - 255.0f * (playerscr_minY - playerscr_buttonInfo[p][buttonIdx].position[1]) / (playerscr_gButtonSpace * playerscr_gButtonScale);
        else playerscr_buttonInfo[p][buttonIdx].alpha = 255;
        if (!playerscr_gSelectMenuVisible[p][buttonIdx]) playerscr_buttonInfo[p][buttonIdx].alpha = 0;
        buttonColor[3] = playerscr_buttonInfo[p][buttonIdx].alpha;
        ZOID_SetColor(playerscr_buttonInfo[p][buttonIdx].handle, buttonColor);
        if (!playerscr_gSelectMenuVisible[p][buttonIdx] || !playerscr_buttonInfo[p][buttonIdx].alpha) MBOX_HideObject(playerscr_buttonInfo[p][buttonIdx].handle, 1, 15);
        else MBOX_ShowObject(playerscr_buttonInfo[p][buttonIdx].handle, 0, 1 << p);
        if (buttonIdx == 1) {
            playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].position[0] = playerscr_buttonX + 160 * playerscr_gButtonScale;
            playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].position[1] = playerscr_buttonInfo[p][buttonIdx].position[1];
            playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].position[2] = playerscr_buttonInfo[p][buttonIdx].position[2];
            playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].angle = playerscr_buttonInfo[p][buttonIdx].angle;
            ScaleUV(unitmatrix, playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].matrix, playerscr_gButtonScale);
            WPitchUV(playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].angle - 1.5707964f, playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].matrix);
            ZOID_SetTexture(playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].handle, tex, -1);
            playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].alpha = playerscr_buttonInfo[p][buttonIdx].alpha;
            ZOID_SetColor(playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].handle, buttonColor);
            if (!playerscr_gSelectMenuVisible[p][buttonIdx] || !playerscr_buttonInfo[p][buttonIdx].alpha) MBOX_HideObject(playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].handle, 1, 15);
            else MBOX_ShowObject(playerscr_buttonInfo[p][PLAYER_BUTTON_PAK_VALUE].handle, 0, 1 << p);
        } else {
            if (gPlayerState[p] == kPlayerStateSelect && (playerscr_gSelectMenuSelect[p] == kSelectMenuJustPlay || (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p])))) {
                if (buttonIdx == playerscr_gSelectMenuSelect[p]) {
                    playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].position[0] = 120 * playerscr_gButtonScale + playerscr_buttonX;
                    playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].position[1] = playerscr_buttonInfo[p][buttonIdx].position[1];
                    playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].position[2] = playerscr_buttonInfo[p][buttonIdx].position[2];
                    if (gSelectMenuControls[p]) {
                        if (playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle < 3.1415927f) playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle += 12.566371f * gFrameSeconds;
                        if (playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle > 3.1415927f) playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle = 3.1415927f;
                        if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle = 3.1415927f;
                    } else {
                        if (playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle > 0) playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle -= 12.566371f * gFrameSeconds;
                        if (playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle < 0) playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle = 0;
                        if (playerscr_buttonsJustInited[p]) playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle = 0;
                    }
                    ScaleUV(unitmatrix, playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].matrix, playerscr_gButtonScale);
                    WPitchUV(playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle - 1.5707964f, playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].matrix);
                    if (playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle > 1.5707964f)
                        tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
                    else tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
                    ZOID_SetTexture(playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].handle, tex, -1);
                    playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].alpha = playerscr_buttonInfo[p][buttonIdx].alpha;
                    ZOID_SetColor(playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].handle, buttonColor);
                    if (!playerscr_gSelectMenuVisible[p][buttonIdx] || !playerscr_buttonInfo[p][buttonIdx].alpha) {
                        playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle = 0;
                        MBOX_HideObject(playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].handle, 1, 15);
                    } else MBOX_ShowObject(playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].handle, 0, 1 << p);
                }
            } else {
                playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].angle = 0;
                MBOX_HideObject(playerscr_buttonInfo[p][PLAYER_BUTTON_CONTROLS].handle, 1, 15);
            }
        }
        if (playerscr_gSelectMenuVisible[p][buttonIdx] == 1) buttonY -= playerscr_gButtonSpace * playerscr_gButtonScale;
    }
    if (clearScrollSpeed == 1) playerscr_scrollSpeed[p] = 0;
    playerscr_buttonsJustInited[p] = 0;
}

extern void player_mode_set ( s32 , s32 );

VIEW(char *, gPlayerNameChars);
#define gPlayerNameChars gPlayerNameChars_view

/* The delete menu's input for player p (MAT3 playerscr.c controlDeletePlayer): pick a profile on a pak (up and
 * down through the profiles of the chosen pak, skipping the ones other players hold; left and right change the pak),
 * A asks "are you sure" (left and right flip yes and no; A on yes deletes it and says so), Start or B backs out of a
 * question or the menu. With no profiles left it goes back to the select menu. */
void controlDeletePlayer(s32 p)
{
    s8 origSelect;
    s32 pak;
    s32 pIdx;
    Owner *player;
    pak = playerscr_gSelectPakIdx[p];
    origSelect = gDeleteMenuSelect[p];
    if (playerscr_gNumPakPlayers[pak] <= 0) {
        setPlayerState(p, kPlayerStateSelect);
        return;
    }
    while (!playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1) {
        gDeleteMenuSelect[p]++;
        if (gDeleteMenuSelect[p] > kSelectMenuCount - 1) gDeleteMenuSelect[p] = 0;
        if (gDeleteMenuSelect[p] == origSelect) {
            setPlayerState(p, kPlayerStateSelect);
            return;
        }
        if (IS_PROFILE_ROW(gDeleteMenuSelect[p]) && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
            if (gSelectedPlayer[p]) {
                do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->next; }
                while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
            }
            if (!gSelectedPlayer[p]) {
                gSelectedPlayer[p] = gPlayerFiles.head;
                while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p])
                    gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
            }
        } else gSelectedPlayer[p] = 0;
        playerscr_buttonsJustInited[p] = 1;
    }
    if (gPlayers[p].edges & BUTTON_START) {
        PlayStartABSound(gPlayers[p].edges);
        if (gDeleteMenuNotify[p] == 1) {
            gDeleteMenuNotify[p] = 0;
            setPlayerState(p, kPlayerStateSelect);
        } else if (gDeleteMenuConfirm[p] == 1) {
            gDeleteMenuConfirm[p] = 0;
            gDeleteMenuYes[p] = 0;
        } else setPlayerState(p, kPlayerStateSelect);
    } else if (gPlayers[p].edges & BUTTON_A) {
        if (gDeleteMenuNotify[p] == 1) {
            PlayStartABSound(gPlayers[p].edges);
            gDeleteMenuNotify[p] = 0;
            setPlayerState(p, kPlayerStateSelect);
        } else if (gDeleteMenuConfirm[p] == 1) {
            if (gDeleteMenuYes[p] == 1) {
                PlayStartABSound(gPlayers[p].edges);
                DeletePlayer(gSelectedPlayer[p]);
                for (player = gPlayerFiles.head; player; player = player->body->next)
                    if (gSelectedPlayer[p] == player) break;
                if (!player) {
                    for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
                        if (pIdx != p && playerscr_gSelectPakIdx[pIdx] == playerscr_gSelectPakIdx[p] &&
                            playerscr_gSelectMenuSelect[pIdx] >= kSelectMenuSelectPlayer && playerscr_gSelectMenuSelect[pIdx] < kSelectMenuCreatePlayer) {
                            playerscr_gSelectMenuSelect[pIdx] = kSelectMenuSelectPak;
                            gSelectedPlayer[pIdx] = 0;
                        }
                    }
                    gSelectedPlayer[p] = 0;
                    playerscr_updatePakInfo();
                    playerscr_updateSelectInfo(-1);
                    gDeleteMenuSelect[p] = 0;
                    gDeleteMenuNotify[p] = 1;
                }
            } else PlayStartABSound(BUTTON_B);
            gDeleteMenuConfirm[p] = 0;
            gDeleteMenuYes[p] = 0;
        } else if (IS_PROFILE_ROW(gDeleteMenuSelect[p])) {
            PlayStartABSound(gPlayers[p].edges);
            TB_Strcpy(gDeleteMenuName[p], gSelectedPlayer[p]->body->name);
            gDeleteMenuConfirm[p] = 1;
            gDeleteMenuYes[p] = 0;
        }
    } else if (gPlayers[p].edges & BUTTON_B) {
        PlayStartABSound(gPlayers[p].edges);
        if (gDeleteMenuNotify[p] == 1) {
            gDeleteMenuNotify[p] = 0;
            setPlayerState(p, kPlayerStateSelect);
        } else if (gDeleteMenuConfirm[p] == 1) {
            gDeleteMenuConfirm[p] = 0;
            gDeleteMenuYes[p] = 0;
        } else setPlayerState(p, kPlayerStateSelect);
    }
    if (gPlayers[p].autoEdges & BUTTON_RIGHT) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) {
            PlayLRSound(gPlayers[p].edges);
            gDeleteMenuYes[p] = !gDeleteMenuYes[p];
        } else if (gDeleteMenuSelect[p] == kSelectMenuSelectPak) {
            playerscr_gSelectPakIdx[p]++;
            if (playerscr_gSelectPakIdx[p] > 3) playerscr_gSelectPakIdx[p] = 0;
            playerscr_updateSelectInfo(p);
            PlayLRSound(gPlayers[p].edges);
        }
    } else if (gPlayers[p].autoEdges & BUTTON_LEFT) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) {
            PlayLRSound(gPlayers[p].edges);
            gDeleteMenuYes[p] = !gDeleteMenuYes[p];
        } else if (gDeleteMenuSelect[p] == kSelectMenuSelectPak) {
            playerscr_gSelectPakIdx[p]--;
            if (playerscr_gSelectPakIdx[p] < 0) playerscr_gSelectPakIdx[p] = 3;
            playerscr_updateSelectInfo(p);
            PlayLRSound(gPlayers[p].edges);
        }
    } else if (gPlayers[p].autoEdges & BUTTON_UP) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) return;
        PlayUDSound(gPlayers[p].edges);
        do {
            if (gDeleteMenuSelect[p] == kSelectMenuSelectPlayer && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            gDeleteMenuSelect[p]--;
            if (gDeleteMenuSelect[p] < 0) gDeleteMenuSelect[p] = kSelectMenuCount - 1;
            if (IS_PROFILE_ROW(gDeleteMenuSelect[p]) && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.tail;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1);
    } else if (gPlayers[p].autoEdges & BUTTON_DOWN) {
        if (gDeleteMenuNotify[p] == 1) return;
        if (gDeleteMenuConfirm[p] == 1) return;
        PlayUDSound(gPlayers[p].edges);
        do {
            if (gDeleteMenuSelect[p] == kSelectMenuLastPlayer && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->next; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            gDeleteMenuSelect[p]++;
            if (gDeleteMenuSelect[p] > kSelectMenuCount - 1) gDeleteMenuSelect[p] = 0;
            if (IS_PROFILE_ROW(gDeleteMenuSelect[p]) && playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do { gSelectedPlayer[p] = gSelectedPlayer[p]->body->next; }
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.head;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][gDeleteMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1);
    }
}

/* playerBeingDeleted and playerBeingCreated take the player to skip as their second parameter after an unused one
 * (src/ovl_a/playerBeingDeleted.c: the N64 passed it in the second register); these calls give only the player. */

#define playerBeingDeleted(skip) playerBeingDeleted(0, skip)
#define playerBeingCreated(skip) playerBeingCreated(0, skip)

/* The select menu's input for player p (MAT3 playerscr.c controlSelect). The selection first moves off a row that
 * isn't offered, a profile another player holds, or create or delete while another player is doing so. Start takes
 * the row as it is (a profile, else "just play") and waits for the others; A does the same, or with the controls
 * button turned opens the controls screen for the profile, or opens create, delete or the pak's reset; B leaves for
 * the start menu. Left and right turn the controls button (on "just play" or a profile) or change the pak; up and
 * down move through the offered rows, a profile row stepping through the pak's profiles. */
void playerscr_controlSelect(s32 p)
{
    while (!playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]] ||
           playerClaimed(p, gSelectedPlayer[p]) == 1 ||
           (playerscr_gSelectMenuSelect[p] == kSelectMenuCreatePlayer && playerBeingCreated(p) == 1) ||
           (playerscr_gSelectMenuSelect[p] == kSelectMenuDeletePlayer && playerBeingDeleted(p) == 1)) {
        gSelectMenuControls[p] = 0;
        playerscr_gSelectMenuSelect[p]--;
        if (playerscr_gSelectMenuSelect[p] < 0) playerscr_gSelectMenuSelect[p] = kSelectMenuCount - 1;
        if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p]) && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
            if (gSelectedPlayer[p]) {
                do {
                    gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
            }
            if (!gSelectedPlayer[p]) {
                gSelectedPlayer[p] = gPlayerFiles.tail;
                while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p])
                    gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
            }
        } else gSelectedPlayer[p] = 0;
    }
    if (gPlayers[p].edges & BUTTON_START) {
        PlayStartABSound(gPlayers[p].edges);
        if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p]))
            claimPlayer(p, gSelectedPlayer[p]);
        else
            claimPlayer(p, &gJustPlayers[p]);
        setPlayerState(p, kPlayerStateWait);
    } else if (gPlayers[p].edges & BUTTON_A) {
        PlayStartABSound(gPlayers[p].edges);
        if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p])) {
            claimPlayer(p, gSelectedPlayer[p]);
            if (gSelectMenuControls[p] == 1) {
                if (ReadPlayerData(gPlayers[p].owner)) {
                    controlScrActive[p] = 1;
                    PS_LockFile((void **)gPlayers[p].owner->body->fileh, 1);
                    setPlayerState(p, kPlayerStateControls);
                }
            } else setPlayerState(p, kPlayerStateWait);
        } else switch (playerscr_gSelectMenuSelect[p]) {
        case kSelectMenuJustPlay:
            claimPlayer(p, &gJustPlayers[p]);
            if (gSelectMenuControls[p] == 1) {
                controlScrActive[p] = 1;
                PS_LockFile((void **)gPlayers[p].owner->body->fileh, 1);
                setPlayerState(p, kPlayerStateControls);
            } else setPlayerState(p, kPlayerStateWait);
            break;
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
            break;
        case kSelectMenuCreatePlayer: setPlayerState(p, kPlayerStateCreatePlayer); break;
        case kSelectMenuDeletePlayer: gDeleteMenuSelect[p] = 0; setPlayerState(p, kPlayerStateDeletePlayer); break;
        case kSelectMenuResetPak: setPlayerState(p, kPlayerStatePakDamaged); break;
        }
    } else if (gPlayers[p].edges & BUTTON_B) {
        PlayStartABSound(gPlayers[p].edges);
        exitPlayerScreen = 2;
    }
    if (gPlayers[p].autoEdges & BUTTON_RIGHT) {
        if (playerscr_gSelectMenuSelect[p] == kSelectMenuJustPlay || (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p]))) {
            gSelectMenuControls[p] = !gSelectMenuControls[p];
            PlayLRSound(gPlayers[p].edges);
        } else if (playerscr_gSelectMenuSelect[p] == kSelectMenuSelectPak) {
            playerscr_gSelectPakIdx[p]++;
            if (playerscr_gSelectPakIdx[p] >= 4) playerscr_gSelectPakIdx[p] = 0;
            playerscr_updateSelectInfo(p);
            playerscr_buttonsJustInited[p] = 1;
            PlayLRSound(gPlayers[p].edges);
        }
    } else if (gPlayers[p].autoEdges & BUTTON_LEFT) {
        if (playerscr_gSelectMenuSelect[p] == kSelectMenuJustPlay || (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p]))) {
            gSelectMenuControls[p] = !gSelectMenuControls[p];
            PlayLRSound(gPlayers[p].edges);
        } else if (playerscr_gSelectMenuSelect[p] == kSelectMenuSelectPak) {
            playerscr_gSelectPakIdx[p]--;
            if (playerscr_gSelectPakIdx[p] < 0) playerscr_gSelectPakIdx[p] = 3;
            playerscr_updateSelectInfo(p);
            playerscr_buttonsJustInited[p] = 1;
            PlayLRSound(gPlayers[p].edges);
        }
    } else if (gPlayers[p].autoEdges & BUTTON_UP) {
        PlayUDSound(gPlayers[p].edges);
        gSelectMenuControls[p] = 0;
        do {
            if (playerscr_gSelectMenuSelect[p] == kSelectMenuSelectPlayer && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            playerscr_gSelectMenuSelect[p]--;
            if (playerscr_gSelectMenuSelect[p] < 0) playerscr_gSelectMenuSelect[p] = kSelectMenuCount - 1;
            if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p]) && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.tail;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->previous;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1 ||
                 (playerscr_gSelectMenuSelect[p] == kSelectMenuCreatePlayer && playerBeingCreated(p) == 1) ||
                 (playerscr_gSelectMenuSelect[p] == kSelectMenuDeletePlayer && playerBeingDeleted(p) == 1));
    } else if (gPlayers[p].autoEdges & BUTTON_DOWN) {
        PlayUDSound(gPlayers[p].edges);
        gSelectMenuControls[p] = 0;
        do {
            if (playerscr_gSelectMenuSelect[p] == kSelectMenuLastPlayer && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (gSelectedPlayer[p]) break;
            }
            playerscr_gSelectMenuSelect[p]++;
            if (playerscr_gSelectMenuSelect[p] >= kSelectMenuCount) playerscr_gSelectMenuSelect[p] = 0;
            if (IS_PROFILE_ROW(playerscr_gSelectMenuSelect[p]) && playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]]) {
                if (gSelectedPlayer[p]) {
                    do {
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                    } while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p]);
                }
                if (!gSelectedPlayer[p]) {
                    gSelectedPlayer[p] = gPlayerFiles.head;
                    while (gSelectedPlayer[p] && gSelectedPlayer[p]->body->fileh->file->pak != playerscr_gSelectPakIdx[p])
                        gSelectedPlayer[p] = gSelectedPlayer[p]->body->next;
                }
            } else gSelectedPlayer[p] = 0;
        } while (!playerscr_gSelectMenuValid[p][playerscr_gSelectMenuSelect[p]] || playerClaimed(p, gSelectedPlayer[p]) == 1 ||
                 (playerscr_gSelectMenuSelect[p] == kSelectMenuCreatePlayer && playerBeingCreated(p) == 1) ||
                 (playerscr_gSelectMenuSelect[p] == kSelectMenuDeletePlayer && playerBeingDeleted(p) == 1));
    }
}
/* Give player playerIdx profile `player` (gPlayers[].save) and work out every window's menu again. */
void claimPlayer(s32 playerIdx, Owner *player)
{
    gPlayers[playerIdx].owner = player;
    playerscr_updateSelectInfo(-1);
}

/* The name entry of a new profile for player p (MAT3 playerscr.c controlCreatePlayer): the d-pad moves over the
 * letter grid (as the high-score name entry), A adds a letter or a space, deletes or ends; Start ends; B deletes, or
 * backs out with an empty name. Ending trims trailing spaces, censors the name and, if any is left, creates the
 * profile on the chosen pak and gives it to the player (the other windows on that pak go back to the pak row). The
 * window goes back to the select menu if its pak becomes unusable or full. */
void controlCreatePlayer(s32 p)
{
    Owner *player;
    s32 pIdx;
    if (playerscr_gNumPakPlayers[playerscr_gSelectPakIdx[p]] < 0 || gPakFull[playerscr_gSelectPakIdx[p]])
        setPlayerState(p, kPlayerStateSelect);
    if (gPlayers[p].autoEdges & BUTTON_UP) {
        PlayUDSound(gPlayers[p].autoEdges);
        if (playerscr_gNameSelect[p] == NAME_SEL_END) playerscr_gNameSelect[p] = 52;
        else if (playerscr_gNameSelect[p] == NAME_SEL_DELETE) playerscr_gNameSelect[p] = 49;
        else if (playerscr_gNameSelect[p] == NAME_SEL_SPACE) playerscr_gNameSelect[p] = 46;
        else if (playerscr_gNameSelect[p] < 3) playerscr_gNameSelect[p] = NAME_SEL_SPACE;
        else if (playerscr_gNameSelect[p] < 6) playerscr_gNameSelect[p] = NAME_SEL_DELETE;
        else if (playerscr_gNameSelect[p] < 9) playerscr_gNameSelect[p] = NAME_SEL_END;
        else playerscr_gNameSelect[p] -= NAME_GRID_COLUMNS;
    } else if (gPlayers[p].autoEdges & BUTTON_DOWN) {
        PlayUDSound(gPlayers[p].autoEdges);
        if (playerscr_gNameSelect[p] == NAME_SEL_END) playerscr_gNameSelect[p] = 7;
        else if (playerscr_gNameSelect[p] == NAME_SEL_DELETE) playerscr_gNameSelect[p] = 4;
        else if (playerscr_gNameSelect[p] == NAME_SEL_SPACE) playerscr_gNameSelect[p] = 1;
        else if (playerscr_gNameSelect[p] < 45) playerscr_gNameSelect[p] += NAME_GRID_COLUMNS;
        else if (playerscr_gNameSelect[p] < 48) playerscr_gNameSelect[p] = NAME_SEL_SPACE;
        else if (playerscr_gNameSelect[p] < 51) playerscr_gNameSelect[p] = NAME_SEL_DELETE;
        else playerscr_gNameSelect[p] = NAME_SEL_END;
    } else if (gPlayers[p].autoEdges & BUTTON_LEFT) {
        PlayLRSound(gPlayers[p].autoEdges);
        if (playerscr_gNameSelect[p] == NAME_SEL_END) playerscr_gNameSelect[p] = NAME_SEL_DELETE;
        else if (playerscr_gNameSelect[p] == NAME_SEL_DELETE) playerscr_gNameSelect[p] = NAME_SEL_SPACE;
        else if (playerscr_gNameSelect[p] == NAME_SEL_SPACE) playerscr_gNameSelect[p] = NAME_SEL_END;
        else if (playerscr_gNameSelect[p] % 9 == 0) playerscr_gNameSelect[p] += 8;
        else playerscr_gNameSelect[p]--;
    } else if (gPlayers[p].autoEdges & BUTTON_RIGHT) {
        PlayLRSound(gPlayers[p].autoEdges);
        if (playerscr_gNameSelect[p] == NAME_SEL_END) playerscr_gNameSelect[p] = NAME_SEL_SPACE;
        else if (playerscr_gNameSelect[p] == NAME_SEL_DELETE) playerscr_gNameSelect[p] = NAME_SEL_END;
        else if (playerscr_gNameSelect[p] == NAME_SEL_SPACE) playerscr_gNameSelect[p] = NAME_SEL_DELETE;
        else if (playerscr_gNameSelect[p] % 9 == 8) playerscr_gNameSelect[p] -= 8;
        else playerscr_gNameSelect[p]++;
    }
    if (gPlayers[p].edges & BUTTON_ACCEPT) {
        PlayStartABSound(gPlayers[p].edges);
        if (gPlayers[p].edges & BUTTON_START) playerscr_gNameSelect[p] = NAME_SEL_END;
        if (playerscr_gNameSelect[p] == NAME_SEL_DELETE) {
            if (playerscr_gNameLen[p] > 0) {
                playerscr_gNameLen[p]--;
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
            }
        } else if (playerscr_gNameSelect[p] == NAME_SEL_END) {
            while (gCreatePlayerNames[p][playerscr_gNameLen[p] - 1] == ' ') {
                playerscr_gNameLen[p]--;
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
            }
            censor(gCreatePlayerNames[p]);
            if (gCreatePlayerNames[p][0]) {
                player = CreatePlayer(playerscr_gSelectPakIdx[p], gCreatePlayerNames[p]);
                if (player) {
                    claimPlayer(p, player);
                    playerscr_gSelectMenuSelect[p] = kSelectMenuSelectPlayer;
                    gSelectedPlayer[p] = player;
                    for (pIdx = 0; pIdx < gNumPlayers; pIdx++) {
                        if (pIdx != p && playerscr_gSelectPakIdx[pIdx] == playerscr_gSelectPakIdx[p] &&
                            playerscr_gSelectMenuSelect[pIdx] >= kSelectMenuSelectPlayer && playerscr_gSelectMenuSelect[pIdx] < kSelectMenuCreatePlayer) {
                            playerscr_gSelectMenuSelect[pIdx] = kSelectMenuSelectPak;
                            gSelectedPlayer[pIdx] = 0;
                        }
                    }
                }
                playerscr_updatePakInfo();
                playerscr_updateSelectInfo(-1);
            }
            setPlayerState(p, kPlayerStateSelect);
        } else if (playerscr_gNameLen[p] < NAME_MAX && (playerscr_gNameSelect[p] != NAME_SEL_SPACE || playerscr_gNameLen[p] > 0)) {
            if (playerscr_gNameSelect[p] == NAME_SEL_SPACE)
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = ' ';
            else
                gCreatePlayerNames[p][playerscr_gNameLen[p]] = gPlayerNameChars[playerscr_gNameSelect[p]];
            playerscr_gNameLen[p]++;
            gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
            if (playerscr_gNameLen[p] == NAME_MAX) playerscr_gNameSelect[p] = NAME_SEL_END;
        }
    } else if (gPlayers[p].edges & BUTTON_B) {
        PlayStartABSound(gPlayers[p].edges);
        if (playerscr_gNameLen[p] > 0) {
            playerscr_gNameLen[p]--;
            gCreatePlayerNames[p][playerscr_gNameLen[p]] = 0;
        } else setPlayerState(p, kPlayerStateSelect);
    }
}

/* The screen's frame (GS_PLAYER).
 * - After a race (gSkipToGameState), the players go straight on to the mode menu with their profiles, unless a
 *   profile has gone (its pak was pulled): then the screen runs as usual (the goto restarts the test, which then
 *   finds SKIP_NONE: MAT3's form).
 * - Each window's state: its input, then its buttons; then the controls screen for the windows that have it open.
 * - When every player is waiting, the profiles nobody holds are purged and each player's profile data is read and
 *   locked (the second and later players' sticks act as d-pads); a profile that can't be read sends everyone back to
 *   the select menu; else on to the mode menu (GS_MODE). B on a select menu goes back to the start menu; a soft reset
 *   to the title.
 * - The paks and the menus are read again every frame. */
void updatePlayerScreen(void)
{
    s32 playerIdx;
    s32 p;
    while (1) {
        if (gSkipToGameState != SKIP_NONE) {
            if (gSkipToGameState != gGameState) {
                s32 i;
                Owner *player;
                for (i = 0; i < gNumPlayers; i++) {
                    if (gPlayers[i].owner != &gJustPlayers[i]) {
                        for (player = gPlayerFiles.head; player; player = player->body->next)
                            if (gPlayers[i].owner == player) break;
                        if (!player) {
                            gSkipToGameState = SKIP_NONE;
                            goto restart;
                        }
                    }
                    if (i > 1) player_mode_set(i, 1);
                    FindPlayerNum(i, 1);
                }
                for (; i < 4; i++)
                    if (i > 1) player_mode_set(i, 0);
                CalcUnlockedStuff();
                setGameState(GS_MODE);
                return;
            }
            gSkipToGameState = SKIP_NONE;
        }
        break;
restart:;
    }
    if (!screenInited) playerscr_initScreen();
    for (playerIdx = 0; playerIdx < gNumPlayers; playerIdx++) {
        p = playerIdx;
        switch (gPlayerState[p]) {
        case kPlayerStateSelect:
            playerscr_controlSelect(playerIdx);
            break;
        case kPlayerStateWait:
            controlWait(playerIdx);
            break;
        case kPlayerStateControls:
            controlControls(playerIdx);
            break;
        case kPlayerStateCreatePlayer:
            controlCreatePlayer(playerIdx);
            break;
        case kPlayerStateDeletePlayer:
            controlDeletePlayer(playerIdx);
            break;
        case kPlayerStatePakFull:
            controlPakFull(playerIdx);
            break;
        case kPlayerStatePakDamaged:
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
            if (gPlayerState[playerIdx] != kPlayerStateWait) allReady = 0;
        if (allReady == 1) exitPlayerScreen = 1;
    }
    if (gSoftReset) {
        playerscr_uninitScreen();
        setGameState(GS_TITLE);
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
                PS_LockFile((void **)gPlayers[p].owner->body->fileh, 1);
                if (p > 1) player_mode_set(p, 1);
                FindPlayerNum(p, 1);
            }
            if (p < gNumPlayers) {
                s32 i;
                for (i = 0; i < gNumPlayers; i++) {
                    playerscr_gSelectMenuSelect[i] = kSelectMenuSelectPlayer;
                    setPlayerState(i, kPlayerStateSelect);
                }
                break;
            }
            for (; p < 4; p++)
                if (p > 1) player_mode_set(p, 0);
            CalcUnlockedStuff();
            playerscr_uninitScreen();
            setGameState(GS_MODE);
            break;
        }
        case 2: {
            s32 i;
            playerscr_uninitScreen();
            setGameState(GS_START);
            for (i = 0; i < gNumPlayers; i++) gPlayers[i].owner = 0;
            break;
        }
        }
        exitPlayerScreen = 0;
    }
    playerscr_updatePakInfo();
    playerscr_updateSelectInfo(-1);
}

/* A waiting player (MAT3 playerscr.c controlWait) goes back to the select menu with B, or when its profile has gone
 * or its pak has been changed. */
void controlWait(s32 p)
{
    if (!gPlayers[p].owner ||
        (gPlayers[p].owner->body->fileh &&
         PS_PakChanged(gPlayers[p].owner->body->fileh->file->pak)) ||
        (gPlayers[p].edges & BUTTON_B)) {
        if (gPlayers[p].edges & BUTTON_B) PlayStartABSound(gPlayers[p].edges);
        playerscr_updateSelectInfo(-1);
        setPlayerState(p, kPlayerStateSelect);
    }
}

/* When player p closes the controls screen (MAT3 playerscr.c controlControls): save the profile, unlock and purge
 * its data, and go back to the select menu. */
void controlControls(s32 p)
{
    if (!controlScrActive[p]) {
        SaveFlush(1);
        PS_LockFile((void **)gPlayers[p].owner->body->fileh, 0);
        PurgePlayerData(gPlayers[p].owner);
        playerscr_updateSelectInfo(-1);
        setPlayerState(p, kPlayerStateSelect);
        playerscr_updateButtons(p);
    }
}

/* The damaged-pak notice (MAT3 playerscr.c controlPakDamaged): Start, A or B closes it. */
void controlPakDamaged(s32 p)
{
    if (gPlayers[p].edges & (BUTTON_ACCEPT | BUTTON_B)) {
        PlayStartABSound(gPlayers[p].edges);
        setPlayerState(p, kPlayerStateSelect);
    }
}

/* The full-pak notice (MAT3 playerscr.c controlPakFull): Start, A or B closes it. */
void controlPakFull(s32 p)
{
    if (gPlayers[p].edges & (BUTTON_ACCEPT | BUTTON_B)) {
        PlayStartABSound(gPlayers[p].edges);
        setPlayerState(p, kPlayerStateSelect);
    }
}
