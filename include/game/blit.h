/* include/game/blit.h: the 2D blitter and panes */
#ifndef GAME_BLIT_H
#define GAME_BLIT_H

#include "game/types.h"

/* Data */
extern u8 default_blit_image[EXTENT(4)];
extern PaneBlits gPaneBlits[EXTENT(16)];
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
extern struct Blit *BlitPtrs[EXTENT(200)];
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
s32 blit_mask_bits(s32 arg0);
void blit_set_mode(s32 mode);
void BlitClearAttribute(u32 mask);
void BlitClearColors(void);
void BlitColors(u8 *img, s32 mode, s32 pal, s32 bank);
void BlitDraw(s32 x, s32 y, s32 right, s32 bottom, s32 s, s32 t);
void *BlitFinish(void);
void BlitInit(void);
void BlitRect(s32 x0, s32 y0, s32 x1, s32 y1, u8 *color);
void BlitScissor(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void BlitSetAttribute(u32 mask);
void BlitXLU(u16 arg0);
void BlitZDist(u16 arg0);
u32 CalcChecksumInternal(u8 *data, u32 size);
void ChangePaneColor(void *arg0, void *arg1, void *arg2);
void ControlChoice(s32 arg0, s32 arg1);
void ControlPakErrNQA(s32 arg1, s32 arg2);
void DrawBlits(void);
void DrawChoice(s32 row, s32 col, intptr_t text, char *a, char *b, s32 sel);
void DrawChoice3(s32 row, s32 col, u8 *text, char *s1, char *s2, char *s3, s32 sel1, s32 sel2);
void DrawPakErrNQA(s32 row, s32 col, u8 *text);
void free_car_palette(s32 index);
void *game_memmove(void *dst, const void *src, u32 n);
u8 *game_strcat(u8 *arg0, u8 *arg1);
u8 *game_strcpy(u8 *arg0, u8 *arg1);
void InitBlit(struct Blit *blit);
void InitBlits(void);
void move_blit_chain(struct Config64 *config, s16 dx, s16 dy);
void noop_two_args(s32 arg0, s32 arg1);
s32 pak_removed(s32 arg0);
void RemovePane(s8 *arg0);
void RemovePanes(s32 arg0);
void RemovePlayerFile(uintptr_t identifier);
void RenameBlit(struct Blit *blit, char *name, s32 preserve);
void rumble_thread_entry(void *arg);
void SaveRaceSetting(struct tPlayerProfile **player, u8 option, s8 value, u8 track);
void SelectBlit(struct Blit *blt, s32 index, s32 hSize, s32 vSize);
void si_lock(void);
void si_unlock(void);
void stop_sequence(s32 h);
void UpdateBlit(struct Sprite *spr);
s32 ZOID_AddBlit(s32 id, void *p4, void *p0, s32 a, s32 b, s32 w, s32 h);
void ZOID_CropBlit(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void ZOID_FlipBlit(s32 arg0, s32 arg1);
void ZOID_RemoveBlit(s32 arg0);
void ZOID_SetBlitData(s32 arg0, s32 arg1, s32 arg2, u16 arg3);
void ZOID_TextBlit(s32 arg0, s32 arg1);
void ZOID_UpdateBlit(u32 index, u32 h8, uintptr_t w4, uintptr_t w0, u32 hA, u32 hC, u32 h10, u32 h12);

#endif /* GAME_BLIT_H */
