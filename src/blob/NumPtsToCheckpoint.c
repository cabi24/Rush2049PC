/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * NumPtsToCheckpoint (0x800D2A74, 412 bytes; historical label, also aliased lap_counter_display):
 * N64-only path-distance helper, no arcade ancestor found.  Returns how many path points lie
 * ahead of `point` up to section `sec`'s marker on path `path`, wrapping by the path length:
 *   mode == 0          main path `path`: marker gCheckpoints[sec].first[path], wrap length
 *                      gDronePaths[path].total minus the loop section's marker (record 0 .anchor).
 *   mode != 0, path<0  same on the main loop: .total, wrap gResPath.total.
 *   mode != 0, path>=0 branch `path`: marker .second[path]; if it is unset (<0) or behind
 *                      `point`, recurse into the parent branch (nodes[path].parent) from the
 *                      branch's join point (nodes[path].end) and add the branch length (.delta).
 * Types (from loads; consistent with CalcResPathBounds and notes):
 *   gCheckpoints[] 80-byte section records (record 0 +2 = loop/anchor section);
 *   gResPath path graph header {u16 total; ...; u8 nbranches @8; Branch *branches @12},
 *   Branch 16 bytes {u8 kind; .. s8 parent @4; u16 end @6; .. u16 count @10; Vertex *pts @12}.
 * Shaping: NO named locals.  Every named local costs a frame slot (retail frame is 32 bytes with
 * only the call-spill slots), so each table read is written out in full and CSE'd by uopt.  The
 * recursive term is written `delta + recursion` (retail adds the call result first).
 * Does not match at -O2 (s0/s1 saved instead of the caller-home spill).
 */
typedef signed short s16;
typedef unsigned short u16;
typedef signed char s8;
typedef struct TrackRecord {
    u16 prefix;
    s16 anchor;
    unsigned char opaque[42];
    s16 total;
    s16 first[4];
    s16 second[12];
} TrackRecord;
/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct Descriptor {
    unsigned char prefix[4];
    s8 parent;
    unsigned char gap;
    u16 end;
    unsigned char gap2[2];
    u16 delta;
    unsigned char tail[12];   /* the native pts pointer */
} Descriptor;
typedef struct Header {
    u16 total;
    unsigned char opaque[22];
    Descriptor *nodes;
} Header;
typedef struct Count {u16 total;unsigned char opaque[14];} Count;
extern TrackRecord gCheckpoints[];
extern Header gResPath;
extern Count gDronePaths[];

int NumPtsToCheckpoint(int mode, int path, int point, int sec) {
    if (mode == 0) {
        if (gCheckpoints[sec].first[path] < point) {
            return gDronePaths[path].total - point + gCheckpoints[sec].first[path]
                - ((sec >= gCheckpoints[0].anchor) ? gCheckpoints[gCheckpoints[0].anchor].first[path] : 0);
        }
        return gCheckpoints[sec].first[path] - point;
    }
    if (path < 0) {
        if (gCheckpoints[sec].total < point) {
            return gResPath.total - point + gCheckpoints[sec].total
                - ((sec >= gCheckpoints[0].anchor) ? gCheckpoints[gCheckpoints[0].anchor].total : 0);
        }
        return gCheckpoints[sec].total - point;
    }
    if (gCheckpoints[sec].second[path] >= 0 && gCheckpoints[sec].second[path] >= point)
        return gCheckpoints[sec].second[path] - point;
    return gResPath.nodes[path].delta
        + NumPtsToCheckpoint(mode, gResPath.nodes[path].parent, gResPath.nodes[path].end, sec)
        - point;
}
