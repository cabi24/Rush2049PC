/* port/include/pc_gfx_list.h: the display lists' commands, in the PC's own form (docs/port/GFX.md "The command
 * records").
 *
 * A display list is an array of Gfx, each a record of 16 bytes: what it does (op, a GfxOp) and its operands as named
 * fields, a pointer where it names a list, vertices, a matrix or an image. The game builds its lists so (the macros in
 * include/game/gbi.h); the WAD converter builds the files' lists so when a file loads, a record for each of the file's
 * F3DEX2 commands (port/assets/wad.c, gfx_n64.c); the renderer (port/gfx) reads them, and nothing else. The values
 * keep the units the RDP and the microcode work in (quarter pixels, 10.5 texture coordinates, the other modes' bits),
 * which the renderer takes as the N64 did, so what is drawn is what was drawn.
 *
 * Every record's first byte is its op; the union's `op` reads it whatever the record. A record is written whole, as one
 * of the union's members (a compound literal), so its unused bytes are zeros. */
#ifndef PC_GFX_LIST_H
#define PC_GFX_LIST_H

#include <stdint.h>

/* What a command does. 0 is a no-op, as a zeroed record. */
typedef enum GfxOp {
    GFX_OP_NOOP = 0,            /* nothing: the RDP's pipeline syncs, a file's no-ops and the halves folded into others */

    /* The lists: calls, branches, their end */
    GFX_OP_CALL_LIST,           /* list: go into it, and on with the next record at its end */
    GFX_OP_BRANCH_LIST,         /* list: go on there */
    GFX_OP_END_LIST,            /* back to the list that called this one, or the task's end */
    GFX_OP_CULL_LIST,           /* cull: end the list when vertices first..last are all past one side of the view */
    GFX_OP_BRANCH_Z,            /* branch_z: go on at list when a vertex is at least that near */
    GFX_OP_SECTION,             /* section: a file's level-of-detail marker (prepare_model_dl); nothing to draw */

    /* The geometry (the RSP's) */
    GFX_OP_VERTICES,            /* vertices: load count vertices into the vertex buffer's slots first.. */
    GFX_OP_MODIFY_VERTEX,       /* modify_vertex: set one attribute of a loaded vertex */
    GFX_OP_TRIANGLE,            /* triangles: one triangle, v[0..2] */
    GFX_OP_TRIANGLES,           /* triangles: two triangles, v[0..2] and v[3..5] */
    GFX_OP_TEXTURE,             /* texture: the texture coordinates' scale, the tile, texturing on or off */
    GFX_OP_GEOMETRY_MODE,       /* geometry_mode: the geometry mode's bits kept and set */
    GFX_OP_MATRIX,              /* matrix: load or multiply the modelview or the projection */
    GFX_OP_POP_MATRIX,          /* pop_matrix: pop count modelview matrices */
    GFX_OP_FORCE_MATRIX,        /* matrix: load the modelview x projection product itself */
    GFX_OP_INSERT_MATRIX,       /* insert_matrix: write one word of the modelview x projection product */
    GFX_OP_VIEWPORT,            /* viewport */
    GFX_OP_LIGHT,               /* light: a light, or the look-at direction, into its slot */
    GFX_OP_NUM_LIGHTS,          /* num_lights: how many directional lights there are */
    GFX_OP_LIGHT_COLOR,         /* light_color: one light's colour */
    GFX_OP_FOG,                 /* fog: the fog's multiplier and offset */
    GFX_OP_PERSP_NORMALIZE,     /* persp_normalize: the microcode's w scale for its fixed point; nothing on the PC */

    /* The RDP's state */
    GFX_OP_OTHER_MODE_HIGH,     /* other_mode: a field of the other modes' high word */
    GFX_OP_OTHER_MODE_LOW,      /* other_mode: a field of the low word (the render mode, alpha compare, depth source) */
    GFX_OP_OTHER_MODES,         /* other_modes: both words */
    GFX_OP_COMBINE,             /* combine: the colour combiner's two cycles */
    GFX_OP_FILL_COLOR,          /* fill_color */
    GFX_OP_FOG_COLOR,           /* color */
    GFX_OP_BLEND_COLOR,         /* color */
    GFX_OP_PRIM_COLOR,          /* color, with the primitive's level of detail */
    GFX_OP_ENV_COLOR,           /* color */
    GFX_OP_PRIM_DEPTH,          /* prim_depth: the depth of what is drawn with the primitive depth source */
    GFX_OP_SCISSOR,             /* scissor */
    GFX_OP_COLOR_IMAGE,         /* image: where the frame is drawn */
    GFX_OP_DEPTH_IMAGE,         /* image: the depth buffer */

    /* Textures: the image texels are loaded from, the tiles, the loads into texture memory */
    GFX_OP_TEXTURE_IMAGE,       /* image */
    GFX_OP_SET_TILE,            /* set_tile: a tile's format and place in texture memory, its clamp, mirror and mask */
    GFX_OP_TILE_SIZE,           /* tile_rect: a tile's rectangle of texels */
    GFX_OP_LOAD_TILE,           /* tile_rect: load a rectangle of the texture image into a tile (and set its size) */
    GFX_OP_LOAD_BLOCK,          /* load_block: load a run of texels into a tile */
    GFX_OP_LOAD_TLUT,           /* load_tlut: load palette entries into texture memory */

    /* Rectangles */
    GFX_OP_FILL_RECTANGLE,      /* rectangle */
    GFX_OP_TEXTURE_RECTANGLE,   /* rectangle: a textured rectangle; its texture coordinates are the next record's */
    GFX_OP_RECTANGLE_TEXTURE,   /* rectangle_texture: the texture coordinates of the textured rectangle before it */

    GFX_OP_COUNT
} GfxOp;

