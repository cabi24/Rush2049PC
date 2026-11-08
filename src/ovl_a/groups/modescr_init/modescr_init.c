/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: modescr_initScreen = initScreen@modescr.c calls initButtons (MAT3 modescr.c) as the
 * non-kept helper modescr_initButtons, defined after its caller (MAT3 order); its deleted procedure is the retail stub
 * 0x803998B8 right before initScreen. The button set-up moved verbatim except that MAT3's nested call shape
 * `MBOX_NewObject(MBOX_FindObject_WORK(...), ..., type <= 1 ? 0x42000 : 0)` replaces the native `object`/`flags`
 * carriers: those two locals were standing in for umerge's 8-byte reservation for the inlined call (frame 80). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Resource { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 player; } Resource;
typedef struct Owner { void *unknown0,*unknown4; Resource **resource; } Owner;   /* the profile, pc_prelude.h */
typedef struct Player76 { u8 unknown0[72]; Owner **owner; } Player76;
typedef struct Blit Blit;
extern s16 gNumPlayers;
extern s8 modescr_validOption_biased[][7],modescr_inited,gSoftReset;
extern s32 modescr_buttonsInited,modescr_buttonsJustInited,gModeMenuSelect,exitModeScreen,gRace;
extern u32 gSkipToGameState,gGameState,gActiveEdges;
extern f32 modescr_gButtonScale,modescr_gButtonSpace,modescr_buttonX,modescr_maxY,modescr_buttonZ,modescr_cursorZ;
extern f32 modescr_cursorSpeed,unitmatrix[3][3];
extern f32 gFrameSeconds;
extern Slot64 modescr_buttonInfo[13];
extern u8 gNumModels;
extern char modescr_select_texture_name[],modescr_button_texture_name[];
extern Player76 gPlayers[];
extern Blit *modescr_blit;
extern s32 IsTrackUnlocked(s32);
extern s8 PS_PakChanged(s32);
extern void *memcpy(void *,const void *,u32);
extern void CopyUV(void *,void *);
extern void WPitchUV(f32,f32 (*)[3]);
extern void WRollUV(f32,f32 (*)[3]);
extern void ScaleUV(f32 (*)[3],f32 (*)[3],f32);
extern void ZOID_SetTexture(s16,intptr_t,s32);
extern void *MBOX_FindTexture_WORK(char *,s16 *,s32,s8,s32);
extern void MBOX_HideObject(s32,s32,u32),MBOX_ShowObject(s32,s32,u32);
extern void setGameState(s32),modescr_initScreen(void);
extern void PlayStartABSound(u32),PlayUDSound(u32);
extern void ZOID_RemoveObject(s16),RemoveBlit(Blit *),removeBG(void);
extern f32 fabsf(f32);
#pragma intrinsic (fabsf)

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
extern Color4 modescr_button_color;
extern char *modescr_buttonNames[];
extern char modescr_cursor_texture_name[],modescr_a_button_texture_name[],modescr_b_button_texture_name[],modescr_title_texture_name[];
extern MultiBlit modescr_mblit;
extern void SaveFlush(s32),particle_lifetime_set(void);
extern void ZeroCameras(void),ClearRushControls(void);
extern s32 MBOX_FindObject_WORK(char *,s8,s8,s8);
extern s32 MBOX_NewObject(u32,uintptr_t,s32,u32);
extern void ZOID_SetColor(s16,Color4 *);
/* PC: ZOID_SetColor reads a colour as one word, so the colours passed to it are word-aligned, as IDO's
 * frame has them (docs/port/SANITIZE.md 3.2) */
#define PC_WORD_ALIGNED __attribute__((aligned(4)))
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);
extern u8 GetPlayerRaceType(void **);

