#include "game/game.h"

typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID;
} Blit;
typedef union MenuSlot {
    float matrix[16];
    struct { float x,y,depth; u8 rest[48]; u8 alpha; u8 tail[3]; } state;
} MenuSlot;
typedef struct ProjectContext {
    float rotation[3][3], position[3]; u8 opaque[104];
} ProjectContext;
VIEW(MenuSlot[], trackscr_buttonInfo);
#define trackscr_buttonInfo trackscr_buttonInfo_view
VIEW(ProjectContext[], gCamera);
#define gCamera gCamera_view

s32 trackscr_animNewSlider(Blit *blt)
{
    s32 segment=blt->AnimID&15;
    s32 marker=blt->AnimID>>16;
    s16 position[2];


    if (Hidden(blt,
        (0.6981316805f<trackscr_buttonInfo[marker+21].state.depth && trackscr_buttonInfo[marker+21].state.depth<2.443460941f)
        || !trackscr_buttonInfo[marker+21].state.alpha))
        return 1;
    ZOID_ProjectWindow(0,&trackscr_buttonInfo[marker+21].matrix[12],&gCamera[0],0,position);
    blt->X=position[0]-blt->Width/2;
    blt->Y=position[1]-(blt->Height/4)/2;
    blt->Top=0;
    blt->Bot=blt->Height/4-1;
    blt->Left=0;
    blt->Right=blt->Width-1;
    blt->Alpha=trackscr_buttonInfo[marker+21].state.alpha;
    if (blt->Alpha==255)
        blt->Alpha=254;
    if (1.570796371f<trackscr_buttonInfo[marker+21].state.depth) {
        blt->Top+=blt->Height/2;
        blt->Bot+=blt->Height/2;
    }
    if (segment==1) {
        blt->Top+=blt->Height/4;
        blt->Bot+=blt->Height/4;
        switch (marker) {
        case 14: blt->Right=(blt->Width-1)*gFogIntensity/3; break;
        case 15: blt->Right=(blt->Width-1)*gWind/4; break;
        case 18: blt->Right=(blt->Width-1)*gDifficulty/5; break;
        case 19: blt->Right=(blt->Width-1)*gCatchup/2; break;
        }
    }
    UpdateBlit(blt);
    return 1;
}
