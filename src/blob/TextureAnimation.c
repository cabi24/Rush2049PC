#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef struct Gfx { u32 w0; uintptr_t w1; } Gfx;
typedef union AnimatedData { u16 *palette; Gfx *commands; } AnimatedData;
typedef struct PaletteAnimation20 { void *owner; s16 first,last; s8 step; u8 action; s16 ticks; f32 delay; AnimatedData data; } PaletteAnimation20;
/* PC: ObjectView is a texture record (TXHD) of a WAD, which keeps its N64 layout: commands (texBase) is a 32-bit
 * RDRAM-window address. The palettes are the WAD's TLUTs, which stay big-endian, as do the TXLD lists
 * (port/assets/wad.c). */
#define TXHD_PTR(type) u32
#define PAL_SET(entry, value) pc_be16_set(&(entry), value)
/* ... which displaylist_set_texture_image writes the frame's address into: N64PTR0(T, a) is the pointer at N64 address a
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64PTR0(T, a) ((T)PC_N64P(a))
typedef struct ObjectView { u8 before24[24]; TXHD_PTR(Gfx) commands; u32 flags; } ObjectView;
/* PC: the frame's first word is its name, a pointer (InitTextureAnimation's Item12; docs/port/POINTERS.md 10). */
typedef struct AnimationFrame12 { char *tag; ObjectView *object; Gfx *commands; } AnimationFrame12;
typedef struct ObjectAnimation20 { s16 count,object_index,direction,index; f32 delay,interval; AnimationFrame12 *frames; } ObjectAnimation20;
/* PC: game.frame_rate 60 (pc_game_fps.h). The texture scrolls (actions 9 and 10) add step x the frame's retraces each
 * time they run: every frame when ticks is 0 (so step a retrace at any rate), else once every ticks 30 Hz frames, when
 * on the N64 the frame's retraces are its 2. At 60 those keep the N64's 2, so the scroll keeps its speed. */
#define PC_SCROLL_RETRACES(d, ticks) (pc_fps60 && (ticks) > 0 ? 2 : (d))

VIEW(PaletteAnimation20 *[], PaletteLists);
#define PaletteLists PaletteLists_view
VIEW(ObjectAnimation20 *[], TextureSwapLists);
#define TextureSwapLists TextureSwapLists_view
VIEW(AnimationFrame12[], texture_swap_shared_frames);
#define texture_swap_shared_frames texture_swap_shared_frames_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(volatile int, gLastFrameCount);
#define gLastFrameCount gLastFrameCount_view

