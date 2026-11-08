/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef float f32;
extern s16 gNumPlayers;
extern s32 gPlayerState[];

s32 playerBeingCreated(s32 count, s32 skip)
{
    s8 i;
    s32 found = 0;

    count = gNumPlayers;
    for (i = 0; i < count; i++) {
        if (i != skip && gPlayerState[i] == 3) {
            found = 1;
        }
    }
    return found;
}