void modescr_initButtons(void);
void modescr_initScreen(void)
{
    SaveFlush(1);
    particle_lifetime_set();
    modescr_initButtons();
    modescr_blit=NewMultiBlit(0,0,&modescr_mblit,1);
    exitModeScreen=0;
    ClearRushControls();
    gModeMenuSelect=GetPlayerRaceType((void **)gPlayers[0].owner);
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

/* initButtons@modescr.c (MAT3 GC initButtons): deleted stub 0x803998B8 */
void modescr_initButtons(void)
{
    Color4 color PC_WORD_ALIGNED;
    s32 i;
    s16 texture;
    Slot64 *slot;
    f32 (*matrix)[3];
    color=modescr_button_color;
    for(i=0;i<13;i++) {
        slot=&modescr_buttonInfo[i];
        if(slot->id==-1) {
            matrix=slot->matrix;
            memcpy(matrix,unitmatrix,36);
            slot->id=MBOX_NewObject(MBOX_FindObject_WORK(modescr_buttonNames[slot->unknown0],0,gNumModels-1,1),(uintptr_t)matrix,-1,(slot->unknown0==0 || slot->unknown0==1) ? 0x42000 : 0);
            ZOID_SetColor(slot->id,&color);
        }
    }
    ZOID_SetTexture(modescr_buttonInfo[7].id,(intptr_t)MBOX_FindTexture_WORK(modescr_cursor_texture_name,&texture,0,gNumModels-1,1),-1);
    ZOID_SetTexture(modescr_buttonInfo[8].id,(intptr_t)MBOX_FindTexture_WORK(modescr_a_button_texture_name,&texture,0,gNumModels-1,1),-1);
    memcpy(modescr_buttonInfo[8].matrix,unitmatrix,36);
    WPitchUV(-1.5707963705062866f,modescr_buttonInfo[8].matrix);
    modescr_buttonInfo[8].position[0]=75.0999984741211f;
    modescr_buttonInfo[8].position[1]=55.20000076293945f;
    modescr_buttonInfo[8].position[2]=100.0f;
    ScaleUV(modescr_buttonInfo[8].matrix,modescr_buttonInfo[8].matrix,0.5f);
    memcpy(modescr_buttonInfo[9].matrix,unitmatrix,36);
    WPitchUV(-1.5707963705062866f,modescr_buttonInfo[9].matrix);
    WRollUV(3.1415927410125732f,modescr_buttonInfo[9].matrix);
    modescr_buttonInfo[9].position[0]=87.0999984741211f;
    modescr_buttonInfo[9].position[1]=55.20000076293945f;
    modescr_buttonInfo[9].position[2]=100.0f;
    ZOID_SetTexture(modescr_buttonInfo[10].id,(intptr_t)MBOX_FindTexture_WORK(modescr_b_button_texture_name,&texture,0,gNumModels-1,1),-1);
    memcpy(modescr_buttonInfo[10].matrix,unitmatrix,36);
    WPitchUV(-1.5707963705062866f,modescr_buttonInfo[10].matrix);
    modescr_buttonInfo[10].position[0]=-75.0999984741211f;
    modescr_buttonInfo[10].position[1]=55.20000076293945f;
    modescr_buttonInfo[10].position[2]=100.0f;
    ScaleUV(modescr_buttonInfo[10].matrix,modescr_buttonInfo[10].matrix,0.5f);
    memcpy(modescr_buttonInfo[11].matrix,unitmatrix,36);
    WPitchUV(-1.5707963705062866f,modescr_buttonInfo[11].matrix);
    modescr_buttonInfo[11].position[0]=-87.0999984741211f;
    modescr_buttonInfo[11].position[1]=55.20000076293945f;
    modescr_buttonInfo[11].position[2]=100.0f;
    ZOID_SetTexture(modescr_buttonInfo[12].id,(intptr_t)MBOX_FindTexture_WORK(modescr_title_texture_name,&texture,0,gNumModels-1,1),-1);
    memcpy(modescr_buttonInfo[12].matrix,unitmatrix,36);
    WPitchUV(-1.5707963705062866f,modescr_buttonInfo[12].matrix);
    modescr_buttonInfo[12].position[0]=0.0f;
    modescr_buttonInfo[12].position[1]=115.0f;
    modescr_buttonInfo[12].position[2]=200.0f;
    modescr_buttonsInited=1;
    modescr_buttonsJustInited=1;
    ZeroCameras();
}
