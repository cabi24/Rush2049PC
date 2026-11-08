/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
extern u8 gPaks_rumble_request[];
void PS_SetRumblePercent(s32 index,u32 value) {gPaks_rumble_request[index*772]=value;}
