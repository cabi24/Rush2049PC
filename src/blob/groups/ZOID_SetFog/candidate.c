#include "game/game.h"

typedef struct Descriptor72 {
    void *first, *bounds;
    s32 orthographic;
    f32 horizontal, vertical, tan_horizontal, tan_vertical, inverse_horizontal, inverse_vertical;
    f32 width, height, near_plane, far_plane, aspect, zoom, fog;
    s16 range_start; u16 range_end;
    u8 red, green, blue, alpha;
} Descriptor72;
VIEW(Descriptor72[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

#define GPACK_RGBA5551(r, g, b, a) ((((r) << 8) & 0xf800) | (((g) << 3) & 0x7c0) | (((b) >> 2) & 0x3e) | ((a) & 1))

Descriptor72 *ZOID_SetFog(s16 range_start, s16 range_end, u8 red, u8 green, u8 blue, u8 alpha, u32 index)
{
    Descriptor72 *win = &ZoidWinInfo[index];
    win->range_start = range_start;
    win->range_end = range_end;
    win->red = red;
    win->green = green;
    win->blue = blue;
    win->alpha = alpha;
    ZOID_SetBGColor((u16)(((red<<8)&0xf800)|((green<<3)&0x7c0)|((blue>>2)&0x3e))|1);
    return win;
}
