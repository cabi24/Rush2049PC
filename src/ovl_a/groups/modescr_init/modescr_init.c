/* The mode menu's set-up (MAT3 modescr.c initScreen and initButtons). */
#include "game/game.h"

/* A 3D menu button (MAT3 tMenuButton without its alpha): its type (modescr_buttonNames), its scene object. */
typedef struct Slot64 {
    s32 type,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct Player76 { u8 unknown0[72]; void **profile; } Player76;
typedef struct Blit Blit;

VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view

VIEW(Slot64[13], modescr_buttonInfo);
#define modescr_buttonInfo modescr_buttonInfo_view
VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

extern Player76 gPlayers[];

extern void *memcpy(void *,const void *,u32);

extern void ZOID_SetTexture(s16,intptr_t,s32);
extern void *MBOX_FindTexture_WORK(char *,s16 *,s32,s8,s32);


/* PC: the buttons' colour modescr_button_color is R,G,B,A bytes in the ROM, copied whole into the scene nodes (read as bytes).
 * The PC loader fills initialized data by its declarations: as a word it would swap them (docs/port/ENDIAN.md). */
typedef struct Color4 { u8 c[4]; } Color4;
typedef struct MultiBlit {
    const char *texname;
    s16 dulx,duly,width,height,top,bot,left,right;
    u32 zdepth,alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;
VIEW(Color4, modescr_button_color);
#define modescr_button_color modescr_button_color_view

VIEW(MultiBlit, modescr_mblit);
#define modescr_mblit modescr_mblit_view

/* PC: ZOID_SetColor reads a colour as one word, so the colours passed to it are word-aligned, as IDO's
 * frame has them (docs/port/SANITIZE.md 3.2) */
#define PC_WORD_ALIGNED __attribute__((aligned(4)))
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);

/* modescr_buttonInfo[]: the seven rows' buttons, then these. */
#define MODE_CURSOR 7
#define MODE_A_ICON 8               /* the A button's icon and its arrow, by the "select" text */
#define MODE_A_ARROW 9
#define MODE_B_ICON 10              /* the B button's icon and its arrow, by the "back" text */
#define MODE_B_ARROW 11
#define MODE_TITLE 12
#define MODE_BUTTONS 13

/* Set the screen up: flush the profile saves, make the buttons and the text blit, drop the presses so far, and
 * select the mode the first player's profile last raced (GetPlayerRaceType; anything unknown selects a race). */
void modescr_initScreen(void)
{
    SaveFlush(1);
    createBG();
    modescr_initButtons();
    modescr_blit=NewMultiBlit(0,0,&modescr_mblit,1);
    exitModeScreen=0;
    ClearRushControls();
    gModeMenuSelect=GetPlayerRaceType((void **)gPlayers[0].profile);
    switch(gModeMenuSelect) {
    case 1: gModeMenuSelect=1; break;
    case 2: gModeMenuSelect=2; break;
    case 3: gModeMenuSelect=3; break;
    case 4: gModeMenuSelect=4; break;
    case 5: gModeMenuSelect=5; break;
    case 6: gModeMenuSelect=6; break;
    default: gModeMenuSelect=0; break;
    }
    modescr_inited=1;
}

/* Make the screen's 3D buttons that don't exist yet, each a model by its type (types 0 and 1 drawn see-through,
 * 0x42000), in the screen's button colour; then texture and place the cursor, the A and B icons with their arrows
 * and the title. */
void modescr_initButtons(void)
{
    Color4 color PC_WORD_ALIGNED;
    s32 i;
    s16 texture;
    Slot64 *slot;
    f32 (*matrix)[3];
    color=modescr_button_color;
    for(i=0;i<MODE_BUTTONS;i++) {
        slot=&modescr_buttonInfo[i];
        if(slot->id==-1) {
            matrix=slot->matrix;
            memcpy(matrix,unitmatrix,36);
            slot->id=MBOX_NewObject(MBOX_FindObject_WORK(modescr_buttonNames[slot->type],0,gNumModels-1,1),(uintptr_t)matrix,-1,(slot->type==0 || slot->type==1) ? 0x42000 : 0);
            ZOID_SetColor(slot->id,&color);
        }
    }
    ZOID_SetTexture(modescr_buttonInfo[MODE_CURSOR].id,(intptr_t)MBOX_FindTexture_WORK(modescr_cursor_texture_name,&texture,0,gNumModels-1,1),-1);
    ZOID_SetTexture(modescr_buttonInfo[MODE_A_ICON].id,(intptr_t)MBOX_FindTexture_WORK(modescr_a_button_texture_name,&texture,0,gNumModels-1,1),-1);
    memcpy(modescr_buttonInfo[MODE_A_ICON].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,modescr_buttonInfo[MODE_A_ICON].matrix);
    modescr_buttonInfo[MODE_A_ICON].position[0]=75.1f;
    modescr_buttonInfo[MODE_A_ICON].position[1]=55.2f;
    modescr_buttonInfo[MODE_A_ICON].position[2]=100.0f;
    ScaleUV(modescr_buttonInfo[MODE_A_ICON].matrix,modescr_buttonInfo[MODE_A_ICON].matrix,0.5f);
    memcpy(modescr_buttonInfo[MODE_A_ARROW].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,modescr_buttonInfo[MODE_A_ARROW].matrix);
    WRollUV(3.1415927f,modescr_buttonInfo[MODE_A_ARROW].matrix);
    modescr_buttonInfo[MODE_A_ARROW].position[0]=87.1f;
    modescr_buttonInfo[MODE_A_ARROW].position[1]=55.2f;
    modescr_buttonInfo[MODE_A_ARROW].position[2]=100.0f;
    ZOID_SetTexture(modescr_buttonInfo[MODE_B_ICON].id,(intptr_t)MBOX_FindTexture_WORK(modescr_b_button_texture_name,&texture,0,gNumModels-1,1),-1);
    memcpy(modescr_buttonInfo[MODE_B_ICON].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,modescr_buttonInfo[MODE_B_ICON].matrix);
    modescr_buttonInfo[MODE_B_ICON].position[0]=-75.1f;
    modescr_buttonInfo[MODE_B_ICON].position[1]=55.2f;
    modescr_buttonInfo[MODE_B_ICON].position[2]=100.0f;
    ScaleUV(modescr_buttonInfo[MODE_B_ICON].matrix,modescr_buttonInfo[MODE_B_ICON].matrix,0.5f);
    memcpy(modescr_buttonInfo[MODE_B_ARROW].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,modescr_buttonInfo[MODE_B_ARROW].matrix);
    modescr_buttonInfo[MODE_B_ARROW].position[0]=-87.1f;
    modescr_buttonInfo[MODE_B_ARROW].position[1]=55.2f;
    modescr_buttonInfo[MODE_B_ARROW].position[2]=100.0f;
    ZOID_SetTexture(modescr_buttonInfo[MODE_TITLE].id,(intptr_t)MBOX_FindTexture_WORK(modescr_title_texture_name,&texture,0,gNumModels-1,1),-1);
    memcpy(modescr_buttonInfo[MODE_TITLE].matrix,unitmatrix,36);
    WPitchUV(-1.5707964f,modescr_buttonInfo[MODE_TITLE].matrix);
    modescr_buttonInfo[MODE_TITLE].position[0]=0.0f;
    modescr_buttonInfo[MODE_TITLE].position[1]=115.0f;
    modescr_buttonInfo[MODE_TITLE].position[2]=200.0f;
    modescr_buttonsInited=1;
    modescr_buttonsJustInited=1;
    ZeroCameras();
}
