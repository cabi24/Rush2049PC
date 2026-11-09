/* relocate_model_dl: the relocation of a model's display list after its WAD is loaded (N64 only: MAT3's GameCube
 * models have no display lists). Called by SetupWaveModel for each level of detail. */
#include "game/game.h"

/* ROM display lists stay in N64 form on PC: 8-byte big-endian commands with 32-bit N64 addresses
 * (docs/port/POINTERS.md). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
#define DL_AT(addr) ((u32 *)pc_n64_ptr(addr))
/* The relocated address past 16 MiB of a larger window (game.memory) is the K0 one, not the 24-bit offset; at
 * original, the N64's (pc_rcp_reloc, docs/port/MEMORY.md 3). KEEP: w's own segment; SEG: segment bits >> 24. */
#define RELOC_KEEP(w, b) pc_rcp_reloc(w, b, -1)
#define RELOC_SEG(w, b, bits) pc_rcp_reloc(w, b, (s32)((bits) >> 24))
#define LOD_KEEP(sum, bits, w, b) pc_rcp_reloc(w, b, -1)
#define LOD_SEG(sum, bits, w, b) pc_rcp_reloc(w, b, (s32)((bits) >> 24))

/* The F3DEX2 commands the walk looks at (the first byte of a command). */
#define G_VTX 0x01
#define G_BRANCH_Z 0x04
#define G_MTX 0xDA
#define G_MOVEWORD 0xDB
#define G_MOVEMEM 0xDC
#define G_LOAD_UCODE 0xDD
#define G_DL 0xDE
#define G_ENDDL 0xDF
#define G_SPNOOP 0xE0       /* the models' section markers: type 1 in bits 16-23, the section's bit in 0-15 */
#define G_RDPHALF_1 0xE1
#define G_SETTIMG 0xFD

/* Walks the display list at dl to its end and relocates every address in it by base: vertices, matrices, the lists
 * it calls or branches to, images. seg < 0 keeps each address's own segment, else the relocated addresses get
 * segment seg. The walk doesn't enter the lists a G_DL calls (the stack of ten is never pushed); at a G_BRANCH_Z it
 * goes on at the target's second command and doesn't come back to the commands after the branch. A section marker
 * whose bit isn't in mask (DL_KEEP_*: waves.c) becomes a branch to the address it holds, skipping the section. The
 * first vertex load's vertices are prepared for mirroring (mirror_vertex_texcoords; flag: the track is mirrored). */
void relocate_model_dl(u32 *dl, s32 base, s32 seg, s32 flag, u32 mask) {
    u32 *cmd;
    u32 *head;
    u32 val;
    u32 saved;
    s32 first;
    s32 was;
    u8 op;
    u8 op2;
    u32 *stk[10];
    u32 *start;
    u32 **slot;
    u32 *nextcmd;
    u32 segbits;
    u32 segnib;
    u32 newaddr;
    s32 w1;
    s32 depth;
    s32 type;
    s32 idx;
    u32 w0;
    u32 one;
    u32 bit;
    u32 hit;
    s32 islod;
    u32 sum;
    u32 hiaddr;
    u32 sum2;
    u32 segnib2;
    u32 segbits2;
    u32 branch;
    u32 *target;
    s32 up;
    u32 vtx;
    s32 step;

    first = 1;
    depth = 0;
    start = dl;
    stk[0] = start;

    while (depth >= 0 && depth < 10) {
        head = stk[depth];
        slot = &stk[depth];
        cmd = *slot;
        op = (DLW(head[0]) & 0xFF000000) >> 24;
        /* commands with no address (the RDP's, most of the RSP's) */
        if ((op & 0xC0) == 0x40 || (op & 0xC0) == 0x80 || (op >= 9 && op < 0x40) || (op >= 0xC0 && op < 0xD6)) {
            goto next;
        }
        nextcmd = cmd + 2;
        op2 = (DLW(nextcmd[0]) & 0xFF000000) >> 24;
        if (op == G_VTX || op == G_LOAD_UCODE || op == G_DL || op == G_MTX || op == G_MOVEMEM ||
            (op == G_RDPHALF_1 && (op2 == G_BRANCH_Z || op2 == G_LOAD_UCODE)) || op >= G_SETTIMG) {
            if (seg < 0) {
                val = RELOC_KEEP(DLW(cmd[1]), base);
            } else {
                segnib = seg & 0xF;
                segbits = segnib << 24;
                val = RELOC_SEG(DLW(cmd[1]), base, segbits);
            }
            newaddr = val;
            DLW_SET(cmd[1], newaddr);
        }
        switch (op) {
            case G_SPNOOP:
                type = (DLW(cmd[0]) & 0x00FF0000) >> 16;
                w1 = DLW(cmd[1]);
                w0 = DLW(cmd[0]);
                idx = w0 & 0xFFFF;
                one = 1;
                bit = one << idx;
                islod = type == 1;
                if (islod && (hit = bit & mask) == 0) {
                    if (seg < 0) {
                        sum = w1 + (u32) base;
                        hiaddr = w1 & 0x0F000000;
                        val = LOD_KEEP(sum, hiaddr, w1, base);
                    } else {
                        sum2 = w1 + (u32) base;
                        segnib2 = seg & 0xF;
                        segbits2 = segnib2 << 24;
                        val = LOD_SEG(sum2, segbits2, w1, base);
                    }
                    branch = 0xDE010000;    /* G_DL without return: a branch */
                    DLW_SET(cmd[0], branch);
                    DLW_SET(cmd[1], val);
                }
                break;
            case G_VTX:
                was = first != 0;
                first = 0;
                if (was) {
                    vtx = val;
                    mirror_vertex_texcoords(vtx, flag);
                }
                break;
            case G_RDPHALF_1:
                saved = DLW(cmd[1]);
                break;
            case G_BRANCH_Z:
                target = DL_AT(saved);
                stk[depth] = target;
                break;
            case G_DL:
                step = 2;
                stk[depth] += step;
                break;
            case G_ENDDL:
                up = depth - 1;
                depth = up;
                break;
            case G_MOVEWORD:
            case G_MOVEMEM:
            case G_LOAD_UCODE:
                break;
        }
next:
        if (op != G_DL && depth >= 0) {
            stk[depth] += 2;
        }
    }
}
