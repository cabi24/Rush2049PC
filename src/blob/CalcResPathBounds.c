/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * CalcResPathBounds @ 0x800B9740, 408 bytes (102 words) -- MATCH (the scorer fn standalone; unit EQUAL).
 * Identity: GameCube CalcResPathBounds (pathcommon.c, 0x800331B4; void CalcResPathBounds(void), locals
 * int i, j, validPt), proposed rename (not applied). Computes the bounding box of the resurrection path
 * points: gPathMin, gPathMax = gPathMax (s16[3]), gResPath, gResPathPtList =
 * gResPathPtList (s16[3] points), gNumResPathPts = gNumResPathPts. A point counts when it is one of the first
 * numPrimary points, or lies inside an alternate range of type 1 (pointer range test, no early break).
 * Port of the GC body: s16 extrema, `validPt`, indexed alternate ranges `alt[j]`, no break, and a
 * ternary store per axis (min = min < p ? min : p; max = max > p ? max : p) in an axis loop; the chained
 * extrema initialisers give retail's reload-and-copy. Layout field names follow GC (types.h has no
 * DWARF struct for gResPath; offsets are N64 retail's). No shaping devices, pads or false prototypes.
 * (break on hit, pointer-walk vertex, if-form updates) plateaued at 83/102.
 */
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned char u8;
typedef s16 Vertex[3];
typedef struct AltPath {
    u8 type;
    u8 unk01[9];
    u16 numPts;
    Vertex *pts;
} AltPath;
/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct ResPath {
    u16 numPrimary;
    u8 unk02[14];
    u8 numAlt;
    u8 unk09[3];
    AltPath *alt;
} ResPath;
extern s16 gPathMin[3];
extern s16 gPathMax[3];
extern ResPath gResPath;
extern Vertex *gResPathPtList;
extern u16 gNumResPathPts;

void CalcResPathBounds(void)
{
    int i, j, validPt;

    gPathMin[0] = gPathMin[1] = gPathMin[2] = 0x7FFF;
    gPathMax[0] = gPathMax[1] = gPathMax[2] = -0x7FFF;
    for (i = 0; i < gNumResPathPts; i++) {
        if (i < gResPath.numPrimary) {
            validPt = 1;
        } else {
            validPt = 0;
            for (j = 0; j < gResPath.numAlt; j++) {
                if (&gResPathPtList[i] >= gResPath.alt[j].pts &&
                    &gResPathPtList[i] < gResPath.alt[j].pts + gResPath.alt[j].numPts &&
                    gResPath.alt[j].type == 1) {
                    validPt = 1;
                }
            }
        }
        if (validPt == 1) {
            for (j = 0; j < 3; j++) {
                gPathMin[j] = (gPathMin[j] < gResPathPtList[i][j]) ? gPathMin[j] : gResPathPtList[i][j];
                gPathMax[j] = (gPathMax[j] > gResPathPtList[i][j]) ? gPathMax[j] : gResPathPtList[i][j];
            }
        }
    }
}
