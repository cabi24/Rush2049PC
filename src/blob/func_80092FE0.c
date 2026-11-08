/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ModelSlot { unsigned int flags, w4; void *obj; float scale, w; unsigned short model; short child, next, mode; void *light, *tex[4], *rect, *clip, *pal; unsigned int v3C; char pad40[4]; } ModelSlot;
/* PC: a..f are the low halves of the car parts' s32 model handles (MBOX_NewObject's): the second s16 of each word on
 * the N64, the first on a little-endian host (docs/port/ENDIAN.md). */
typedef struct PlayerRec { short a; short p0; char p4[12]; short b; short p12; char p14[8]; short c; short p1E; short d; short q; short e; short q2; short f; short q3; char p2C[20]; } PlayerRec;
extern ModelSlot D_8012E700[];
extern PlayerRec D_80139320[];
void func_80092FE0(short idx, unsigned int *val) {
    PlayerRec *r = &D_80139320[idx];
    short a, b, c, d, e, f;
    a = r->a; D_8012E700[a].v3C = *val;
    b = r->b; D_8012E700[b].v3C = *val;
    c = r->c; D_8012E700[c].v3C = *val;
    d = r->d; D_8012E700[d].v3C = *val;
    e = r->e; D_8012E700[e].v3C = *val;
    f = r->f; D_8012E700[f].v3C = *val;
}
