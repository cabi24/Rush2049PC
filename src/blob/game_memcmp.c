#include "game/game.h"

s32 game_memcmp(void *memory,u8 *right,s32 count) {
 if(count!=0) {
  u8 *left=memory;
  do {
   if(*left++!=*right++)return *--left-*--right;
  }while(--count!=0);
 }
 return 0;
}
