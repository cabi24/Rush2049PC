/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef unsigned int u32;
typedef float f32;
typedef struct Egg { u8 size; u8 pad[3]; u32 levels[20]; u32 edges[20]; } Egg;
extern Egg gEggs[];        /* gEggs */
extern Egg *gEgg;         /* gEgg */
extern f32 gNextTimeOut;          /* gNextTimeOut */
extern s32 gEggIdx;          /* gEggIdx */

/* InitEgg, at its address in the front end (the image A loader names it InitEgg): src/ovl_a/func_8038FF90.c
 * was a second copy of it. */
void InitEgg(s32 egg)
{
    gEgg = &gEggs[egg];
    gNextTimeOut = 0.0f;
    gEggIdx = 0;
}
