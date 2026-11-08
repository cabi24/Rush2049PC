/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitCarObj (0x800AF5E0; historical label): create a scene record (ZOID_AddObject: allocate a
 * 0x44-byte record in ObjInstanceList[], word0 = flags, owner pointer, id, linked under `parent`) whose
 * owner is either a fresh object from ZOID_NewMatrix() (dynamic != 0), the fixed object unitmatrix
 * (col == -1, which also sets flag 0x80), or the pointer table entry gCarMatrices[row / 13 * 12 + col].
 * The flag word is ((layer << 8) ^ 0xF00) | flags, the same encoding SkyInit uses.
 *
 * Shaping: the table is indexed flat (`row / 13 * 12 + col`); a 2-D `[12]` declaration gives
 * the same instructions with ugen's mul-by-48 in one register (8 words of register naming).
 * -O3 only (26/44 at -O2).
 */
typedef short s16;
typedef int s32;

extern s32 unitmatrix;
extern s32 *gCarMatrices[];
void *ZOID_NewMatrix(void);
s32 ZOID_AddObject(s32 id, void *owner, s16 parent, s32 flags);

/* This wrapper ends in a call to ZOID_AddObject (MBOX_NewObject), whose result stays in v0 through the void
 * return; every caller reads it. On the PC the wrapper returns it (docs/port/PROTOTYPES.md). */
#define PC_V0_S32 s32
#define PC_V0_RETURN return
PC_V0_S32 InitCarObj(s32 id, s16 layer, s16 parent, s32 flags, s16 row, s16 col, s32 dynamic)
{
    void *owner;

    if (dynamic != 0) {
        owner = ZOID_NewMatrix();
    } else if (col == -1) {
        owner = &unitmatrix;
        flags |= 0x80;
    } else {
        owner = gCarMatrices[row / 13 * 12 + col];
    }
    PC_V0_RETURN ZOID_AddObject(id, owner, parent, ((layer << 8) ^ 0xF00) | flags);
}
