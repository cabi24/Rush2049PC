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
        if ((op & 0xC0) == 0x40 || (op & 0xC0) == 0x80 || (op >= 9 && op < 0x40) || (op >= 0xC0 && op < 0xD6)) {
            goto next;
        }
        nextcmd = cmd + 2;
        op2 = (DLW(nextcmd[0]) & 0xFF000000) >> 24;
        if (op == 1 || op == 0xDD || op == 0xDE || op == 0xDA || op == 0xDC || (op == 0xE1 && (op2 == 4 || op2 == 0xDD)) || op >= 0xFD) {
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
            case 0xE0:
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
                    branch = 0xDE010000;
                    DLW_SET(cmd[0], branch);
                    DLW_SET(cmd[1], val);
                }
                break;
            case 1:
                if (cmd[0] & 0x00FF0000) {}
                was = first != 0;
                first = 0;
                if (was) {
                    vtx = val;
                    mirror_vertex_texcoords(vtx, flag);
                }
                break;
            case 0xE1:
                saved = DLW(cmd[1]);
                break;
            case 4:
                target = DL_AT(saved);
                stk[depth] = target;
                break;
            case 0xDE:
                step = 2;
                stk[depth] += step;
                break;
            case 0xDF:
                up = depth - 1;
                depth = up;
                break;
            case 0xDB:
            case 0xDC:
            case 0xDD:
                break;
        }
next:
        if (op != 0xDE && depth >= 0) {
            stk[depth] += 2;
        }
    }
}
