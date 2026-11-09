/* ChangePaneSize: move and size a pane (MAT3 pane.c). */
#include "game/game.h"

/* Lays the pane's five blits out over the rectangle (x1, y1)-(x2, y2) (either corner first): one-pixel edges along
 * the top, left, right and bottom, the inside one pixel in from them; then updates each blit. */
void ChangePaneSize(tBlitSet *pane,s32 x1,s32 y1,s32 x2,s32 y2)
{
    s32 i;
    if(x2<x1) { x1^=x2; x2^=x1; x1^=x2; }
    if(y2<y1) { y1^=y2; y2^=y1; y1^=y2; }
    for(i=0;i<PANE_BLITS;i++) {
        pane->blits[i]->x=x1;
        pane->blits[i]->y=y1;
        pane->blits[i]->width=x2-x1;
        pane->blits[i]->height=y2-y1;
    }
    pane->blits[0]->height=1;                                  /* the top edge */
    pane->blits[1]->width=1;                                   /* the left edge */
    pane->blits[1]->y++;
    pane->blits[1]->height-=2;
    pane->blits[2]->x++;                                       /* the inside */
    pane->blits[2]->y++;
    pane->blits[2]->width-=2;
    pane->blits[2]->height-=2;
    pane->blits[3]->x+=pane->blits[3]->width-1;                /* the right edge */
    pane->blits[3]->width=1;
    pane->blits[3]->y++;
    pane->blits[3]->height-=2;
    pane->blits[4]->y+=pane->blits[4]->height-1;               /* the bottom edge */
    pane->blits[4]->height=1;
    for(i=0;i<PANE_BLITS;i++) {
        UpdateBlit(pane->blits[i]);
    }
}
