#include "game/game.h"

typedef struct Viewport16 {s16 scale[4],translate[4];} Viewport16;
typedef struct Descriptor72 {
    void *first,*bounds;
    int orthographic;
    f32 horizontal,vertical,tan_horizontal,tan_vertical,inverse_horizontal,inverse_vertical;
    f32 width,height,near_plane,far_plane,aspect,zoom,fog;
    s16 range_start,range_end;
    u8 red,green,blue,alpha;
} Descriptor72;
VIEW(Descriptor72[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view
VIEW(Viewport16, vp0);
#define vp0 vp0_view

extern Descriptor72 *ZOID_SetWindow(int,f32,f32,f32,f32,f32,f32);
/* Exact SDK macro from reference/repos/ultralib/include/PR/gbi.h. */
#define GPACK_RGBA5551(r,g,b,a) ((((r)<<8)&0xf800)|(((g)<<3)&0x7c0)|(((b)>>2)&0x3e)|((a)&1))
Descriptor72 *ZOID_InitializeWindow(int index,void *first,void *bounds,f32 horizontal,f32 vertical,f32 width,f32 height,f32 near_plane,f32 far_plane)
{
    ZoidWinInfo[index].first=first;
    ZoidWinInfo[index].bounds=bounds;
    ZoidWinInfo[index].zoom=2.5f;
    ZoidWinInfo[index].fog=2000.0f;
    gViewableDistance=2000.0f;
    vp0.scale[0]=(s16)(width*2);
    vp0.translate[0]=(s16)(near_plane*2);
    vp0.scale[1]=(s16)(height*2);
    vp0.translate[1]=(s16)(far_plane*2);
    ZOID_SetWindow(index,horizontal,vertical,width,height,near_plane,far_plane);
    ZoidWinInfo[index].range_start=1000;
    ZoidWinInfo[index].range_end=999;
    ZoidWinInfo[index].red=0;
    ZoidWinInfo[index].green=0;
    ZoidWinInfo[index].blue=0;
    ZoidWinInfo[index].alpha=255;
    ZOID_SetBGColor((u16)(((ZoidWinInfo[index].red<<8)&0xf800)|((ZoidWinInfo[index].green<<3)&0x7c0)|((ZoidWinInfo[index].blue>>2)&0x3e))|1);
    return &ZoidWinInfo[index];
}
