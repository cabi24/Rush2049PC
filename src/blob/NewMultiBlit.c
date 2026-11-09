#include "game/game.h"

typedef struct Blit Blit;
struct Blit {
    const char *Name;
    void *Image;
    void *Info;
    u16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide, Init;
    s16 Top, Bot, Left, Right, color;
    u16 reserved;
    s32 (*AnimFunc)(Blit *);
    u32 AnimID;
    intptr_t AnimDTA;
    u16 BLIdx;
    u16 reserved36;
    void *data;
    Blit *child;
};
typedef struct MultiBlit {
 const char *texname;
 s16 dulx,duly,width,height,top,bot,left,right;
 u32 zdepth,alpha;
 s32 (*animfunc)(Blit *);
 u32 animid;
} MultiBlit;
extern Blit *NewBlit();

extern void RemoveBlit(Blit *);
Blit *NewMultiBlit(s16 ulx, s16 uly, const MultiBlit *mblit, s16 nblits)
{
 Blit *rootblit,*lastblit,*curblit;
 int i;
 if(nblits<=0) return (Blit *)0;
 rootblit=(Blit *)0;
 lastblit=(Blit *)0;
 for(i=0;i<nblits;i++) {
  curblit=NewBlit(mblit[i].texname,ulx+mblit[i].dulx,uly+mblit[i].duly,mblit[i].animid & 0x80000000U);
  curblit->Alpha=mblit[i].alpha;
  curblit->Z=mblit[i].zdepth;
  curblit->Left=mblit[i].left;
  curblit->Top=mblit[i].top;
  curblit->Right=mblit[i].right;
  curblit->Bot=mblit[i].bot;
  if(mblit[i].width>=0) curblit->Width=mblit[i].width;
  if(mblit[i].height>=0) curblit->Height=mblit[i].height;
  UpdateBlit(curblit);
  if(mblit[i].animfunc) {
   curblit->AnimID=mblit[i].animid & 0x7fffffffU;
   if(mblit[i].texname==(char *)-1) {
    curblit->Info=(void *)mblit[i].animfunc;
    curblit->Image=curblit;
    UpdateBlit(curblit);
   } else {
    curblit->AnimFunc=mblit[i].animfunc;
    if(!curblit->AnimFunc(curblit)) {
     RemoveBlit(curblit);
     return rootblit;
    }
   }
  }
  if(i==0) rootblit=curblit;
  else lastblit->child=curblit;
  lastblit=curblit;
 }
 return rootblit;
}
