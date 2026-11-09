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
    unsigned char prefix;
    s8 previous;
    u16 previous_end;
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
void TracePathPtForward(int mode,int segment,int distance,int increment,int *out_segment,int *out_distance) {
 if(mode==0 || segment<0) {
  *out_segment=segment;
  *out_distance=distance+increment;
  if(mode) {
   while(*out_distance>=gResPath.total)
    *out_distance=*out_distance+gCheckpoints[gCheckpoints[0].anchor].total-gResPath.total;
  } else {
   while(*out_distance>=gDronePaths[segment].total)
    *out_distance=gCheckpoints[gCheckpoints[0].anchor].first[segment]+*out_distance-gDronePaths[segment].total;
  }
 } else if(distance+increment<gResPath.nodes[segment].delta) {
  *out_segment=segment;
  *out_distance=distance+increment;
 } else {
  TracePathPtForward(mode,gResPath.nodes[segment].parent,gResPath.nodes[segment].end,increment-gResPath.nodes[segment].delta+distance,out_segment,out_distance);
 }
}
