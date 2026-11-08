/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
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
extern TrackRecord gCheckpoints[];
extern Header gResPath;
extern Count gDronePaths[];
void TraceToIndex(int segment,int distance,int amount,int *out_segment,int *out_distance) {
 if(segment<0) {
  *out_segment=segment;
  *out_distance=distance-amount;
  if(distance<gCheckpoints[gCheckpoints[0].anchor].total) {
   if(*out_distance<0)*out_distance=0;
  } else {
   while(*out_distance<gCheckpoints[gCheckpoints[0].anchor].total)
    *out_distance=*out_distance+gResPath.total-gCheckpoints[gCheckpoints[0].anchor].total;
  }
 } else if(distance>=amount) {
  *out_segment=segment;
  *out_distance=distance-amount;
 } else {
  TraceToIndex(gResPath.nodes[segment].previous,gResPath.nodes[segment].previous_end,amount-distance,out_segment,out_distance);
 }
}
