/* include/game/blit.h: the 2D blitter and panes.
 *
 * Two layers (docs/code/ui.md): the blits (tBlit, MAT3 blit.c), the game's sprites with a texture, a position and an
 * animation function, each mirrored into an entry of the zoid blit list (tBlitInstance, MAT3 zoidblit.c); and the
 * blitter (MAT3 blit_pc.c), which turns that list into RDP commands in a display list of its own each frame
 * (BlitInit ... BlitFinish), its state in blitAttribute, the scissor box and the current mode. Panes (tBlitSet,
 * MAT3 pane.c) are bordered rectangles made of five blits. */
#ifndef GAME_BLIT_H
#define GAME_BLIT_H

#include "game/types.h"

/* The blitter's state bits (blitAttribute; BlitSetAttribute, BlitClearAttribute). */
#define BLIT_ATTR_ALPHA_COMPARE 0x0001 /* the RDP's alpha threshold: transparent texels are not drawn */
#define BLIT_ATTR_FLIP_X        0x0004 /* the texture mirrored left to right */
#define BLIT_ATTR_FLIP_Y        0x0008 /* the texture mirrored top to bottom */
#define BLIT_ATTR_ZDIST         0x0010 /* a depth from the primitive depth (BlitZDist) */
#define BLIT_ATTR_XLU           0x0020 /* translucent: the primitive colour's alpha (BlitXLU) */
#define BLIT_ATTR_FILL          0x4000 /* a filled rectangle (BlitRect) */
#define BLIT_ATTR_TALL          0x8000 /* the screen is taller than 220 lines: textures drawn at double height */

/* A tBlitInstance's flags (the zoid blit list). */
#define BLITINST_ALPHA_COMPARE 0x01 /* drawn with BLIT_ATTR_ALPHA_COMPARE */
#define BLITINST_FLIP          0x04 /* mirrored left to right (ZOID_FlipBlit) */
#define BLITINST_UPRIGHT       0x08 /* drawn without BLIT_ATTR_FLIP_Y (the textures are stored bottom row first) */
#define BLITINST_TEXT          0x80 /* no texture: the renderer calls its texture field instead (ZOID_TextBlit) */

/* A tBlitInstance's hide: the entry is free (ZOID_RemoveBlit; ZOID_AddBlit reuses it). */
#define BLITINST_FREE 2

/* The blitter's modes (blit_set_mode, blitMode; -1 after BlitInit: none set yet). */
#define BLIT_MODE_COPY            0 /* copy mode: RGBA and CI texels straight to the frame buffer */
#define BLIT_MODE_TEXTURE         1 /* one cycle: RGBA and CI textures, when translucent, depth-tested or tall */
#define BLIT_MODE_INTENSITY       2 /* one cycle: I and IA textures tinted by the primitive colour (fonts) */
#define BLIT_MODE_INTENSITY_ALPHA 3 /* as 2, with the alpha threshold (BLIT_ATTR_ALPHA_COMPARE) */
#define BLIT_MODE_TWO_TILE        4 /* two cycles, the environment colour's alpha 0x55: CI8 with a 4-bit second tile */

#define MAX_BLITS 200      /* the blit pool (InitBlits) and the zoid blit list */
#define MAX_PANES 16       /* gPaneBlits */
#define PANE_BLITS 5       /* the blits of a pane (tBlitSet.blits) */

/* Data */
extern u8 default_blit_image[EXTENT(4)];
extern tBlitSet gPaneBlits[EXTENT(16)];
extern BlitColor blitPrimColor;
extern u16 rumble_duty_masks[EXTENT(10)];
extern u32 blitAttribute;
extern s32 blitXmin;
extern s32 blitXmax;
extern s32 blitYmin;
extern s32 blitYmax;
extern s16 blitZDist;
extern s32 blitLUT;
extern void *blit_cache_image;
extern s64 blit_cache_tile;
extern s32 blitDLnum;
extern u8 *blitColors;
extern s32 gMaxBlitInsts;
extern tBlitInstance gBlitInstanceList[EXTENT(200)];
extern tBlit *BlitPtrs[EXTENT(200)];
extern s32 NumBlits;
extern s32 blitMode;
extern OSMesgQueueWords rumble_queue;
extern OSMesg rumble_msg;
extern s32 gNumBlitInsts;
/* pc_text_dl: declared by its files (src/blob/groups/blit/init.c: defines it) */

