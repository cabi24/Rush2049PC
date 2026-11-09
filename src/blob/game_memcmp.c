/* game_memcmp: the game's memcmp (the N64's own C library routines; MAT3 uses the platform's). */
#include "game/game.h"

/* Compares count bytes of memory and right: 0 when they are the same, else the difference of the first two bytes
 * that differ (unsigned). */
s32 game_memcmp(void *memory,u8 *right,s32 count) {
 if(count!=0) {
  u8 *left=memory;
  do {
   if(*left++!=*right++)return *--left-*--right;
  }while(--count!=0);
 }
 return 0;
}
