/* The controller-pak manager (MAT3 pakscr.c), GS_PAK, opened by holding Start at boot: pick a pak, then delete its
 * notes (files) to make room. N64 only in what it manages (MAT3's GameCube version manages memory cards). The
 * drawing is pakscr_draw.c. */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit {
    const char *texname;
    s16 dulx,duly,width,height,top,bot,left,right;
    u32 zdepth,alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;


#define PAK_ERR_NO_CONTROLLER 1     /* gPakErr: SetPakError's box for a missing controller */
#define PAK_ERR_NO_PAK 2            /* gPakErr: for a controller without a pak */
#define DELETE_COMBO_A 0x100        /* game buttons 8 and 7 together (one held, the other pressed) delete a note */
#define DELETE_COMBO_B 0x80

/* Go to state mode: the note list is rebuilt on entering it, the "are you sure" answer reset to no. */
void SetState(s32 mode)
{
    switch(mode) {
    case kStatePickPak: break;
    case kStateFileDel:
        BuildFileDelList();
        break;
    case kStateFileDelVerify: pakscr_gYes=0; break;
    }
    gState=mode;
}

/* The note list's size: the chosen pak's free pages, its 16 note slots, at most 8 rows shown. */
void BuildFileDelList(void)
{
    gMaxMenuHeight=8;
    pakscr_gFreePages=PS_GetFreeBlocks(gPickedPak);
    gDeleteSize=16;
    if(gMaxMenuHeight<gDeleteSize)gDeleteHeight=gMaxMenuHeight;
    else gDeleteHeight=gDeleteSize;
}

/* The note list's input: up and down (auto-repeating) move round the 16 slots, scrolling the 8 rows shown; the
 * delete combination asks to delete the selected note (if the slot has one); Start or B goes back to picking a
 * pak. */
void ControlFileDel(void)
{
    if(gActiveAutoEdges&BUTTON_UP) {
        AudPlayEffect(SND_MENU_UP,0,1,0);
        if(--gDeleteSelect<0)gDeleteSelect=gDeleteSize-1;
        if(gDeleteHeight<gDeleteSize) {
            if(gDeleteSelPos==0) {
                if(--gDeleteSelTop<0)gDeleteSelTop=gDeleteSize-1;
            } else gDeleteSelPos--;
        }
    } else if(gActiveAutoEdges&BUTTON_DOWN) {
        AudPlayEffect(SND_MENU_DOWN,0,1,0);
        if(++gDeleteSelect>=gDeleteSize)gDeleteSelect=0;
        if(gDeleteHeight<gDeleteSize) {
            if(gDeleteSelPos==gDeleteHeight-1) {
                if(++gDeleteSelTop>=gDeleteSize)gDeleteSelTop=0;
            } else gDeleteSelPos++;
        }
    }
    if(gActiveAutoEdges&(BUTTON_UP | BUTTON_DOWN))gDeleteFile=pakscr_FindFile(gDeleteSelect);
    if(((gActiveEdges&DELETE_COMBO_A)&&(gActiveLevels&DELETE_COMBO_B)) ||
       ((gActiveLevels&DELETE_COMBO_A)&&(gActiveEdges&DELETE_COMBO_B))) {
        AudPlayEffect(SND_MENU_DELETE,0,1,0);
        if(gDeleteFile)SetState(kStateFileDelVerify);
    } else if(gActiveEdges&(BUTTON_START | BUTTON_B)) {
        AudPlayEffect(SND_MENU_BACK,0,1,0);
        SetState(kStatePickPak);
    }
}

/* Picking a pak: up and down move round the four controllers; A opens the chosen pak (an error box for a missing
 * controller or pak, a notice for a damaged or Rumble Pak, else its note list); Start or B leaves the manager, to the
 * licence screen at boot (the warning screen if the licence was already seen). */
void ControlPickPak(void)
{
    if(gActiveEdges&BUTTON_UP) {
        AudPlayEffect(SND_MENU_UP,0,1,0);
        if(--gPickedPak<0)gPickedPak=3;
    } else if(gActiveEdges&BUTTON_DOWN) {
        AudPlayEffect(SND_MENU_DOWN,0,1,0);
        if(++gPickedPak>=4)gPickedPak=0;
    }
    if(gActiveEdges&BUTTON_A) {
        AudPlayEffect(SND_MENU_A,0,1,0);
        if(!gControllerData[gPickedPak].attached) {
            SetPakError(gPickedPak,PAK_ERR_NO_CONTROLLER,10,50,300,140);
            gPakErr=PAK_ERR_NO_CONTROLLER;
        } else if(!PS_PakInserted(gPickedPak)) {
            SetPakError(gPickedPak,PAK_ERR_NO_PAK,10,50,300,140);
            gPakErr=PAK_ERR_NO_PAK;
        } else if(PS_PakDamaged(gPickedPak))SetState(kStateDamaged);
        else if(PS_PakRumble(gPickedPak))SetState(kStateRumblePak);
        else {
            gDeleteSelTop=gDeleteSelPos=gDeleteSelect=0;
            gDeleteFile=pakscr_FindFile(gDeleteSelect);
            SetState(kStateFileDel);
        }
    } else if(gActiveEdges&(BUTTON_START | BUTTON_B)) {
        PlayStartABSound(gActiveEdges);
        setGameState(gLicenseShown ? GS_WARNING : GS_LICENSE);
        if(pakscr_gInited) {
            pakscr_gInited=0;
            if(gBlit) {
                RemoveBlit(gBlit);
                gBlit=0;
            }
            removeBG();
        }
    }
}

/* The screen's frame. The first frame selects the first pak, makes the text blit and turns every controller on.
 * While an error box is up it runs (ShowPakError, closed by CancelFunc) and the screen's text is hidden; else a
 * changed pak sends the screen back to picking a pak, and the state's input runs. */
void updatePakScreen(void)
{
    s32 i;
    s8 complete;
    s8 result;
    s8 choice;
    if(!pakscr_gInited) {
        HudBlitDef descriptor;
        descriptor=pakscr_mblit;
        SetState(kStatePickPak);
        if(gGameState&GS_FRONTEND_MASK)createBG();
        gBlit=NewMultiBlit(0,0,&descriptor,1);
        SetControllerActive(-1,1);
        pakscr_gInited=1;
    }
    if(gPakErr) {
        ShowPakError(gPickedPak,&result,&choice,&complete,CancelFunc);
        if(complete)gPakErr=0;
        gDrawText=0;
    } else {
        gDrawText=1;
        for(i=0;i<4;i++) {
            if(PS_PakChanged(i)) {
                SetState(kStatePickPak);
                break;
            }
        }
        switch(gState) {
        case kStatePickPak: ControlPickPak(); break;
        case kStateFileDel: ControlFileDel(); break;
        case kStateFileDelVerify: ControlFileDelVerify(); break;
        case kStateRumblePak: ControlRumblePak(); break;
        case kStateDamaged: ControlDamaged(); break;
        }
    }
}

/* "Delete this note?": left and right flip yes and no, A answers (yes deletes it and refreshes the list), Start or B
 * says no; either way back to the list. */
void ControlFileDelVerify(void)
{
    if(gActiveEdges&(BUTTON_LEFT | BUTTON_RIGHT)) {
        PlayLRSound(gActiveEdges);
        pakscr_gYes=!pakscr_gYes;
    }
    if(gActiveEdges&BUTTON_A) {
        AudPlayEffect(SND_MENU_A,0,1,0);
        if(pakscr_gYes) {
            PS_DeleteFile(gDeleteFile);
            pakscr_gFreePages=PS_GetFreeBlocks(gPickedPak);
            gDeleteFile=pakscr_FindFile(gDeleteSelect);
        }
        SetState(kStateFileDel);
    } else if(gActiveEdges&(BUTTON_START | BUTTON_B)) {
        PlayStartABSound(gActiveEdges);
        SetState(kStateFileDel);
    }
}

/* The Rumble Pak notice: Start, A or B goes back to picking a pak. */
void ControlRumblePak(void)
{
    if(gActiveEdges&(BUTTON_ACCEPT | BUTTON_B)) {
        PlayStartABSound(gActiveEdges);
        SetState(kStatePickPak);
    }
}

/* The damaged pak notice: Start, A or B goes back to picking a pak. */
void ControlDamaged(void)
{
    if(gActiveEdges&(BUTTON_ACCEPT | BUTTON_B)) {
        PlayStartABSound(gActiveEdges);
        SetState(kStatePickPak);
    }
}
