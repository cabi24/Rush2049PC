/* TextureAnimation.c: run a track's texture animations each frame (MAT3 textureanim.c, TextureAnimation()).
 *
 * The palette animations (PaletteLists) and texture flipbooks (TextureSwapLists) InitTextureAnimation set up: palette
 * rotations and swaps, traffic lights and lamp chasers drawn as palette colours, texture scrolls in the textures'
 * load display lists, and textures whose image steps through a series of others. The N64 adds the palette effects
 * 0-8 to MAT3's scrolls. */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef struct Gfx { u32 w0; uintptr_t w1; } Gfx;
/* What a palette animation works on: the palette's colours, or a texture's load display list (the scrolls). */
typedef union AnimatedData { u16 *palette; Gfx *commands; } AnimatedData;
/* A palette animation (MAT3 tPaletteAnim); state is a PALANIM_* (scene.h). */
typedef struct tPaletteAnim {
    void *name;         /* NULL ends the list */
    s16 start, end;     /* the entries it works on; the scroll's position and its wrap */
    s8 offset;          /* the swaps' gap, the scroll's step, the chasers' counter */
    u8 state;
    s16 speed;          /* the period, in 30 Hz frames */
    f32 time;           /* the time left to the next step, seconds */
    AnimatedData ptr;
} tPaletteAnim;
/* PC: tTexture is a texture record (TXHD) of a WAD, which keeps its N64 layout: texBase is a 32-bit RDRAM-window
 * address. The palettes are the WAD's TLUTs, which stay big-endian, as do the TXLD lists (port/assets/wad.c). */
#define PAL_SET(entry, value) pc_be16_set(&(entry), value)
/* ... which displaylist_set_texture_image writes the frame's address into: N64PTR0(T, a) is the pointer at N64 address a
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64PTR0(T, a) ((T)PC_N64P(a))
/* A texture (MAT3 tTexture), the fields used here. */
typedef struct tTexture {
    u8 before24[24];
    u32 texBase;            /* +24 its load display list (Gfx *), or its image (flags & TEX_IMAGE_DIRECT) */
    u32 flags;              /* +28 */
} tTexture;
/* A flipbook frame (MAT3 tTextureBase). PC: its first word is its name, a pointer (InitTextureAnimation's Item12;
 * docs/port/POINTERS.md 10). */
typedef struct tTextureBase { char *name; tTexture *tex; Gfx *texBase; } tTextureBase;
/* A texture flipbook (MAT3 tTextureSwap; InitTextureAnimation.c). */
typedef struct tTextureSwap {
    s16 num, startnum, fd, idx;
    f32 time, initTime;
    tTextureBase *swapTexs;
} tTextureSwap;
/* PC: game.frame_rate 60 (pc_game_fps.h). The texture scrolls (actions 9 and 10) add step x the frame's retraces each
 * time they run: every frame when ticks is 0 (so step a retrace at any rate), else once every ticks 30 Hz frames, when
 * on the N64 the frame's retraces are its 2. At 60 those keep the N64's 2, so the scroll keeps its speed. */
#define PC_SCROLL_RETRACES(d, ticks) (pc_fps60 && (ticks) > 0 ? 2 : (d))

VIEW(tPaletteAnim *[], PaletteLists);
#define PaletteLists PaletteLists_view
VIEW(tTextureSwap *[], TextureSwapLists);
#define TextureSwapLists TextureSwapLists_view
VIEW(tTextureBase[], texture_swap_shared_frames);
#define texture_swap_shared_frames texture_swap_shared_frames_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(volatile int, gLastFrameCount);
#define gLastFrameCount gLastFrameCount_view

/* One frame of the track's (gTrackNum) texture animations, none in the attract demo without the Expansion Pak.
 * Each palette animation whose period is up starts the next and does its step (the PALANIM_* states; the light
 * cycles set their own periods, and the colours they write are RGBA5551). The scrolls move start by offset per
 * retrace, wrapped at end, and write it (in quarter texels: >> 2) into the texture's SETTILESIZE command (s for
 * PALANIM_SCROLL_S, t for PALANIM_SCROLL_T). Each flipbook whose time is up steps idx (wrapping, in the direction
 * fd) and makes the texture of frame startnum show frame idx's image: in texBase itself or in the SETTIMG command of
 * its load display list. Without the Expansion Pak the first six tracks leave out the flipbooks of the shared
 * frames, as InitTextureAnimation does. */
