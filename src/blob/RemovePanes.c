#include "game/game.h"

typedef struct PadConfig_s
{
  struct PadConfig_s *unk0;
  struct PadConfig_s *unk4;
  s16 unk8;
  s16 unkA;
  s16 unkC;
  s16 unkE;
  s16 unk10;
  s16 unk12;
  u8 unk14;
  u8 unk15;
  u8 unk16;
  u8 pad17;
  s16 unk18;
  s16 unk1A;
  s16 unk1C;
  s16 unk1E;
} PadConfig;

extern void RemoveBlit(intptr_t sound_id);

VIEW(s32, gPaneBlits);
#define gPaneBlits gPaneBlits_view

/* PC: a pane group (FiveParts, InitPane.c) keeps its five blits pointer-sized from +0x10; the m2c body
 * reads them as s32 at the N64 offsets (+0xC, 4 apart) and stops at the address after the 16 groups, gPasswordAlphabet,
 * which is a variable of its own on the PC. */
typedef struct { s8 active; u8 own; u8 rgba1[4]; u8 rgba2[4]; u8 other10[2]; void *part[5]; } PcFiveParts;
void RemovePanes(s32 arg0)
{
  PcFiveParts *group;
  s32 i;

  for (group = (PcFiveParts *) &gPaneBlits; group < (PcFiveParts *) &gPaneBlits + 16; group++)
  {
    if ((group->active != 0) && ((arg0 != 0) || (group->own == 0)))
    {
      for (i = 0; i < 5; i++)
      {
        if (group->part[i] != 0)
        {
          RemoveBlit((intptr_t) group->part[i]);
          group->part[i] = 0;
        }
      }
      group->active = 0;
    }
  }
}

