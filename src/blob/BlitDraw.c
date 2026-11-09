/* BlitDraw: one textured rectangle of the blitter (MAT3 blit_pc.c; the N64 side of it, an RDP texture rectangle). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union Gfx {struct {u32 w0; uintptr_t w1;} words;double align;} Gfx;

/* SDK forms from reference/repos/ultralib/include/PR/gbi.h: _SHIFTL,
   gImmp1 and gSPTextureRectangle, F3DEX2 command values. */
#define _SHIFTL(v,s,w) ((u32)(((u32)(v)&((0x01U<<(w))-1))<<(s)))
#define gImmp1(pkt,c,p0) {Gfx *_g=(Gfx *)(pkt);_g->words.w0=_SHIFTL((c),24,8);_g->words.w1=(u32)(p0);}
#define gSPTextureRectangle(pkt,xl,yl,xh,yh,tile,s,t,dsdx,dtdy) { \
 Gfx *_g=(Gfx *)(pkt); \
 _g->words.w0=(_SHIFTL(0xE4,24,8)|_SHIFTL(xh,12,12)|_SHIFTL(yh,0,12)); \
 _g->words.w1=(_SHIFTL(tile,24,3)|_SHIFTL(xl,12,12)|_SHIFTL(yl,0,12)); \
 gImmp1(pkt,0xE1,(_SHIFTL(s,16,16)|_SHIFTL(t,0,16))); \
 gImmp1(pkt,0xF1,(_SHIFTL(dsdx,16,16)|_SHIFTL(dtdy,0,16))); }

/* Draws the texture loaded last (blit_load_texture) into the screen rectangle x..right, y..bottom, its top left
 * texel at (s, t). The rectangle is clipped to the scissor box, the texture coordinates moving with the clipped edge
 * (from the opposite edge when the texture is flipped that way). A flip (BLIT_ATTR_FLIP_X, BLIT_ATTR_FLIP_Y) starts
 * the coordinates at the far edge and steps them backwards. Copy mode (blitMode 0) steps 4 texels a pixel across
 * (the RDP's copy mode draws four at once) and takes the rectangle's right and bottom edges as exclusive; the other
 * modes step one and include them. BLIT_ATTR_TALL, a screen of more than 220 lines, draws each texel row twice
 * (half steps down the texture) and so makes the rectangle twice as tall. Coordinates go to the RDP in 10.2 fixed
 * point for the screen and 10.5 for the texture. */
void BlitDraw(int x,int y,int right,int bottom,int s,int t)
{
    int height,step,offset,texture_edge;
    if(x<blitXmin) {
        if(!(blitAttribute&BLIT_ATTR_FLIP_X))s+=blitXmin-x;
        x=blitXmin;
    }
    if(y<blitYmin) {
        if(!(blitAttribute&BLIT_ATTR_FLIP_Y))t+=blitYmin-y;
        y=blitYmin;
    }
    if(right>blitXmax) {
        if(blitAttribute&BLIT_ATTR_FLIP_X)s=s-blitXmax+right;
        right=blitXmax;
    }
    if(bottom>blitYmax) {
        if(blitAttribute&BLIT_ATTR_FLIP_Y)t=t-blitYmax+bottom;
        bottom=blitYmax;
    }
    if(right<x || bottom<y)return;
    if(!blitMode) {
        /* copy mode: 4 texels a pixel across (the steps are s5.10: 1024 is one texel) */
        if((blitAttribute&BLIT_ATTR_FLIP_X)&&(blitAttribute&BLIT_ATTR_FLIP_Y)) {
            texture_edge=s+right;s=texture_edge-x;t+=bottom-y;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,-4096,-1024);
        } else if(blitAttribute&BLIT_ATTR_FLIP_X) {
            texture_edge=s+right;s=texture_edge-x;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,-4096,1024);
        } else if(blitAttribute&BLIT_ATTR_FLIP_Y) {
            t+=bottom-y;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,4096,-1024);
        } else {
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,4096,1024);
        }
    } else {
        height=bottom-y;
        if(blitAttribute&BLIT_ATTR_TALL) {bottom+=height+1;step=512;offset=16;}   /* half a texel a line */
        else {step=1024;offset=0;}
        if((blitAttribute&BLIT_ATTR_FLIP_X)&&(blitAttribute&BLIT_ATTR_FLIP_Y)) {
            texture_edge=s+right;s=texture_edge-x;t+=height;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,(t<<5)+offset,-1024,-step);
        } else if(blitAttribute&BLIT_ATTR_FLIP_X) {
            texture_edge=s+right;s=texture_edge-x;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,t<<5,-1024,step);
        } else if(blitAttribute&BLIT_ATTR_FLIP_Y) {
            t+=height;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,(t<<5)+offset,1024,-step);
        } else {
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,t<<5,1024,step);
        }
    }
}