void TextureAnimation(void)
{
    PaletteAnimation20 *palette_animation;
    ObjectAnimation20 *object_animation;
    u16 *palette;
    u16 saved;
    int i,offset;
    ObjectView *object;
    if((gGameState&8) && !expansion_pak_present) return;
    palette_animation=PaletteLists[gTrackNum];
    if(palette_animation && palette_animation->owner) {
        do {
            palette_animation->delay-=gFrameSeconds;
            if(!(palette_animation->delay>0.0f)) {
                palette_animation->delay+=(f32)palette_animation->ticks*0.0333333f;
                palette=palette_animation->data.palette;
                switch(palette_animation->action) {
                case 0:
                    i=palette_animation->last;
                    saved=palette[i];
                    for(i--;i>=palette_animation->first;i--) palette[i+1]=palette[i];
                    palette[palette_animation->first]=saved;
                    break;
                case 1:
                    i=palette_animation->first;
                    saved=palette[i];
                    for(i++;i<=palette_animation->last;i++) palette[i-1]=palette[i];
                    palette[palette_animation->last]=saved;
                    break;
                case 2:
                    offset=palette_animation->last-palette_animation->first+palette_animation->step+1;
                    for(i=palette_animation->first;i<=palette_animation->last;i++) {
                        saved=palette[i]; palette[i]=palette[i+offset]; palette[i+offset]=saved;
                    }
                    break;
                case 3:
                    offset=palette_animation->last-palette_animation->first+palette_animation->step+1;
                    for(i=palette_animation->first;i<=palette_animation->last;i++) {
                        saved=palette[i]; palette[i]=palette[i+offset]; palette[i+offset]=palette[i+offset+offset]; palette[i+offset+offset]=saved;
                    }
                    break;
                case 6:
                    PAL_SET(palette[palette_animation->first+2], 1985); PAL_SET(palette[palette_animation->first+1], 10561); PAL_SET(palette[palette_animation->first], 10241);
                    palette_animation->action=5; palette_animation->delay=10.0f;
                    break;
                case 5:
                    PAL_SET(palette[palette_animation->first+2], 321); PAL_SET(palette[palette_animation->first+1], 0xffc1); PAL_SET(palette[palette_animation->first], 10241);
                    palette_animation->action=4; palette_animation->delay=0.5f;
                    break;
                case 4:
                    PAL_SET(palette[palette_animation->first+2], 321); PAL_SET(palette[palette_animation->first+1], 10561); PAL_SET(palette[palette_animation->first], 0xf801);
                    palette_animation->action=6; palette_animation->delay=10.5f;
                    break;
                case 7:
                    if(palette_animation->step<10) PAL_SET(palette[palette_animation->step], 0xf001);
                    else if(palette_animation->step>=21) {
                        for(i=0;i<10;i++)
 PAL_SET(palette[i], 12421);
                        palette_animation->step=-1;
                    }
                    palette_animation->step++;
                    break;
                case 8:
                    if(palette_animation->step==0) PAL_SET(palette[palette_animation->step+7], 0xce73);
                    else if(palette_animation->step<8) {
                        PAL_SET(palette[palette_animation->step-1], 0xf141);
                        PAL_SET(palette[palette_animation->step+7], 0xce73);
                    } else if(palette_animation->step>=23) {
                        for(i=0;i<7;i++) {
 PAL_SET(palette[i], 12289);
 PAL_SET(palette[i+7], 4229);
 }
                        PAL_SET(palette[14], 4229); palette_animation->step=-1;
                    }
                    palette_animation->step++;
                    break;
                case 9:
                    palette_animation->first+=palette_animation->step*PC_SCROLL_RETRACES(gLastFrameCount, palette_animation->ticks);
                    if(palette_animation->first<0) palette_animation->first+=palette_animation->last;
                    else if(palette_animation->first>=palette_animation->last) palette_animation->first-=palette_animation->last;
                    displaylist_set_tile_size((Gfx *)palette,palette_animation->first>>2,-1,-1,-1);
                    break;
                case 10:
                    palette_animation->first+=palette_animation->step*PC_SCROLL_RETRACES(gLastFrameCount, palette_animation->ticks);
                    if(palette_animation->first<0) palette_animation->first+=palette_animation->last;
                    else if(palette_animation->first>=palette_animation->last) palette_animation->first-=palette_animation->last;
                    displaylist_set_tile_size((Gfx *)palette,-1,palette_animation->first>>2,-1,-1);
                    break;
                }
            }
            palette_animation++;
        } while(palette_animation->owner);
    }
    object_animation=TextureSwapLists[gTrackNum];
    if(object_animation && object_animation->count) {
        do {
            if(!(!expansion_pak_present && gTrackNum>=0 && gTrackNum<6 && object_animation->frames==texture_swap_shared_frames)) {
                object_animation->delay-=gFrameSeconds;
                if(object_animation->delay<=0.0f) {
                    object_animation->delay=object_animation->interval;
                    if(object_animation->direction) {
                        object_animation->index++;
                        if(object_animation->index>=object_animation->count) object_animation->index=0;
                    } else {
                        object_animation->index--;
                        if(object_animation->index<0) object_animation->index=object_animation->count-1;
                    }
                    object=object_animation->frames[object_animation->object_index].object;
                    if(object->flags&0x08000000) object->commands=object_animation->frames[object_animation->index].commands;
                    else displaylist_set_texture_image(N64PTR0(Gfx *, object->commands),object_animation->frames[object_animation->index].commands);
                }
            }
            object_animation++;
        } while(object_animation->count);
    }
}
