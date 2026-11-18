/* prepare_model_dl (relocate_model_dl until NATIVE.md step 10): a model's display list set up for the race once its
 * WAD is loaded (N64 only: MAT3's GameCube models have no display lists). Called by SetupWaveModel for each level of
 * detail. On the N64 it relocated the list too: it added the file's address to every address in it, in place. On the
 * PC the WAD's converter built the list as records with host pointers when the file loaded (port/assets/wad.c,
 * docs/port/NATIVE.md step 10), so what is left is the race's: the sections it keeps and the mirrored texture
 * coordinates. */
#include "game/game.h"

/* Walks the display list dl a record at a time to its end: past a call (not into it), and at a GFX_OP_BRANCH_Z on at
 * the second record of the list it names. A level-of-detail marker (GFX_OP_SECTION) whose bit isn't in keep becomes a
 * branch to the list where its section ends, so the race skips the section. The first vertex load's vertices are
 * prepared for mirroring (mirror_vertex_texcoords; mirrored: the track is mirrored).
 * On the N64 the marker's bit was 1 << (its number), which the CPU took modulo 32: so is it here. A marker or a branch
 * that names no list (where the N64's word named no place in the file: the converter said so) stays a no-op, or ends
 * the walk, where the N64 branched into whatever lay there. */
void prepare_model_dl(Gfx *dl, s32 mirrored, u32 keep) {
    Gfx *cmd;
    s32 first;

    first = 1;
    for (cmd = dl;; cmd++) {
        switch (cmd->op) {
            case GFX_OP_SECTION:
                if (!((1u << (cmd->section.bit & 0x1F)) & keep) && cmd->section.end != NULL) {
                    *cmd = gbi_list(GFX_OP_BRANCH_LIST, cmd->section.end);
                }
                break;
            case GFX_OP_VERTICES:
                if (first) {
                    first = 0;
                    if (cmd->vertices.vertices != NULL) {   /* none where the converter found none (it said so) */
                        mirror_vertex_texcoords(cmd->vertices.vertices, mirrored);
                    }
                }
                break;
            case GFX_OP_BRANCH_Z:
                if (cmd->branch_z.list == NULL) {
                    return;
                }
                cmd = cmd->branch_z.list;   /* the loop's step takes it to the list's second record */
                break;
            case GFX_OP_END_LIST:
                return;
        }
    }
}
