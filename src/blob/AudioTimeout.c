/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
extern f32 gAudioSeconds;
s32 AudioTimeout(f32 x,f32 delta) {f32 sum=x+delta;if(gAudioSeconds<x)sum-=14400.0f;return sum<gAudioSeconds;}