void TextureAnimation(void)
{
    tPaletteAnim *plist;
    tTextureSwap *tlist;
    u16 *palette;
    u16 saved;
    int i,gap;
    tTexture *tex;
    if((gGameState&8) && !expansion_pak_present) return;   /* GS_DEMO */
    plist=PaletteLists[gTrackNum];
    if(plist && plist->name) {
        do {
            plist->time-=gFrameSeconds;
            if(!(plist->time>0.0f)) {
                plist->time+=(f32)plist->speed*0.0333333f;
                palette=plist->ptr.palette;
                switch(plist->state) {
                case PALANIM_ROTATE_UP:
                    i=plist->end;
                    saved=palette[i];
                    for(i--;i>=plist->start;i--) palette[i+1]=palette[i];
                    palette[plist->start]=saved;
                    break;
                case PALANIM_ROTATE_DOWN:
                    i=plist->start;
                    saved=palette[i];
                    for(i++;i<=plist->end;i++) palette[i-1]=palette[i];
                    palette[plist->end]=saved;
                    break;
                case PALANIM_SWAP2:
                    gap=plist->end-plist->start+plist->offset+1;
                    for(i=plist->start;i<=plist->end;i++) {
                        saved=palette[i]; palette[i]=palette[i+gap]; palette[i+gap]=saved;
                    }
                    break;
                case PALANIM_CYCLE3:
                    gap=plist->end-plist->start+plist->offset+1;
                    for(i=plist->start;i<=plist->end;i++) {
                        saved=palette[i]; palette[i]=palette[i+gap]; palette[i+gap]=palette[i+gap+gap]; palette[i+gap+gap]=saved;
                    }
                    break;
                case PALANIM_LIGHT_GREEN:   /* green on, yellow and red dark; yellow next, in 10 s */
                    PAL_SET(palette[plist->start+2], 1985); PAL_SET(palette[plist->start+1], 10561); PAL_SET(palette[plist->start], 10241);
                    plist->state=PALANIM_LIGHT_YELLOW; plist->time=10.0f;
                    break;
                case PALANIM_LIGHT_YELLOW:  /* yellow on; red next, in 0.5 s */
                    PAL_SET(palette[plist->start+2], 321); PAL_SET(palette[plist->start+1], 0xffc1); PAL_SET(palette[plist->start], 10241);
                    plist->state=PALANIM_LIGHT_RED; plist->time=0.5f;
                    break;
                case PALANIM_LIGHT_RED:     /* red on; green next, in 10.5 s */
                    PAL_SET(palette[plist->start+2], 321); PAL_SET(palette[plist->start+1], 10561); PAL_SET(palette[plist->start], 0xf801);
                    plist->state=PALANIM_LIGHT_GREEN; plist->time=10.5f;
                    break;
                case PALANIM_CHASE10:       /* entries 0-9 light red one a period; at 21 all go dark */
                    if(plist->offset<10) PAL_SET(palette[plist->offset], 0xf001);
                    else if(plist->offset>=21) {
                        for(i=0;i<10;i++)
 PAL_SET(palette[i], 12421);
                        plist->offset=-1;
                    }
                    plist->offset++;
                    break;
                case PALANIM_CHASE7:        /* entries 0-6 light red, 7-14 white, one a period; at 23 all go dark */
                    if(plist->offset==0) PAL_SET(palette[plist->offset+7], 0xce73);
                    else if(plist->offset<8) {
                        PAL_SET(palette[plist->offset-1], 0xf141);
                        PAL_SET(palette[plist->offset+7], 0xce73);
                    } else if(plist->offset>=23) {
                        for(i=0;i<7;i++) {
 PAL_SET(palette[i], 12289);
 PAL_SET(palette[i+7], 4229);
 }
                        PAL_SET(palette[14], 4229); plist->offset=-1;
                    }
                    plist->offset++;
                    break;
                case PALANIM_SCROLL_S:
                    plist->start+=plist->offset*PC_SCROLL_RETRACES(gLastFrameCount, plist->speed);
                    if(plist->start<0) plist->start+=plist->end;
                    else if(plist->start>=plist->end) plist->start-=plist->end;
                    displaylist_set_tile_size((Gfx *)palette,plist->start>>2,-1,-1,-1);
                    break;
                case PALANIM_SCROLL_T:
                    plist->start+=plist->offset*PC_SCROLL_RETRACES(gLastFrameCount, plist->speed);
                    if(plist->start<0) plist->start+=plist->end;
                    else if(plist->start>=plist->end) plist->start-=plist->end;
                    displaylist_set_tile_size((Gfx *)palette,-1,plist->start>>2,-1,-1);
                    break;
                }
            }
            plist++;
        } while(plist->name);
    }
    tlist=TextureSwapLists[gTrackNum];
    if(tlist && tlist->num) {
        do {
            if(!(!expansion_pak_present && gTrackNum>=0 && gTrackNum<6 && tlist->swapTexs==texture_swap_shared_frames)) {
                tlist->time-=gFrameSeconds;
                if(tlist->time<=0.0f) {
                    tlist->time=tlist->initTime;
                    if(tlist->fd) {
                        tlist->idx++;
                        if(tlist->idx>=tlist->num) tlist->idx=0;
                    } else {
                        tlist->idx--;
                        if(tlist->idx<0) tlist->idx=tlist->num-1;
                    }
                    tex=tlist->swapTexs[tlist->startnum].tex;
                    if(tex->flags&TEX_IMAGE_DIRECT) tex->texBase=tlist->swapTexs[tlist->idx].texBase;
                    else displaylist_set_texture_image(N64PTR0(Gfx *, tex->texBase),tlist->swapTexs[tlist->idx].texBase);
                }
            }
            tlist++;
        } while(tlist->num);
    }
}
