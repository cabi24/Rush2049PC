/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {s16 pad0,first;u8 pad4[4];s16 count;} Path;
extern Path gCheckpoints;
s32 GetNextCheckpoint(s32 index) {if(index+1 == gCheckpoints.count)return gCheckpoints.first;return index+1;}
