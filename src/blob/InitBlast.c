/* InitBlast.c: set the explosions up for a race (MAT3 visuals.c, InitBlast()).
 *
 * Takes the 50 matrices the blast sprites use in turn (gBlastMats, StartBlast) and marks every car's fireball and
 * shock ring as having no scene objects yet (pc_blasts, pc_blast_rings: groups/blast; PC_MAX_LINKS of each on the PC,
 * the N64's 6). */
#include "game/game.h"

/* Fill gBlastMats with 50 matrices from the scene's matrix pool, mark every car's fireball (its four flame balls
 * and its glow) and ring as having no scene object (-1), and start the round of matrices at the first. */
void InitBlast(void)
{
  s16 i;
  for (i = 0; i < 50; i++) { gBlastMats[i].mat = ZOID_NewMatrix(); }
  for (i = 0; i < PC_MAX_LINKS; i++) {
    pc_blast_rings[i].ringIdx = -1;
    pc_blasts[i].glowIdx = -1;
    pc_blasts[i].blastObjs[0].mbIdx = -1;
    pc_blasts[i].blastObjs[1].mbIdx = -1;
    pc_blasts[i].blastObjs[2].mbIdx = -1;
    pc_blasts[i].blastObjs[3].mbIdx = -1;
  }
  gBlastMatCnt = 0;
}
