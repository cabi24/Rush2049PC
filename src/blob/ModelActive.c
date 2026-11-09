#include "game/game.h"

typedef struct ModelRecord 
{
  u8 pad000[0x7C6];
  u16 unk7C6;
  u8 pad7C8[0x7E8 - 0x7C8];
  u8 unk7E8;
} ModelRecord;
extern ModelRecord model;

VIEW(volatile unsigned char, gWaveInstance);
#define gWaveInstance gWaveInstance_view

/* PC: gWaveInstance has its host layout, 0x20 bytes a slot with ptr at +0x10 (every other view declares the
 * pointers); the N64 code addresses it by its N64 offsets (0x14 a slot, ptr at +0xC). Returns the model flag of
 * a slot that holds a ready wave. */
typedef struct { s8 loaded, ready, locked, model; u8 mirrored; s8 handle; u8 wave; u32 size; u8 *ptr; u8 *lastPtr; } WaveSlot;
s8 ModelActive(s32 arg0)
{
  WaveSlot *slot = (WaveSlot *) &gWaveInstance + arg0;
  if (slot->ptr == 0)
  {
    return 0;
  }
  if (slot->ready == 0)
  {
    return 0;
  }
  return slot->model;
}
