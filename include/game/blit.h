/* include/game/blit.h: the 2D blitter and panes */
#ifndef GAME_BLIT_H
#define GAME_BLIT_H

#include "game/types.h"

/* Data */
extern u8 default_blit_image[EXTENT(4)];
extern PaneBlits gPaneBlits[EXTENT(16)];
extern BlitColor blitPrimColor;
extern u8 blitPrimColor_alpha;
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
extern s8 gPaks_inserted;
extern struct Blit *BlitPtrs[EXTENT(200)];
extern s32 NumBlits;
extern s32 blitMode;
extern OSMesgQueueWords rumble_queue;
extern OSMesg rumble_msg;
extern s32 gNumBlitInsts;
/* pc_text_dl: declared by its files (src/blob/groups/blit/init.c: defines it) */
#endif /* GAME_BLIT_H */