/* Functions */
s32 are_sound_voices_idle(void);
void audio_prenmi(void);
void blit_load_texture(void *img, u16 type, u16 siz, u16 width, u16 height, u16 uls, u16 ult, u16 lrs, u16 lrt, u16 pal, s32 tile);
s32 blit_mask_bits(s32 size);
void blit_set_mode(s32 mode);
void BlitClearAttribute(u32 mask);
void BlitClearColors(void);
void BlitColors(u8 *colors, s32 format, s32 texel_size, s32 palette_bank);
void BlitDraw(s32 x, s32 y, s32 right, s32 bottom, s32 s, s32 t);
void *BlitFinish(void);
void BlitInit(void);
void BlitRect(s32 x0, s32 y0, s32 x1, s32 y1, u8 *color);
void BlitScissor(s32 ulx, s32 uly, s32 lrx, s32 lry);
void BlitSetAttribute(u32 mask);
void BlitXLU(u16 alpha);
void BlitZDist(u16 depth);
u32 CalcChecksumInternal(u8 *data, u32 size);
void ChangePaneColor(tBlitSet *pane, u8 *paneColor, u8 *borderColor);
void ChangePaneSize(tBlitSet *pane, s32 x1, s32 y1, s32 x2, s32 y2);
void ControlChoice(s32 arg0, s32 arg1);
void ControlPakErrNQA(s32 arg1, s32 arg2);
void DrawBlits(void);
void DrawChoice(s32 row, s32 col, intptr_t text, char *a, char *b, s32 sel);
void DrawChoice3(s32 row, s32 col, u8 *text, char *s1, char *s2, char *s3, s32 sel1, s32 sel2);
void DrawPakErrNQA(s32 row, s32 col, u8 *text);
void free_car_palette(s32 index);
void *game_memmove(void *dst, const void *src, u32 n);
u8 *game_strcat(u8 *dst, u8 *src);
u8 *game_strcpy(u8 *dst, u8 *src);
tBlitSet *InitPane(s32 x1, s32 y1, s32 x2, s32 y2, s32 alpha, s32 (*animFunc)(tBlit *), u32 animID, s32 permanent);
void InitBlit(tBlit *blit);
void InitBlits(void);
void move_blit_chain(tBlit *blit, s16 dx, s16 dy);
tBlit *NewBlit(const char *name, s32 x, s32 y, s32 flags);
tBlit *NewMultiBlit(s16 ulx, s16 uly, const tBlitDef *mblit, s16 nblits);
void noop_two_args(s32 unused0, s32 unused1);
s32 pak_removed(s32 pak);
void RemoveBlit(tBlit *blit);
void RemovePane(tBlitSet *pane);
void RemovePanes(s32 permanent);
void RemovePlayerFile(uintptr_t fileh);
void RenameBlit(tBlit *blit, char *name, s32 preserve);
void rumble_thread_entry(void *arg);
void SaveRaceSetting(tProfile **player, u8 option, s8 value, u8 track);
void SelectBlit(tBlit *blt, s32 index, s32 hSize, s32 vSize);
void si_lock(void);
void si_unlock(void);
void stop_sequence(s32 h);
void UpdateBlit(tBlit *blit);
s32 ZOID_AddBlit(s32 texIndex, void *texture, void *palette, s32 x, s32 y, s32 width, s32 height);
void ZOID_CropBlit(s32 index, s32 top, s32 bot, s32 left, s32 right);
void ZOID_FlipBlit(s32 index, s32 flip);
void ZOID_RemoveBlit(s32 index);
void ZOID_SetBlitData(s32 index, s32 hide, s32 alpha, u16 z);
void ZOID_TextBlit(s32 index, s32 text);
void ZOID_UpdateBlit(u32 index, u32 texIndex, void *texture, void *palette, u32 x, u32 y, u32 width, u32 height);

#endif /* GAME_BLIT_H */
