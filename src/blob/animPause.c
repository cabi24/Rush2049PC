/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* N64 static-blit callback. The visibility helper is the genuine arcade
 * game/hud.c Hidden, pinned at 845329d7b36f5a384c5625ed9a0aef584ab46139.
 * The callback is N64-specific; proof:
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Blit {
    const char *Name;
    void *image;
    void *Info;
    s16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right;
    s16 color;
    u16 unknown26;
    s32 (*AnimFunc)(struct Blit *);
    u32 AnimID;
} Blit;
extern s32 pauseMenuSelect;
extern u32 animPause_color;
extern void UpdateBlit(Blit *);
/* MAT3 hud.c Hidden: the blit hidden or shown, updated when that changes. */
s32 Hidden(Blit *blt, s32 hide)
{
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    return blt->Hide;
}
s32 animPause(Blit *blt)
{
    if (Hidden(blt, pauseMenuSelect != 0))
        return 1;
    blt->image = &animPause_color;
    UpdateBlit(blt);
    blt->AnimFunc = 0;
    return 1;
}
