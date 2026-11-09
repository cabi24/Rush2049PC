/* InitPane: make a pane, a bordered rectangle of five blits (MAT3 pane.c). */
#include "game/game.h"


/* Makes a pane over the rectangle (x1, y1)-(x2, y2) (either corner first) from the first free one of gPaneBlits: a
 * dark blue inside (0, 0, 16) and a light blue border (0, 0, 192), both of alpha; each of its five blits gets the
 * animation function animFunc with animID (left uncalled here: the caller's DrawBlits calls it), the first one also
 * the pane itself as animDTA. permanent panes outlive RemovePanes(0). Returns the pane. (MAT3 also keeps the pane's
 * blits out of the "draw first" pass, ZOID_IgnoreDrawFirstBlit; the N64 doesn't.) */
tBlitSet *InitPane(s32 x1,s32 y1,s32 x2,s32 y2,s32 alpha,s32 (*animFunc)(tBlit *),u32 animID,s32 permanent)
{
    tBlitSet *pane=gPaneBlits;
    s32 i;
    for(i=0;i<MAX_PANES;i++,pane++) {
        if(!pane->used) break;
    }
    /* Original game: with all 16 panes in use this returns &gPaneBlits[16], one past the array (the loop has
     * stepped past the last one), not NULL; the callers then write through it (MAT3 pane.c does the same).
     * Kept: it is the retail behaviour. */
    if(i>=MAX_PANES) return pane;
    if(x2<x1) { x1^=x2; x2^=x1; x1^=x2; }
    if(y2<y1) { y1^=y2; y2^=y1; y1^=y2; }
    pane->blackColor[0]=0;
    pane->blackColor[1]=0;
    pane->blackColor[2]=16;
    pane->blackColor[3]=alpha;
    pane->grayColor[0]=0;
    pane->grayColor[1]=0;
    pane->grayColor[2]=192;
    pane->grayColor[3]=alpha;
    pane->perm=permanent;
    pane->used=1;
    for(i=0;i<PANE_BLITS;i++) {
        pane->blits[i]=NewBlit((void *)0,x1,y1,0);
        pane->blits[i]->alpha=alpha;
        pane->blits[i]->palette=pane->blackColor;
        pane->blits[i]->animFunc=animFunc;
        pane->blits[i]->animID=animID;
        pane->blits[i]->animDTA=0;
    }
    pane->blits[0]->animDTA=(intptr_t)pane;
    pane->blits[0]->palette=pane->grayColor;
    pane->blits[1]->palette=pane->grayColor;
    pane->blits[3]->palette=pane->grayColor;
    pane->blits[4]->palette=pane->grayColor;
    ChangePaneSize(pane,x1,y1,x2,y2);
    return pane;
}
