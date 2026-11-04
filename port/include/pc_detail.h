/* port/include/pc_detail.h: the levels of video.draw_distance, detail, fog and texture_lod (docs/port/DETAIL.md), for
 * port/gfx and port/config (pc_port.h) and the game C (pc_game_types.h). No system headers. */
#ifndef PC_DETAIL_H
#define PC_DETAIL_H

#define PC_DRAW_ORIGINAL 0     /* the N64's distance limits, its far plane, its visibility tables */
#define PC_DRAW_FAR 1          /* each PC_DRAW_FAR_FACTOR times as far */
#define PC_DRAW_MAX 2          /* none: the whole track, every model at any distance */
#define PC_DETAIL_ORIGINAL 0   /* the level of detail the game chooses */
#define PC_DETAIL_HIGH 1       /* each model's most detailed level wherever it is drawn */
#define PC_FOG_ORIGINAL 0
#define PC_FOG_FAR 1           /* the fog's start and end PC_DRAW_FAR_FACTOR times as far */
#define PC_FOG_OFF 2

#define PC_TEXLOD_ORIGINAL 0   /* video.texture_lod: the mip level the N64 shows, from its own pixels */
#define PC_TEXLOD_SHARP 1      /* the level for the window's own pixels */
#define PC_TEXLOD_ANISO 2      /* that, and PC_TEXLOD_TAPS samples along a surface seen edge on */
#define PC_TEXLOD_TAPS 4

#define PC_DRAW_FAR_FACTOR 2.0f   /* far: the game's distance limits and its far plane times this; fog far: its fog */
#define PC_DRAW_MAX_DEPTH 32.0f   /* max: the depth range's far plane, times the game's */

#endif
