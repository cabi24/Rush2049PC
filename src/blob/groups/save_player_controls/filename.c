#include "game/game.h"

typedef struct Y { u8 pad[0x38]; s32 f38; u8 name[11]; } Y;
typedef struct X { void *next, *prev; struct C96Handle *f8; u8 p1[0x15]; u8 name[11]; Y **f2C; } X;   /* the profile, pc_prelude.h */

extern void *memcpy(void *, void *, unsigned);

void PS_WriteFile(struct C96Handle *, u8 *, u32);
void SavePlayerControls(X **arg0, u8 *arg1)
{
Y *y;
 if ((*arg0)->f2C != 0) {
 y = *(*arg0)->f2C;
 if (game_memcmp(y->name, arg1, 11) != 0) {
 memcpy((*arg0)->name, arg1, 11);
 memcpy(y->name, arg1, 11);
 y->f38 = CalcChecksumInternal(y->name, 11);
 PS_WriteFile((*arg0)->f8, (u8 *) &y->f38, 15);
 }
 }
}