/* The colour combiner's setting for one cycle: colour = (rgb_a - rgb_b) * rgb_c + rgb_d, and alpha likewise, each a
 * selector of the combiner's inputs (include/game/gbi.h's G_CCMUX_* and G_ACMUX_*, the RDP's numbers). The 16
 * selectors of the two cycles take each the width the RDP gives it, so a combine record holds them in 16 bytes. */
typedef struct GfxCombineCycle {
    unsigned int rgb_a : 4, rgb_b : 4, rgb_c : 5, rgb_d : 3;
    unsigned int alpha_a : 3, alpha_b : 3, alpha_c : 3, alpha_d : 3;
} GfxCombineCycle;

/* One command. Coordinates are the RDP's: the screen's in 10.2 fixed point (quarter pixels), a tile's in quarter
 * texels, a rectangle's texture coordinates in s10.5 (1/32 texel) and its steps in s5.10 (1/1024 texel a pixel). */
typedef union Gfx {
    uint8_t op;                         /* GfxOp, in every record */

    struct {                            /* CALL_LIST, BRANCH_LIST */
        uint8_t op;
        uint8_t unused[7];
        union Gfx *list;
    } list;
    struct {                            /* CULL_LIST: vertex slots first..last (last is held to the buffer's 32) */
        uint8_t op;
        uint8_t unused;
        uint16_t first, last;
        uint8_t unused2[10];
    } cull;
    struct {                            /* BRANCH_Z: when the vertex in slot `vertex` is at screen depth z or nearer
                                         * (z in 16.16: the viewport's depth units), go on at list */
        uint8_t op;
        uint8_t unused;
        uint16_t vertex;
        int32_t z;
        union Gfx *list;
    } branch_z;
    struct {                            /* SECTION: a level-of-detail marker in a model's list (prepare_model_dl):
                                         * when the race doesn't keep it (bit `bit` of its keep mask, modulo 32), it
                                         * becomes a branch to end, where its section ends */
        uint8_t op;
        uint8_t unused;
        uint16_t bit;
        uint8_t unused2[4];
        union Gfx *end;
    } section;

    struct {                            /* VERTICES: Vtx records (16 bytes each) into slots first..first+count-1 */
        uint8_t op;
        uint8_t count, first;
        uint8_t unused[5];
        void *vertices;
    } vertices;
    struct {                            /* MODIFY_VERTEX: what is a G_MWO_POINT_* (RGBA, ST, XYSCREEN, ZSCREEN) */
        uint8_t op;
        uint8_t what;
        uint16_t vertex;
        uint32_t value;
        uint8_t unused[8];
    } modify_vertex;
    struct {                            /* TRIANGLE, TRIANGLES: the vertices' slots, counterclockwise for the front */
        uint8_t op;
        uint8_t v[6];
        uint8_t unused[9];
    } triangles;
    struct {                            /* TEXTURE: scale_s, scale_t in 0.16 (0xFFFF: 1), the tile, its mipmap's last
                                         * level, texturing on (nonzero) */
        uint8_t op;
        uint8_t level, tile, on;
        uint16_t scale_s, scale_t;
        uint8_t unused[8];
    } texture;
    struct {                            /* GEOMETRY_MODE: mode = (mode & keep) | set */
        uint8_t op;
        uint8_t unused[3];
        uint32_t keep, set;
        uint8_t unused2[4];
    } geometry_mode;
    struct {                            /* MATRIX: an Mtx (the fixed-point matrix guMtxF2L makes), to the projection or
                                         * the modelview; loaded or multiplied in; the modelview pushed first.
                                         * FORCE_MATRIX: the Mtx is the modelview x projection product (the flags 0) */
        uint8_t op;
        uint8_t projection, load, push;
        uint8_t unused[4];
        void *matrix;
    } matrix;
    struct {                            /* POP_MATRIX */
        uint8_t op;
        uint8_t unused[3];
        uint32_t count;
        uint8_t unused2[8];
    } pop_matrix;
    struct {                            /* INSERT_MATRIX: the word at byte offset `offset` of the product's Mtx */
        uint8_t op;
        uint8_t unused;
        uint16_t offset;
        uint32_t value;
        uint8_t unused2[8];
    } insert_matrix;
    struct {                            /* VIEWPORT: a Vp (scale and translation, x and y in 10.2) */
        uint8_t op;
        uint8_t unused[7];
        void *viewport;
    } viewport;
    struct {                            /* LIGHT: a Light into slot `slot`: 0 the look-at x direction, 1 its y, 2 on
                                         * the lights (the last of them the ambient light) */
        uint8_t op;
        uint8_t slot;
        uint8_t unused[6];
        void *light;
    } light;
    struct {                            /* NUM_LIGHTS: the directional lights; the ambient one is the next slot */
        uint8_t op;
        uint8_t unused[3];
        uint32_t count;
        uint8_t unused2[8];
    } num_lights;
    struct {                            /* LIGHT_COLOR: light `light`'s colour, or its copy (second) */
        uint8_t op;
        uint8_t second;
        uint16_t light;
        uint8_t r, g, b;
        uint8_t unused[9];
    } light_color;
    struct {                            /* FOG: the shade alpha is z/w x multiplier + offset (the microcode's) */
        uint8_t op;
        uint8_t unused;
        int16_t multiplier, offset;
        uint8_t unused2[10];
    } fog;
    struct {                            /* PERSP_NORMALIZE */
        uint8_t op;
        uint8_t unused;
        uint16_t scale;
        uint8_t unused2[12];
    } persp_normalize;

    struct {                            /* OTHER_MODE_HIGH, OTHER_MODE_LOW: the word's bits shift..shift+length-1 become
                                         * those of bits (in place: bits holds the field shifted to `shift`) */
        uint8_t op;
        uint8_t shift, length;
        uint8_t unused;
        uint32_t bits;
        uint8_t unused2[8];
    } other_mode;
    struct {                            /* OTHER_MODES: both words */
        uint8_t op;
        uint8_t unused[3];
        uint32_t high, low;
        uint8_t unused2[4];
    } other_modes;
    struct {                            /* COMBINE: cycle[0] runs first in 2-cycle mode; 1-cycle mode runs cycle[1] */
        uint8_t op;
        uint8_t unused[3];
        GfxCombineCycle cycle[2];
        uint8_t unused2[4];
    } combine;
    struct {                            /* FILL_COLOR: the fill pattern: two RGBA 5551 pixels, or one RGBA 8888 */
        uint8_t op;
        uint8_t unused[3];
        uint32_t pattern;
        uint8_t unused2[8];
    } fill_color;
    struct {                            /* FOG_COLOR, BLEND_COLOR, PRIM_COLOR, ENV_COLOR; PRIM_COLOR's also the
                                         * primitive's minimum level and level-of-detail fraction (0-255) */
        uint8_t op;
        uint8_t min_level, lod_fraction;
        uint8_t unused;
        uint8_t r, g, b, a;
        uint8_t unused2[8];
    } color;
    struct {                            /* PRIM_DEPTH: z and its slope, the RDP's 16-bit depth */
        uint8_t op;
        uint8_t unused;
        uint16_t z, dz;
        uint8_t unused2[10];
    } prim_depth;
    struct {                            /* SCISSOR: the box drawn in, in 10.2; mode: interlacing (none on the PC) */
        uint8_t op;
        uint8_t mode;
        uint16_t ulx, uly, lrx, lry;
        uint8_t unused[6];
    } scissor;
    struct {                            /* COLOR_IMAGE, DEPTH_IMAGE, TEXTURE_IMAGE: the image, its format and texel
                                         * size (G_IM_FMT_*, G_IM_SIZ_*) and its width in texels (1-4096) */
        uint8_t op;
        uint8_t format, size;
        uint8_t unused;
        uint16_t width;
        uint8_t unused2[2];
        void *image;
    } image;

    struct {                            /* SET_TILE: tile (0-7)'s format and texel size, its line (64-bit words a row)
                                         * and place (tmem, in 64-bit words) in texture memory, its palette (a 16-entry
                                         * bank of the TLUT), and per axis its clamp and mirror (G_TX_*), mask (the
                                         * wrap's power of two; 0 none) and shift (1-10 divide, 11-15 multiply) */
        uint8_t op;
        uint8_t tile, format, size;
        uint16_t line, tmem;
        uint8_t palette;
        uint8_t cmt, maskt, shiftt;
        uint8_t cms, masks, shifts;
        uint8_t unused;
    } set_tile;
    struct {                            /* TILE_SIZE, LOAD_TILE: the tile's corners, upper left and lower right
                                         * inclusive, in quarter texels */
        uint8_t op;
        uint8_t tile;
        uint16_t uls, ult, lrs, lrt;
        uint8_t unused[6];
    } tile_rect;
    struct {                            /* LOAD_BLOCK: texels uls..lrs of row ult of the image, as one run into the
                                         * tile's place; dxt: the 1.11 step to the next row of the tile (its swapped
                                         * odd rows) */
        uint8_t op;
        uint8_t tile;
        uint16_t uls, ult, lrs, dxt;
        uint8_t unused[6];
    } load_block;
    struct {                            /* LOAD_TLUT: palette entries first..last of row `row` of the image (16-bit
                                         * colours) into the tile's place */
        uint8_t op;
        uint8_t tile;
        uint16_t first, row, last;
        uint8_t unused[8];
    } load_tlut;

    struct {                            /* FILL_RECTANGLE, TEXTURE_RECTANGLE: the corners in 10.2 (the lower right
                                         * exclusive in 1- and 2-cycle mode); a textured one's tile, flipped (s down,
                                         * t across) when flip is set */
        uint8_t op;
        uint8_t tile, flip;
        uint8_t unused;
        uint16_t ulx, uly, lrx, lry;
        uint8_t unused2[4];
    } rectangle;
    struct {                            /* RECTANGLE_TEXTURE: the texture coordinates at the upper left corner (s10.5)
                                         * and their steps a pixel across and down (s5.10) */
        uint8_t op;
        uint8_t unused;
        int16_t s, t, dsdx, dtdy;
        uint8_t unused2[6];
    } rectangle_texture;

    int64_t align;                      /* 8-byte alignment, for the pointers */
} Gfx;

#endif /* PC_GFX_LIST_H */
