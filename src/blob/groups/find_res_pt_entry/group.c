#include "game/game.h"

typedef struct Entry {u8 present;u8 pad[23];} Entry;
/* The track's path (FindResPtRecurse.c's PathGraph): its points, and its alternative paths (nodes). */
typedef struct { u16 numPoints; void *points; u8 numNodes; Entry *nodes; } PathGraph;
VIEW(PathGraph, gResPath);
#define gResPath gResPath_view

s32 FindResPt(s32 index,s32 value,s32 *out_index,s32 *out_value,s32 flags) {
    if(flags && gResPath.nodes[index].present) {
        *out_index=index;
        *out_value=value;
        return 1;
    }
    return FindResPtRecurse(index,value,out_index,out_value,flags,0);
}
