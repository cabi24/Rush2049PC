/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef signed short s16;
typedef unsigned short u16;
typedef struct Track80 {
    unsigned char prefix[28];
    s16 flags;
    s16 timer_a;
    s16 timer_b;
    s16 cells[20];
    unsigned char tail[6];
} Track80;
typedef struct Track812 {
    s16 limit;
    s16 selected_a;
    s16 selected_b;
    s16 selected_c;
    s16 count;
    s16 reserved;
    Track80 tracks[10];
} Track812;
extern Track812 gCheckpoints;
extern void *gPathWave;
extern void *memcpy(void *,const void *,unsigned int);
void ReadCheckpointData(void)
{
    s16 i,j;
    memcpy(&gCheckpoints,gPathWave,812);
    gCheckpoints.selected_c=-1;
    gCheckpoints.selected_a=gCheckpoints.selected_c;
    gCheckpoints.selected_b=gCheckpoints.selected_a;
    gCheckpoints.limit=90;
    for(i=0;i<gCheckpoints.count;i++) {
        /* The original executes this three-iteration no-write loop. */
        for(j=0;j<3;j++) {}
        if(gCheckpoints.selected_b<0 && (gCheckpoints.tracks[i].flags&1))
            gCheckpoints.selected_b=i;
        if(gCheckpoints.selected_a<0 && (gCheckpoints.tracks[i].flags&4))
            gCheckpoints.selected_a=i;
        if(gCheckpoints.selected_c<0 && (gCheckpoints.tracks[i].flags&2))
            gCheckpoints.selected_c=i;
        for(j=0;j<20;j++) gCheckpoints.tracks[i].cells[j]=0;
        gCheckpoints.tracks[i].timer_b=45;
        gCheckpoints.tracks[i].timer_a=gCheckpoints.tracks[i].timer_b;
    }
    if(gCheckpoints.selected_b<0)gCheckpoints.selected_b=0;
    if(gCheckpoints.selected_a<0)gCheckpoints.selected_a=0;
    if(gCheckpoints.selected_c<0)gCheckpoints.selected_c=0;
}
