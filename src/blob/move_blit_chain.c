#include "game/game.h"

typedef struct Config64 {u8 pad0[14];s16 x,y;u8 pad18[42];struct Config64 *next;} Config64;
void move_blit_chain(Config64 *config,s16 dx,s16 dy) {
 if(config) {
  config->x+=dx;
  config->y+=dy;
  UpdateBlit(config);
  move_blit_chain(config->next,dx,dy);
 }
}
