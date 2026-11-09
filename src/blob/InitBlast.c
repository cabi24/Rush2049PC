/* InitBlast.c: set the explosions up for a race (MAT3 visuals.c, InitBlast()).
 *
 * Takes the 50 matrices the blast sprites use in turn (gBlastMats, StartBlast) and marks every car's fireball and
 * shock ring as having no scene objects yet (pc_blasts, pc_blast_rings: groups/blast). */
#include "game/game.h"

/* tables of words, through a byte pointer: on the PC the blasts and their rings, pc_blasts and pc_blast_rings,
 * PC_MAX_LINKS long, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6), declared whole (OPTIMIZED.md
 * 2.7); InitBlasts sets up all PC_MAX_LINKS of them, on the N64 the 6 it has */
#define gBlastObjs (*(s32 *)PC_BLASTS)

#define gBlastRingObjs (*(s32 *)PC_BLAST_RINGS)

/* The parts of a car's fireball and ring this sets (MAT3 tBlastStruct and tBlastRingStruct; PC: struct pc_blast
 * and struct pc_blast_ring, 0x190 and 0x3C bytes): each scene object's index, -1 for none. */
typedef struct tBlastObj { s32 mbIdx; u8 pad04[0x54 - 4]; } tBlastObj;
typedef struct tBlastStruct { tBlastObj blastObjs[4]; s32 glowIdx; u8 pad154[0x190 - 0x154]; } tBlastStruct;
typedef struct tBlastRingStruct { s32 ringIdx; u8 pad04[0x3C - 4]; } tBlastRingStruct;

/* The 50 blast matrices ZOID_NewMatrix hands out are kept as pointers, 8 bytes an entry on the PC
 * (docs/port/CASTS.md). */
#define BLAST_MAT(p, i) (((void **) (p))[i])
/* pc_blasts[i].f and pc_blast_rings[i].f, s32 fields, written as the byte sums the compiled code has */
#define BLAST(i, f) (*((s32 *) (blasts + i * 0x190 + __builtin_offsetof(tBlastStruct, f))))
#define RING(i, f) (*((s32 *) (rings + (i * 0x3C) + __builtin_offsetof(tBlastRingStruct, f))))

/* Fill gBlastMats with 50 matrices from the scene's matrix pool, mark every car's fireball (its four flame balls
 * and its glow) and ring as having no scene object (-1), and start the round of matrices at the first. */
void InitBlast(void)
{
  s16 i;
  u8 *mats;
  u8 *rings;
  u8 *blasts;
  mats = (u8 *) (&gBlastMats);
  for (i = 0; i < 50; i++) { BLAST_MAT(mats, i) = ZOID_NewMatrix(); }
  rings = (u8 *) (&gBlastRingObjs);
  blasts = (u8 *) (&gBlastObjs);
  for (i = 0; i < PC_MAX_LINKS; i++) {
    RING(i, ringIdx) = -1;
    BLAST(i, glowIdx) = -1;
    BLAST(i, blastObjs[0].mbIdx) = -1;
    BLAST(i, blastObjs[1].mbIdx) = -1;
    BLAST(i, blastObjs[2].mbIdx) = -1;
    BLAST(i, blastObjs[3].mbIdx) = -1;
  }
  gBlastMatCnt = 0;
}
