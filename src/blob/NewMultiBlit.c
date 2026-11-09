/* NewMultiBlit: make a chain of blits from a table (MAT3 blit.c). */
#include "game/game.h"

extern tBlit *NewBlit();
extern void RemoveBlit(tBlit *);

/* Makes one blit for each of the nblits entries of mblit, at (ulx, uly) plus each entry's offset, with the entry's
 * alpha, depth, crop and (unless -1) size, and chains them through child: the first is returned and removing it removes
 * them all. An entry with an animation function: for a texture, the function becomes the blit's and is called at once,
 * and if it returns 0 that blit goes and the chain ends there (the blits made so far are returned); for
 * BLIT_NO_TEXTURE the function is the text blit's draw function, called by the renderer with the blit. Bit 31 of animid
 * is passed to NewBlit, the rest is the animation's animID. Returns NULL when nblits <= 0. */
tBlit *NewMultiBlit(s16 ulx, s16 uly, const tBlitDef *mblit, s16 nblits)
{
 tBlit *rootblit,*lastblit,*curblit;
 int i;
 if(nblits<=0) return (tBlit *)0;
 rootblit=(tBlit *)0;
 lastblit=(tBlit *)0;
 for(i=0;i<nblits;i++) {
  curblit=NewBlit(mblit[i].texname,ulx+mblit[i].dulx,uly+mblit[i].duly,mblit[i].animid & 0x80000000U);
  curblit->alpha=mblit[i].alpha;
  curblit->z=mblit[i].zdepth;
  curblit->left=mblit[i].left;
  curblit->top=mblit[i].top;
  curblit->right=mblit[i].right;
  curblit->bot=mblit[i].bot;
  if(mblit[i].width>=0) curblit->width=mblit[i].width;
  if(mblit[i].height>=0) curblit->height=mblit[i].height;
  UpdateBlit(curblit);
  if(mblit[i].animfunc) {
   curblit->animID=mblit[i].animid & 0x7fffffffU;
   if(mblit[i].texname==BLIT_NO_TEXTURE) {
    curblit->texture=(void *)mblit[i].animfunc;
    curblit->palette=curblit;
    UpdateBlit(curblit);
   } else {
    curblit->animFunc=mblit[i].animfunc;
    if(!curblit->animFunc(curblit)) {
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
