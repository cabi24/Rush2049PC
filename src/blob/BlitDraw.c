/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * BlitDraw (historical label; 1780 B): clipped textured-rectangle emitter. Clips (x, y, right, bottom) against the
 * clip window blitXmin/blitYmin (left/top) and blitXmax/blitYmax (right/bottom), adjusting the texture origin
 * (s, t) on the side the flip bits of blitAttribute select (4 = flip in x, 8 = flip in y); returns on an empty rectangle;
 * then appends one gSPTextureRectangle (E4/E1/F1 words) to the display list blit_dl for the four flip
 * combinations. blitMode != 0 selects the second mode (inclusive right/bottom, 1024 step, and with blitAttribute & 0x8000
 * a doubled height at step 512 / offset 16). No arcade or MAT3/Dreamcast ancestor identified (N64 RDP code).
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Shaping (disclosed): `do {} while (0);` at the end of the second mode's both-flips arm. It compiles to nothing; it
 * only moves uopt's alias-closure block for the blit_dl base inside the arm (texture_rect_compiler_boundary
 * diagnosis: that closure, emitted after the arm's unconditional exit, became an extra predecessor of the x-flip-only
 * test and kept as1's cross-block hoist of `texture_edge = s + right` out of that test's delay slot).
 * the scorer fn: MATCH. Unit (the whole-program unit --neighbours): EQUAL BlitDraw: 445 words (kept); locked bodies that differ: 0.
 */
typedef unsigned int u32;
typedef union Gfx {struct {u32 w0; uintptr_t w1;} words;double align;} Gfx;
extern Gfx *blit_dl;
extern int blitAttribute,blitXmin,blitXmax,blitYmin,blitYmax,blitMode;
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
void BlitDraw(int x,int y,int right,int bottom,int s,int t)
{
    int height,step,offset,texture_edge;
    if(x<blitXmin) {
        if(!(blitAttribute&4))s+=blitXmin-x;
        x=blitXmin;
    }
    if(y<blitYmin) {
        if(!(blitAttribute&8))t+=blitYmin-y;
        y=blitYmin;
    }
    if(right>blitXmax) {
        if(blitAttribute&4)s=s-blitXmax+right;
        right=blitXmax;
    }
    if(bottom>blitYmax) {
        if(blitAttribute&8)t=t-blitYmax+bottom;
        bottom=blitYmax;
    }
    if(right<x || bottom<y)return;
    if(!blitMode) {
        if((blitAttribute&4)&&(blitAttribute&8)) {
            texture_edge=s+right;s=texture_edge-x;t+=bottom-y;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,-4096,-1024);
        } else if(blitAttribute&4) {
            texture_edge=s+right;s=texture_edge-x;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,-4096,1024);
        } else if(blitAttribute&8) {
            t+=bottom-y;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,4096,-1024);
        } else {
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,right<<2,bottom<<2,0,s<<5,t<<5,4096,1024);
        }
    } else {
        height=bottom-y;
        if(blitAttribute&0x8000) {bottom+=height+1;step=512;offset=16;}
        else {step=1024;offset=0;}
        if((blitAttribute&4)&&(blitAttribute&8)) {
            texture_edge=s+right;s=texture_edge-x;t+=height;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,(t<<5)+offset,-1024,-step);
            do {} while (0);
        } else if(blitAttribute&4) {
            texture_edge=s+right;s=texture_edge-x;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,t<<5,-1024,step);
        } else if(blitAttribute&8) {
            t+=height;
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,(t<<5)+offset,1024,-step);
        } else {
            gSPTextureRectangle(blit_dl++,x<<2,y<<2,(right+1)<<2,(bottom+1)<<2,0,s<<5,t<<5,1024,step);
        }
    }
}
