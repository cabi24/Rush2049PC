/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Runtime image A: menu-state update and its three genuine IPA helpers.
 * New root updatePakScreen keeps the existing executable statements and ABI.
 * superseding revision: this is MAT3 pakscr.c. updatePakScreen calls
 * _ControlFileDelVerify / _ControlRumblePak / _ControlDamaged for states 2/3/4; they are now the non-kept helpers
 * ControlFileDelVerify / ControlRumblePak / ControlDamaged named after their retail deleted stubs, and _SetState
 * (SetState) calls _BuildFileDelList = BuildFileDelList. Post-order of updatePakScreen (last call first):
 * _BuildFileDelList 98B38, _SetState 98B40, _ControlDamaged 98BE0, _ControlRumblePak 98BE8, _FindFile 98BF0 (single),
 * _ControlFileDelVerify 98C34, _ControlFileDel 98C3C, _ControlPickPak 98E70, updatePakScreen 990D0 = retail layout.
 * The former four unused s32 frame slots are gone: the inlined calls' frame areas plus MAT3's local order
 * (`int i` first, the blit descriptor block-local in the init branch) give the retail frame 112 and homes. The
 * case bodies moved verbatim.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct Blit Blit;
typedef struct MultiBlit {
    const char *texname;
    s16 dulx,duly,width,height,top,bot,left,right;
    u32 zdepth,alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;
typedef struct PlayerPresence { s8 present; u8 unknown1[15]; } PlayerPresence;
extern s32 gState,gMaxMenuHeight,gPakErr;
extern s8 pakscr_gYes,pakscr_gInited,gDrawText,gLicenseShown;
extern s16 gPickedPak,pakscr_gFreePages,gDeleteSize,gDeleteSelect;
extern s16 gDeleteSelTop,gDeleteHeight,gDeleteSelPos;
extern void **gDeleteFile;
extern u32 gActiveAutoEdges,gActiveEdges,gActiveLevels,gGameState;
extern Blit *gBlit;
extern MultiBlit pakscr_mblit;
extern PlayerPresence gControllerData[];
extern u32 PS_GetFreeBlocks(s32);
extern s8 PS_PakInserted(s32),PS_PakDamaged(s32),PS_PakRumble(s32),PS_PakChanged(s32);
extern void **pakscr_FindFile(s32);
extern void PS_DeleteFile(void **);
extern u32 AudPlayEffect(u32,u32,u32,u8);
extern void SetPakError(s32,s32,s32,s32,s32,s32);
extern void PlayStartABSound(u32);
extern void PlayLRSound(u32);
extern void setGameState(s32);
extern void removeBG(void);
extern void RemoveBlit(Blit *);
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);
extern void particle_lifetime_set(void);
extern void SetControllerActive(s32,s32);
extern s32 CancelFunc(s32);
extern void ShowPakError(s32,s8 *,s8 *,s8 *,s32 (*)(s32));

void BuildFileDelList(void);
void SetState(s32 mode)
{
    switch(mode) {
    case 0: break;
    case 1:
        BuildFileDelList();
        break;
    case 2: pakscr_gYes=0; break;
    }
    gState=mode;
}

/* _BuildFileDelList (MAT3 pakscr.c): inlined into _SetState SetState; deleted stub 0x80398B38. */
void BuildFileDelList(void)
{
    gMaxMenuHeight=8;
    pakscr_gFreePages=PS_GetFreeBlocks(gPickedPak);
    gDeleteSize=16;
    if(gMaxMenuHeight<gDeleteSize)gDeleteHeight=gMaxMenuHeight;
    else gDeleteHeight=gDeleteSize;
}

