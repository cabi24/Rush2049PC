#include "game/game.h"

typedef struct SoundState
{
  u8 _pad0[0x4];
  struct SoundState *unk4;
  s32 (*unk8)(void *);
  u8 _padC[0x12 - 0xC];
  s16 unk12;
  s16 unk14;
  s16 unk16;
  s8 unk18;
  u8 _pad19[0x1C - 0x19];
  s16 unk1C;
  s16 unk1E;
  s16 unk20;
  s16 unk22;
  u8 _pad24[0x28 - 0x24];
  s32 (*unk28)(void *);
  s32 unk2C;
  u8 _pad30[0x3C - 0x30];
  struct SoundState *unk3C;
} SoundState;

extern void RemoveBlit(intptr_t sound_id);

VIEW(intptr_t, backBlit);
#define backBlit backBlit_view
VIEW(intptr_t, backPolys);
#define backPolys backPolys_view
VIEW(intptr_t, bg_texture);

void ZOID_RemovePoly(s16 *arg0);

#define bg_texture ((&backPolys)[4])
void removeBG(void)
{
  intptr_t *var_s0;
  intptr_t temp_a0;
  if (backBlit != 0)
  {
    RemoveBlit(backBlit);
    backBlit = 0;
  }
 do { var_s0 = &backPolys; do { temp_a0 = *var_s0; if (temp_a0 != 0) { ZOID_RemovePoly((s16 *) temp_a0); *var_s0 = 0; } var_s0 += 1; } while (var_s0 != (&bg_texture)); } while (0);
}

