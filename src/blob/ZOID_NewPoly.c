/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned short u16;
typedef signed short s16;
typedef float f32;
typedef unsigned char u8;
typedef struct Record88 { s16 active; u16 flags, parameter; unsigned char rest[82]; } Record88;
extern Record88 PolyInstanceList[];
extern int gNumPolyInsts,gMaxPolyInsts;
extern void ZOID_UpdatePoly(Record88 *, int, f32 *, u16, u8 *, u16, int);
Record88 *ZOID_NewPoly(int active, f32 *vertices, u16 parameter, u8 *color, u16 flags, int indexed)
{
    int i;
    Record88 *record;
    for (i = 0; i < gNumPolyInsts; i++) {
        if (!PolyInstanceList[i].active) break;
    }
    if (i >= 200) return 0;
    record = &PolyInstanceList[i];
    if (i >= gNumPolyInsts) gNumPolyInsts = i + 1;
    if (gMaxPolyInsts < gNumPolyInsts) gMaxPolyInsts = gNumPolyInsts;
    record->flags = flags;
    if (indexed) {
        record->flags |= 0x400;
        record->parameter = parameter;
    }
    ZOID_UpdatePoly(record, active, vertices, parameter, color, 0, indexed);
    return record;
}
