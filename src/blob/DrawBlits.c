#include "game/game.h"

extern void RemoveBlit(intptr_t sound_id);

VIEW(intptr_t, BlitPtrs);
#define BlitPtrs BlitPtrs_view

/* PC: the Blit's host layout (NewMultiBlit.c, docs/port/POINTERS.md 9); the m2c body reads AnimFunc and Name as s32
 * at the N64 offsets (+0x28, +0) and keeps the blit in an s32. A blit whose AnimFunc returns 0 is deleted (DelBlit
 * swaps the last active one into its slot, which is looked at next). */
typedef struct PcBlit { const char *Name; void *Image; void *Info; u16 TexIndex; s16 X, Y; u16 Z; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide, Init; s16 Top, Bot, Left, Right, color; u16 reserved; s32 (*AnimFunc)(struct PcBlit *);
    u32 AnimID; intptr_t AnimDTA; u16 BLIdx, reserved36; void *data; struct PcBlit *child; } PcBlit;
void DrawBlits(void)
{
  PcBlit **p;
  PcBlit *blit;

  if (NumBlits > 0)
  {
    p = (PcBlit **) &BlitPtrs;
    do
    {
      blit = *p;
      if ((blit->AnimFunc != 0) && (blit->Name != (const char *) -1) && (blit->AnimFunc(blit) == 0))
      {
        RemoveBlit((intptr_t) blit);
        p -= 1;
      }
      p += 1;
    } while (p < &((PcBlit **) &BlitPtrs)[NumBlits]);
  }
}
