/* port/include/pc_gfx_n64.h: display-list commands in the N64's form, read into the PC's records (pc_gfx_list.h).
 *
 * The cartridge holds display lists as the N64's RSP and RDP read them: F3DEX2 and RDP commands, each an opcode and
 * its operands packed into two 32-bit words. Two places meet them: the WAD converter, which builds a record for each
 * command of a file's lists when the file loads (port/assets/wad.c), and StartGraphics, which reads the cartridge's
 * two set-up lists (rsp_init_dl, rdp_init_dl, game data the loader fills from the cartridge) into records. Both go
 * through pc_gfx_from_n64 (port/assets/gfx_n64.c), the one place the N64's encoding is decoded (docs/port/GFX.md "The
 * command records"). */
#ifndef PC_GFX_N64_H
#define PC_GFX_N64_H

#ifndef PC_PRELUDE_H   /* the game's files have uintptr_t from pc_prelude.h, and no system headers */
#include <stdint.h>
#endif

#include "pc_gfx_list.h"

/* A command in the N64's form: w0, and w1 widened to a pointer's size, as the data loader lays out the cartridge's
 * set-up lists. */
typedef struct GfxWords {
    unsigned int w0;
    uintptr_t w1;
} GfxWords;

/* The record of the command `cmd` (prev and next: the commands before and after it in its list, or NULL). A pointer
 * the record holds is (void *)w1 as it is, which a file's converter then makes the host's. A G_BRANCH_Z's list is
 * the G_RDPHALF_1 before it (its w1); a G_RDPHALF_1 after a texture rectangle is that rectangle's texture coordinates
 * (with the G_RDPHALF_2 after it); any other half is a no-op. Returns 0, or 1 for an opcode F3DEX2 doesn't have,
 * which becomes a no-op. */
int pc_gfx_from_n64(Gfx *out, const GfxWords *prev, const GfxWords *cmd, const GfxWords *next);

/* The n commands of a list in the N64's form as records, one for each. */
void pc_gfx_list_from_n64(Gfx *out, const GfxWords *in, int n);

#endif /* PC_GFX_N64_H */
