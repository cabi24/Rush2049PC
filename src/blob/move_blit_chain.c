/* move_blit_chain: move a multiblit (the N64's own helper; MAT3 has no such function). */
#include "game/game.h"

/* Moves blit and every blit chained after it (child) by dx, dy pixels and updates each one's zoid blit list entry. */
void move_blit_chain(tBlit *blit,s16 dx,s16 dy) {
 if(blit) {
  blit->x+=dx;
  blit->y+=dy;
  UpdateBlit(blit);
  move_blit_chain(blit->child,dx,dy);
 }
}
