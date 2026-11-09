#include "game/game.h"

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
VIEW(TrackRecord[], gCheckpoints);
#define gCheckpoints gCheckpoints_view
VIEW(Header, gResPath);
#define gResPath gResPath_view
VIEW(Count[], gDronePaths);
#define gDronePaths gDronePaths_view

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
