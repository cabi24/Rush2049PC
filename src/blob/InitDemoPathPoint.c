/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;typedef unsigned char u8;typedef signed short s16;typedef unsigned short u16;typedef int s32;typedef unsigned int u32;typedef float f32;
#define FIELD(p,t,o) (*(t *)((u8 *)(p)+(o)))
extern s8 gBackwards,gTrackNum;extern s16 cur_demo_ppoint[],num_demo_ppoints[],gDemoPathPt;extern s16 *demo_path_points[];
void InitDemoPathPoint(void) {
 s32 index,next;
 if(gBackwards)index=gTrackNum+6;else index=gTrackNum;
 next=cur_demo_ppoint[index]+1;
 if(next<0||next>=num_demo_ppoints[index])next=0;
 cur_demo_ppoint[index]=next;
 gDemoPathPt=demo_path_points[index][next];
}
