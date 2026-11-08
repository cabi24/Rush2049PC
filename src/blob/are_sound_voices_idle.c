/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)
typedef struct {s8 active;u8 pad[23];} Record;
extern s32 gNumInstances;
extern Record *gSfxInstances;
s32 are_sound_voices_idle(void) {s32 i;for(i=0;i<gNumInstances;i++){if(gSfxInstances[i].active)return 0;}return 1;}
