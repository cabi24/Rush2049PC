#include "game/game.h"

typedef struct Blit {
    const char *Name;
    void *image;
    void *Info;
    s16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right;
    s16 color;
    u16 unknown26;
    s32 (*AnimFunc)(struct Blit *);
    u32 AnimID;
} Blit;

s32 animTach(Blit *blt)
{
    s32 slot, kind, hide;
    f32 amount;
    slot=(blt->AnimID & 0xf0)>>4;
    kind=blt->AnimID & 0xf;
    if(slot>=gNumWindows) {
        if(slot>=gNumPlayers) blt->AnimFunc=0;
        if (blt->Hide != 1) {
            blt->Hide = 1;
            UpdateBlit(blt);
        }
        return 1;
    }
    hide = gShowTach == 0 || PC_MODEL_AUTOTRANS(slot) != 0 ||
           PC_CAR_FINISHED(PC_MODEL_SLOT(slot)) == 1;
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    if (blt->Hide) return 1;
    if(gNumWindows==2) RenameBlit(blt,tachometer_md_name,0);
    else if(gNumWindows>=3) RenameBlit(blt,tachometer_sm_name,0);
    blt->X=tachPos[gNumWindows-1][slot][0]-blt->Width/2;
    blt->Y=tachPos[gNumWindows-1][slot][1];
    if(kind==0) blt->Bot=blt->Height/2-1;
    else if(kind==1) {
        amount=(PC_MODEL_S7D0(slot)*1.35f)*0.0001f;
        if(amount<0.0f) amount=-amount;
        if(amount>1.0f) amount=1.0f;
        blt->Right=(s32)((blt->Width-1)*amount);
        blt->Top=blt->Height/2;
        blt->Bot=blt->Height-1;
    }
    blt->Alpha=254;
    UpdateBlit(blt);
    return 1;
}
