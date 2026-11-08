/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
/* The controller/pak records: the rumble request (rumble/group.c's Controller.request) is a record's byte at host
 * +0x89. PC: the stride here is the N64's record size (0x304), not the PC's 0x310, so the store misses past
 * controller 0's; nothing calls this. */
extern s8 gPaks[];
void PS_SetRumblePercent(s32 index,u32 value) {gPaks[0x89 + index*772]=value;}
