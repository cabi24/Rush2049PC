/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ReadPathData @ 0x800BAAA0, 696 bytes (174 words) -- EQUAL in the whole-program unit with its internal
 * callee CalcCheckpointTimes (provisional until now) as fellow member of group read_path_data.
 * Identity: GameCube ReadPathData (pathcommon.c, 0x8003220C; void ReadPathData(void); locals s16 i, p, j,
 * char *ptr), proposed rename (not applied). Not audio: it parses the track's path block at gPathWave+0x32C
 * (gPathWave): the resurrection-path header gResPath (gResPath, 16 B), its alternate ranges gAltPaths
 * (gAltPaths, 16-B records), the point count gNumResPathPts (gNumResPathPts) and points gResPathPtList
 * (gResPathPtList, s16[3]), then four drone paths gDronePaths (gDronePaths, 8-B {u16 num; pts}) with
 * gNumPathPts (gNumPathPts) and gPathPtList (gPathPtList); then SyncCheckpointsToPaths(-1) (SyncCheckpointsToPaths),
 * SyncAltPaths (SyncAltPaths), CalcResPathBounds (CalcResPathBounds), CalcCheckpointTimes
 * (CalcCheckpointTimes). On gRestartGame (gRestartGame) only the five calls run.
 * The GC build byte-swaps every field in its loops; N64 keeps the loops with empty bodies (big-endian data),
 * which is what retail compiles (the s16 counters still run). Ported from the GC body: GC statement
 * order, and the else arm as GC's chain `gResPath.alt = gAltPaths = 0;` -- with two separate stores the
 * gAltPaths value is not forwarded into the next loop (34 rows); the chain (or `gAltPaths = 0; alt = gAltPaths;`)
 * is EQUAL. N64 order of the three drone-path stores (count, list, pts) follows retail. No shaping devices.
 */
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed int s32;
typedef s16 Vertex[3];
typedef struct AltPath {
    u8 type;
    u8 unk01[9];
    u16 numPts;
    Vertex *pts;
} AltPath;
typedef struct ResPath {
    u16 numPrimary;
    u16 unk02;
    Vertex *pts;
    u8 numAlt;
    u8 unk09[3];
    AltPath *alt;
} ResPath;
typedef struct DronePath {
    u16 num;
    u16 unk02;
    void *pts;
} DronePath;
extern s8 gRestartGame;
extern char *gPathWave;
extern ResPath gResPath;
extern AltPath *gAltPaths;
extern u16 gNumResPathPts;
extern Vertex *gResPathPtList;
extern DronePath gDronePaths[4];
extern s16 gNumPathPts[4];
extern void *gPathPtList[4];
extern void *memcpy(void *, const void *, unsigned int);
void CalcCheckpointTimes(void);
void SyncCheckpointsToPaths(s16 mode);
void SyncAltPaths(void);
void CalcResPathBounds(void);

/* PC: the file is in host order (port/assets/path.c), but its headers have 4-byte pointer slots. The resurrection
 * and drone path headers are copied field by field into the native gResPath and gDronePaths, and the alternative
 * paths, whose pts this sets and every reader reads with the native stride, are kept in a native array (numAlt is a
 * u8) instead of in the file (docs/port/ASSETS.md §4 #22). */
static AltPath pc_alt_paths[256];

void ReadPathData(void)
{
    s16 i;
    s16 p;
    char *ptr;

    if (gRestartGame) {
        CalcCheckpointTimes();
        SyncCheckpointsToPaths(-1);
        SyncAltPaths();
        CalcResPathBounds();
        CalcCheckpointTimes();
        return;
    }
    ptr = gPathWave + 0x32C;
    memcpy(&gResPath, ptr, 4);                        /* numPrimary, unk02 */
    memcpy(&gResPath.numAlt, ptr + 8, 4);             /* numAlt, unk09 */
    ptr += 0x10;
    gAltPaths = pc_alt_paths;
    for (i = 0; i < gResPath.numAlt; i++) {
        memcpy(&pc_alt_paths[i], ptr, 12);              /* everything before the pts slot */
        ptr += 16;
    }
    memcpy(&gNumResPathPts, ptr, 2);
    ptr += 2;
    gResPathPtList = (Vertex *)ptr;
    ptr += gResPath.numPrimary * 6;
    gResPath.pts = gResPathPtList;
    if (gResPath.numAlt > 0) {
        gResPath.alt = gAltPaths;
    } else {
        gResPath.alt = gAltPaths = 0;
    }
    for (i = 0; i < gResPath.numAlt; i++) {
        gAltPaths[i].pts = (Vertex *)ptr;
        ptr += gAltPaths[i].numPts * 6;
    }
    for (p = 0; p < 4; p++) {
        memcpy(&gDronePaths[p], ptr, 4);                 /* num, unk02 */
        gNumPathPts[p] = gDronePaths[p].num;
        ptr += 8;
        gPathPtList[p] = ptr;
        gDronePaths[p].pts = ptr;
        ptr += gDronePaths[p].num * 8;
    }
    SyncCheckpointsToPaths(-1);
    SyncAltPaths();
    CalcResPathBounds();
    CalcCheckpointTimes();
}