void ControlFileDel(void)
{
    if(gActiveAutoEdges&0x400) {
        AudPlayEffect(40,0,1,0);
        if(--gDeleteSelect<0)gDeleteSelect=gDeleteSize-1;
        if(gDeleteHeight<gDeleteSize) {
            if(gDeleteSelPos==0) {
                if(--gDeleteSelTop<0)gDeleteSelTop=gDeleteSize-1;
            } else gDeleteSelPos--;
        }
    } else if(gActiveAutoEdges&0x800) {
        AudPlayEffect(39,0,1,0);
        if(++gDeleteSelect>=gDeleteSize)gDeleteSelect=0;
        if(gDeleteHeight<gDeleteSize) {
            if(gDeleteSelPos==gDeleteHeight-1) {
                if(++gDeleteSelTop>=gDeleteSize)gDeleteSelTop=0;
            } else gDeleteSelPos++;
        }
    }
    if(gActiveAutoEdges&0xC00)gDeleteFile=pakscr_FindFile(gDeleteSelect);
    if(((gActiveEdges&0x100)&&(gActiveLevels&0x80)) ||
       ((gActiveLevels&0x100)&&(gActiveEdges&0x80))) {
        AudPlayEffect(43,0,1,0);
        if(gDeleteFile)SetState(2);
    } else if(gActiveEdges&5) {
        AudPlayEffect(38,0,1,0);
        SetState(0);
    }
}

void ControlPickPak(void)
{
    if(gActiveEdges&0x400) {
        AudPlayEffect(40,0,1,0);
        if(--gPickedPak<0)gPickedPak=3;
    } else if(gActiveEdges&0x800) {
        AudPlayEffect(39,0,1,0);
        if(++gPickedPak>=4)gPickedPak=0;
    }
    if(gActiveEdges&2) {
        AudPlayEffect(37,0,1,0);
        if(!gControllerData[gPickedPak].present) {
            SetPakError(gPickedPak,1,10,50,300,140);
            gPakErr=1;
        } else if(!PS_PakInserted(gPickedPak)) {
            SetPakError(gPickedPak,2,10,50,300,140);
            gPakErr=2;
        } else if(PS_PakDamaged(gPickedPak))SetState(4);
        else if(PS_PakRumble(gPickedPak))SetState(3);
        else {
            gDeleteSelTop=gDeleteSelPos=gDeleteSelect=0;
            gDeleteFile=pakscr_FindFile(gDeleteSelect);
            SetState(1);
        }
    } else if(gActiveEdges&5) {
        PlayStartABSound(gActiveEdges);
        setGameState(gLicenseShown ? 0x40000000 : 2);
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

void ControlFileDelVerify(void);
void ControlRumblePak(void);
void ControlDamaged(void);
void updatePakScreen(void)
{
    s32 i;
    s8 complete;
    s8 result;
    s8 choice;
    if(!pakscr_gInited) {
        MultiBlit descriptor;
        descriptor=pakscr_mblit;
        SetState(0);
        if(gGameState&0x7C03FFFE)particle_lifetime_set();
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
                SetState(0);
                break;
            }
        }
        switch(gState) {
        case 0: ControlPickPak(); break;
        case 1: ControlFileDel(); break;
        case 2: ControlFileDelVerify(); break;
        case 3: ControlRumblePak(); break;
        case 4: ControlDamaged(); break;
        }
    }
}

/* _ControlFileDelVerify (MAT3 pakscr.c): inlined into updatePakScreen updatePakScreen; deleted stub 0x80398C34. */
void ControlFileDelVerify(void)
{
    if(gActiveEdges&0x3000) {
        PlayLRSound(gActiveEdges);
        pakscr_gYes=!pakscr_gYes;
    }
    if(gActiveEdges&2) {
        AudPlayEffect(37,0,1,0);
        if(pakscr_gYes) {
            PS_DeleteFile(gDeleteFile);
            pakscr_gFreePages=PS_GetFreeBlocks(gPickedPak);
            gDeleteFile=pakscr_FindFile(gDeleteSelect);
        }
        SetState(1);
    } else if(gActiveEdges&5) {
        PlayStartABSound(gActiveEdges);
        SetState(1);
    }
}

/* _ControlRumblePak (MAT3 pakscr.c): inlined into updatePakScreen updatePakScreen; deleted stub 0x80398BE8. */
void ControlRumblePak(void)
{
    if(gActiveEdges&7) {
        PlayStartABSound(gActiveEdges);
        SetState(0);
    }
}

/* _ControlDamaged (MAT3 pakscr.c): inlined into updatePakScreen updatePakScreen; deleted stub 0x80398BE0. */
void ControlDamaged(void)
{
    if(gActiveEdges&7) {
        PlayStartABSound(gActiveEdges);
        SetState(0);
    }
}

