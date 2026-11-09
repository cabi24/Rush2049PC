#include "game/game.h"

typedef struct InputRecord 
{
  u8 pad00[1];
  u8 active;
  u8 pad02[2];
  s32 unk04;
  s32 unk08;
  s32 unk0C;
  f32 unk10;
  f32 unk14;
  u8 _pad18[0x48 - 0x18]; void *ref; u8 _pad18_4C[0x4D - 0x4C];
  u8 unk4D;
} InputRecord;
extern InputRecord gPlayers;

VIEW(s32[4], gContLevels);
#define gContLevels gContLevels_view
VIEW(s32[4], gContEdges);
#define gContEdges gContEdges_view
VIEW(s32[4], gAutoEdges);
#define gAutoEdges gAutoEdges_view
typedef struct 
{
  f32 unk0;
  f32 unk4;
} ContStick;
VIEW(ContStick[4], gContStick);
#define gContStick gContStick_view

/* PC: an input record (gPlayers) is 80 bytes, its pointer at +0x48 is 8 (docs/port/POINTERS.md 10). */
#define INPUT_SIZE 80
void FindPlayerNum(u32 arg0, s32 arg1)
{
  InputRecord *r;
  s32 *t;
  ContStick *e;
  if (arg1 != 0) {
    r = (InputRecord *) ((u8 *) &gPlayers + arg0 * INPUT_SIZE);
    t = &gContEdges[arg0];
  } else {
    r = (InputRecord *) ((u8 *) &gPlayers + arg0 * INPUT_SIZE);
    if (r->active != 5) return;
    t = &gContEdges[arg0];
    if (!(*t & 1)) return;
  }
  if(arg0) {}
  r->active = arg0;
  r->unk08 = gContLevels[arg0];
  r->unk04 = *t;
  r->unk0C = gAutoEdges[arg0];
  e = &gContStick[arg0];
  r->unk10 = e->unk0;
  r->unk14 = e->unk4;
  SetControllerActive(arg0 & 0xFF, 1);
}
