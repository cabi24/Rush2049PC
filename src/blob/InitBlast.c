#include "game/game.h"

VIEW(void *[50], gBlastMats);
#define gBlastMats gBlastMats_view

/* tables of words, through a byte pointer: on the PC the blasts and their rings, pc_blasts and pc_blast_rings,
 * PC_MAX_LINKS long, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6), declared whole (OPTIMIZED.md
 * 2.7); InitBlasts sets up all PC_MAX_LINKS of them, on the N64 the 6 it has */
#define gBlastObjs (*(s32 *)PC_BLASTS)

#define gBlastRingObjs (*(s32 *)PC_BLAST_RINGS)

/* The 50 blast matrices ZOID_NewMatrix hands out are kept as pointers, 8 bytes an entry on the PC
 * (docs/port/CASTS.md). */
#define BLAST_MAT(p, i) (((void **) (p))[i])
#define BLAST_MAT_PTR
void InitBlast(void)
{
  s16 i;
  u8 *p;
  u8 *p2;
  u8 *p3;
  p = (u8 *) (&gBlastMats);
  for (i = 0; i < 50; i++) { BLAST_MAT(p, i) = BLAST_MAT_PTR ZOID_NewMatrix(); }
  p2 = (u8 *) (&gBlastRingObjs);
  p3 = (u8 *) (&gBlastObjs);
  for (i = 0; i < PC_MAX_LINKS; i++) {
    *((s32 *) (p2 + (i * 0x3C))) = -1;
    *((s32 *) (p3 + i * 0x190 + 0x150)) = -1;
    *((s32 *) (p3 + i * 0x190)) = -1;
    *((s32 *) (p3 + i * 0x190 + 0x54)) = -1;
    *((s32 *) (p3 + i * 0x190 + 0xA8)) = -1;
    *((s32 *) (p3 + i * 0x190 + 0xFC)) = -1;
  }
  gBlastMatCnt = 0;
}
