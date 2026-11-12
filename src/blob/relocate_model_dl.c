/* relocate_model_dl: a model's display list set up for the race once its WAD is loaded (N64 only: MAT3's GameCube
 * models have no display lists). Called by SetupWaveModel for each level of detail. On the N64 it relocated the list
 * too: it added the file's address to every address in it, in place. On the PC the WAD's converter built the list
 * with host pointers when the file loaded (port/assets/wad.c, docs/port/NATIVE.md step 10), so what is left is the
 * race's: the sections it keeps and the mirrored texture coordinates. */
#include "game/game.h"

/* A LOD marker: a G_SPNOOP whose byte 1 is LOD_MARKER, its low half the bit of the race's keep mask its section needs
 * (waves.c's DL_KEEP_*), its w1 the list where the section ends (a pointer: the converter's). */
#define LOD_MARKER 1
#define G_DL_BRANCH_WORD 0xDE010000u   /* a G_DL that doesn't come back */

/* Walks the display list dl a command at a time to its G_ENDDL: past a G_DL (not into it), and at a G_BRANCH_Z on at
 * the second command of the list its G_RDPHALF_1 names. A LOD marker whose bit isn't in keep becomes a branch to the
 * list it names, so the race skips its section. The first vertex load's vertices are prepared for mirroring
 * (mirror_vertex_texcoords; mirrored: the track is mirrored).
 * On the N64 the marker's bit was 1 << (its low half), which the CPU took modulo 32: so is it here. A marker or a
 * G_RDPHALF_1 that names no list (where the N64's word named no place in the file: the converter said so) stays a
 * no-op, or ends the walk, where the N64 branched into whatever lay there. */
void relocate_model_dl(Gfx *dl, s32 mirrored, u32 keep) {
    Gfx *cmd;
    Gfx *latched;
    u32 w0;
    s32 first;

    first = 1;
    latched = NULL;
    for (cmd = dl;; cmd++) {
        w0 = cmd->words.w0;
        switch (w0 >> 24) {
            case G_SPNOOP:
                if (((w0 >> 16) & 0xFF) == LOD_MARKER && !((1u << (w0 & 0x1F)) & keep) && cmd->words.w1 != 0) {
                    cmd->words.w0 = G_DL_BRANCH_WORD;
                }
                break;
            case G_VTX:
                if (first) {
                    first = 0;
                    if (cmd->words.w1 != 0) {   /* PC: none where the converter found none (it said so) */
                        mirror_vertex_texcoords((Vtx *)cmd->words.w1, mirrored);
                    }
                }
                break;
            case G_RDPHALF_1:
                latched = (Gfx *)cmd->words.w1;
                break;
            case G_BRANCH_Z:
                if (latched == NULL) {
                    return;
                }
                cmd = latched;   /* the loop's step takes it to the list's second command */
                break;
            case G_ENDDL:
                return;
        }
    }
}
