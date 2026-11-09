#include "game/game.h"

typedef struct PadConfig {
    void *data; intptr_t word4,word8; u16 half12; s16 x,y; u16 half18;
    s16 width,height; u8 status,flag; s8 disabled; u8 other27;
    s16 left,bottom,right,top; u8 other36[4]; intptr_t active; u32 packed;
} PadConfig;
typedef struct FiveParts { u8 other[12]; PadConfig *part[5]; } FiveParts;

void ChangePaneSize(FiveParts *group,s32 x1,s32 y1,s32 x2,s32 y2)
{
    s32 i;
    if(x2<x1) { x1^=x2; x2^=x1; x1^=x2; }
    if(y2<y1) { y1^=y2; y2^=y1; y1^=y2; }
    for(i=0;i<5;i++) {
        group->part[i]->x=x1;
        group->part[i]->y=y1;
        group->part[i]->width=x2-x1;
        group->part[i]->height=y2-y1;
    }
    group->part[0]->height=1;
    group->part[1]->width=1;
    group->part[1]->y++;
    group->part[1]->height-=2;
    group->part[2]->x++;
    group->part[2]->y++;
    group->part[2]->width-=2;
    group->part[2]->height-=2;
    group->part[3]->x+=group->part[3]->width-1;
    group->part[3]->width=1;
    group->part[3]->y++;
    group->part[3]->height-=2;
    group->part[4]->y+=group->part[4]->height-1;
    group->part[4]->height=1;
    for(i=0;i<5;i++) {
        UpdateBlit(group->part[i]);
    }
}
