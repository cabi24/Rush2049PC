/* port/assets/gfx_n64.c: the N64's display-list commands (F3DEX2 2.08 and the RDP's), read into the PC's records
 * (port/include/pc_gfx_n64.h, pc_gfx_list.h; docs/port/GFX.md "The command records"). The encodings are the SDK's
 * documented ones; this is the only code that decodes them. */
#include <stdint.h>
#include <string.h>

#include "pc_gfx_n64.h"

_Static_assert(sizeof(Gfx) == 16 && _Alignof(Gfx) == 8, "a command record: 16 bytes, its pointers aligned");

/* The opcodes: a command's first byte. */
enum {
    N64_NOOP = 0x00, N64_VTX = 0x01, N64_MODIFYVTX = 0x02, N64_CULLDL = 0x03, N64_BRANCH_Z = 0x04, N64_TRI1 = 0x05,
    N64_TRI2 = 0x06, N64_QUAD = 0x07,
    N64_SPECIAL_3 = 0xD3, N64_SPECIAL_2 = 0xD4, N64_SPECIAL_1 = 0xD5, N64_DMA_IO = 0xD6, N64_TEXTURE = 0xD7,
    N64_POPMTX = 0xD8, N64_GEOMETRYMODE = 0xD9, N64_MTX = 0xDA, N64_MOVEWORD = 0xDB, N64_MOVEMEM = 0xDC,
    N64_LOAD_UCODE = 0xDD, N64_DL = 0xDE, N64_ENDDL = 0xDF, N64_SPNOOP = 0xE0, N64_RDPHALF_1 = 0xE1,
    N64_SETOTHERMODE_L = 0xE2, N64_SETOTHERMODE_H = 0xE3, N64_TEXRECT = 0xE4, N64_TEXRECTFLIP = 0xE5,
    N64_RDPLOADSYNC = 0xE6, N64_RDPPIPESYNC = 0xE7, N64_RDPTILESYNC = 0xE8, N64_RDPFULLSYNC = 0xE9,
    N64_SETKEYGB = 0xEA, N64_SETKEYR = 0xEB, N64_SETCONVERT = 0xEC, N64_SETSCISSOR = 0xED, N64_SETPRIMDEPTH = 0xEE,
    N64_RDPSETOTHERMODE = 0xEF, N64_LOADTLUT = 0xF0, N64_RDPHALF_2 = 0xF1, N64_SETTILESIZE = 0xF2,
    N64_LOADBLOCK = 0xF3, N64_LOADTILE = 0xF4, N64_SETTILE = 0xF5, N64_FILLRECT = 0xF6, N64_SETFILLCOLOR = 0xF7,
    N64_SETFOGCOLOR = 0xF8, N64_SETBLENDCOLOR = 0xF9, N64_SETPRIMCOLOR = 0xFA, N64_SETENVCOLOR = 0xFB,
    N64_SETCOMBINE = 0xFC, N64_SETTIMG = 0xFD, N64_SETZIMG = 0xFE, N64_SETCIMG = 0xFF,
};
/* G_MTX's parameters (the command holds them with the push bit inverted). */
enum { N64_MTX_PUSH = 1, N64_MTX_LOAD = 2, N64_MTX_PROJECTION = 4 };
/* G_MOVEWORD's and G_MOVEMEM's indices. */
enum { N64_MW_MATRIX = 0x00, N64_MW_NUMLIGHT = 0x02, N64_MW_FOG = 0x08, N64_MW_LIGHTCOL = 0x0A,
       N64_MW_PERSPNORM = 0x0E };
enum { N64_MV_VIEWPORT = 8, N64_MV_LIGHT = 10, N64_MV_MATRIX = 14 };
#define N64_LOD_MARKER 1   /* a G_SPNOOP's byte 1 in a model's level-of-detail marker (prepare_model_dl) */

/* bits bits of w from bit shift. */
#define FIELD(w, shift, bits) (((uint32_t)(w) >> (shift)) & ((1u << (bits)) - 1))

static void rectangle_texture(Gfx *c, uint32_t st, uint32_t steps) {
    c->op = GFX_OP_RECTANGLE_TEXTURE;
    c->rectangle_texture.s = (int16_t)(st >> 16);
    c->rectangle_texture.t = (int16_t)(st & 0xFFFF);
    c->rectangle_texture.dsdx = (int16_t)(steps >> 16);
    c->rectangle_texture.dtdy = (int16_t)(steps & 0xFFFF);
}

int pc_gfx_from_n64(Gfx *c, const GfxWords *prev, const GfxWords *cmd, const GfxWords *next) {
    uint32_t w0 = cmd->w0, v = (uint32_t)cmd->w1, op = w0 >> 24;
    void *p = (void *)cmd->w1;
    unsigned n, end, len;

    memset(c, 0, sizeof(*c));
    switch (op) {
    case N64_VTX:
        n = FIELD(w0, 12, 8);
        end = FIELD(w0, 1, 7);   /* one past the last slot */
        c->op = GFX_OP_VERTICES;
        if (n <= end) {          /* else a load past its end: nothing (count 0) */
            c->vertices.count = (uint8_t)n;
            c->vertices.first = (uint8_t)(end - n);
        }
        c->vertices.vertices = p;
        break;
    case N64_MODIFYVTX:
        c->op = GFX_OP_MODIFY_VERTEX;
        c->modify_vertex.what = (uint8_t)FIELD(w0, 16, 8);
        c->modify_vertex.vertex = (uint16_t)(FIELD(w0, 0, 16) >> 1);
        c->modify_vertex.value = v;
        break;
    case N64_CULLDL:
        c->op = GFX_OP_CULL_LIST;
        c->cull.first = (uint16_t)(FIELD(w0, 0, 16) >> 1);
        c->cull.last = (uint16_t)(FIELD(v, 0, 16) >> 1);
        break;
    case N64_BRANCH_Z:
        c->op = GFX_OP_BRANCH_Z;
        c->branch_z.vertex = (uint16_t)(FIELD(w0, 0, 12) >> 1);
        c->branch_z.z = (int32_t)v;
        if (prev != NULL && prev->w0 >> 24 == N64_RDPHALF_1)
            c->branch_z.list = (Gfx *)prev->w1;
        break;
    case N64_TRI1:
        c->op = GFX_OP_TRIANGLE;
        c->triangles.v[0] = (uint8_t)(FIELD(w0, 16, 8) >> 1);
        c->triangles.v[1] = (uint8_t)(FIELD(w0, 8, 8) >> 1);
        c->triangles.v[2] = (uint8_t)(FIELD(w0, 0, 8) >> 1);
        break;
    case N64_TRI2:
    case N64_QUAD:
        c->op = GFX_OP_TRIANGLES;
        c->triangles.v[0] = (uint8_t)(FIELD(w0, 16, 8) >> 1);
        c->triangles.v[1] = (uint8_t)(FIELD(w0, 8, 8) >> 1);
        c->triangles.v[2] = (uint8_t)(FIELD(w0, 0, 8) >> 1);
        c->triangles.v[3] = (uint8_t)(FIELD(v, 16, 8) >> 1);
        c->triangles.v[4] = (uint8_t)(FIELD(v, 8, 8) >> 1);
        c->triangles.v[5] = (uint8_t)(FIELD(v, 0, 8) >> 1);
        break;
    case N64_TEXTURE:
        c->op = GFX_OP_TEXTURE;
        c->texture.scale_s = (uint16_t)(v >> 16);
        c->texture.scale_t = (uint16_t)(v & 0xFFFF);
        c->texture.level = (uint8_t)FIELD(w0, 11, 3);
        c->texture.tile = (uint8_t)FIELD(w0, 8, 3);
        c->texture.on = (uint8_t)FIELD(w0, 1, 7);
        break;
    case N64_POPMTX:
        c->op = GFX_OP_POP_MATRIX;
        c->pop_matrix.count = v / 64;
        break;
    case N64_GEOMETRYMODE:
        c->op = GFX_OP_GEOMETRY_MODE;
        c->geometry_mode.keep = FIELD(w0, 0, 24);
        c->geometry_mode.set = v;
        break;
    case N64_MTX: {
        uint32_t params = FIELD(w0, 0, 8) ^ N64_MTX_PUSH;
        c->op = GFX_OP_MATRIX;
        c->matrix.projection = (params & N64_MTX_PROJECTION) != 0;
        c->matrix.load = (params & N64_MTX_LOAD) != 0;
        c->matrix.push = (params & N64_MTX_PUSH) != 0;
        c->matrix.matrix = p;
        break;
    }
    case N64_MOVEWORD: {
        uint32_t off = FIELD(w0, 0, 16);
        switch (FIELD(w0, 16, 8)) {
        case N64_MW_NUMLIGHT:
            c->op = GFX_OP_NUM_LIGHTS;
            c->num_lights.count = v / 24;
            break;
        case N64_MW_FOG:
            c->op = GFX_OP_FOG;
            c->fog.multiplier = (int16_t)(v >> 16);
            c->fog.offset = (int16_t)(v & 0xFFFF);
            break;
        case N64_MW_LIGHTCOL:
            c->op = GFX_OP_LIGHT_COLOR;
            c->light_color.light = (uint16_t)(off / 0x18);
            c->light_color.second = off % 0x18 != 0;
            c->light_color.r = (uint8_t)(v >> 24);
            c->light_color.g = (uint8_t)(v >> 16);
            c->light_color.b = (uint8_t)(v >> 8);
            break;
        case N64_MW_MATRIX:
            c->op = GFX_OP_INSERT_MATRIX;
            c->insert_matrix.offset = (uint16_t)off;
            c->insert_matrix.value = v;
            break;
        case N64_MW_PERSPNORM:
            c->op = GFX_OP_PERSP_NORMALIZE;
            c->persp_normalize.scale = (uint16_t)v;
            break;
        default:   /* a segment's base (no record has a segment), the clip ratio, the force-matrix flag: nothing */
            break;
        }
        break;
    }
    case N64_MOVEMEM:
        switch (FIELD(w0, 0, 8)) {
        case N64_MV_VIEWPORT:
            c->op = GFX_OP_VIEWPORT;
            c->viewport.viewport = p;
            break;
        case N64_MV_LIGHT:
            c->op = GFX_OP_LIGHT;
            c->light.slot = (uint8_t)(FIELD(w0, 8, 8) * 8 / 24);
            c->light.light = p;
            break;
        case N64_MV_MATRIX:
            c->op = GFX_OP_FORCE_MATRIX;
            c->matrix.matrix = p;
            break;
        default:
            break;
        }
        break;
    case N64_DL:
        c->op = FIELD(w0, 16, 8) == 0 ? GFX_OP_CALL_LIST : GFX_OP_BRANCH_LIST;
        c->list.list = (Gfx *)p;
        break;
    case N64_ENDDL:
        c->op = GFX_OP_END_LIST;
        break;
    case N64_SPNOOP:
        if (FIELD(w0, 16, 8) == N64_LOD_MARKER) {
            c->op = GFX_OP_SECTION;
            c->section.bit = (uint16_t)FIELD(w0, 0, 16);
            c->section.end = (Gfx *)p;
        }
        break;
    case N64_SETOTHERMODE_L:
    case N64_SETOTHERMODE_H:
        len = FIELD(w0, 0, 8) + 1;
        c->op = op == N64_SETOTHERMODE_L ? GFX_OP_OTHER_MODE_LOW : GFX_OP_OTHER_MODE_HIGH;
        c->other_mode.length = (uint8_t)(len > 32 ? 32 : len);
        c->other_mode.shift = (uint8_t)(32 - FIELD(w0, 8, 8) - len);
        c->other_mode.bits = v;
        break;
    case N64_RDPSETOTHERMODE:
        c->op = GFX_OP_OTHER_MODES;
        c->other_modes.high = FIELD(w0, 0, 24);
        c->other_modes.low = v;
        break;
    case N64_TEXRECT:
    case N64_TEXRECTFLIP:
    case N64_FILLRECT:
        c->op = op == N64_FILLRECT ? GFX_OP_FILL_RECTANGLE : GFX_OP_TEXTURE_RECTANGLE;
        c->rectangle.lrx = (uint16_t)FIELD(w0, 12, 12);
        c->rectangle.lry = (uint16_t)FIELD(w0, 0, 12);
        c->rectangle.ulx = (uint16_t)FIELD(v, 12, 12);
        c->rectangle.uly = (uint16_t)FIELD(v, 0, 12);
        if (op != N64_FILLRECT) {
            c->rectangle.tile = (uint8_t)FIELD(v, 24, 3);
            c->rectangle.flip = op == N64_TEXRECTFLIP;
        }
        break;
    case N64_RDPHALF_1:   /* a texture rectangle's coordinates; else a G_BRANCH_Z's list, which the branch holds */
        if (prev != NULL && (prev->w0 >> 24 == N64_TEXRECT || prev->w0 >> 24 == N64_TEXRECTFLIP))
            rectangle_texture(c, v, next != NULL && next->w0 >> 24 == N64_RDPHALF_2 ? (uint32_t)next->w1 : 0);
        break;
    case N64_SETSCISSOR:
        c->op = GFX_OP_SCISSOR;
        c->scissor.ulx = (uint16_t)FIELD(w0, 12, 12);
        c->scissor.uly = (uint16_t)FIELD(w0, 0, 12);
        c->scissor.lrx = (uint16_t)FIELD(v, 12, 12);
        c->scissor.lry = (uint16_t)FIELD(v, 0, 12);
        c->scissor.mode = (uint8_t)FIELD(v, 24, 2);
        break;
    case N64_SETPRIMDEPTH:
        c->op = GFX_OP_PRIM_DEPTH;
        c->prim_depth.z = (uint16_t)(v >> 16);
        c->prim_depth.dz = (uint16_t)v;
        break;
    case N64_LOADTLUT:
        c->op = GFX_OP_LOAD_TLUT;
        c->load_tlut.tile = (uint8_t)FIELD(v, 24, 3);
        c->load_tlut.first = (uint16_t)(FIELD(w0, 12, 12) >> 2);
        c->load_tlut.row = (uint16_t)(FIELD(w0, 0, 12) >> 2);
        c->load_tlut.last = (uint16_t)(FIELD(v, 12, 12) >> 2);
        break;
    case N64_SETTILESIZE:
    case N64_LOADTILE:
        c->op = op == N64_SETTILESIZE ? GFX_OP_TILE_SIZE : GFX_OP_LOAD_TILE;
        c->tile_rect.tile = (uint8_t)FIELD(v, 24, 3);
        c->tile_rect.uls = (uint16_t)FIELD(w0, 12, 12);
        c->tile_rect.ult = (uint16_t)FIELD(w0, 0, 12);
        c->tile_rect.lrs = (uint16_t)FIELD(v, 12, 12);
        c->tile_rect.lrt = (uint16_t)FIELD(v, 0, 12);
        break;
    case N64_LOADBLOCK:
        c->op = GFX_OP_LOAD_BLOCK;
        c->load_block.tile = (uint8_t)FIELD(v, 24, 3);
        c->load_block.uls = (uint16_t)FIELD(w0, 12, 12);
        c->load_block.ult = (uint16_t)FIELD(w0, 0, 12);
        c->load_block.lrs = (uint16_t)FIELD(v, 12, 12);
        c->load_block.dxt = (uint16_t)FIELD(v, 0, 12);
        break;
    case N64_SETTILE:
        c->op = GFX_OP_SET_TILE;
        c->set_tile.format = (uint8_t)FIELD(w0, 21, 3);
        c->set_tile.size = (uint8_t)FIELD(w0, 19, 2);
        c->set_tile.line = (uint16_t)FIELD(w0, 9, 9);
        c->set_tile.tmem = (uint16_t)FIELD(w0, 0, 9);
        c->set_tile.tile = (uint8_t)FIELD(v, 24, 3);
        c->set_tile.palette = (uint8_t)FIELD(v, 20, 4);
        c->set_tile.cmt = (uint8_t)FIELD(v, 18, 2);
        c->set_tile.maskt = (uint8_t)FIELD(v, 14, 4);
        c->set_tile.shiftt = (uint8_t)FIELD(v, 10, 4);
        c->set_tile.cms = (uint8_t)FIELD(v, 8, 2);
        c->set_tile.masks = (uint8_t)FIELD(v, 4, 4);
        c->set_tile.shifts = (uint8_t)FIELD(v, 0, 4);
        break;
    case N64_SETFILLCOLOR:
        c->op = GFX_OP_FILL_COLOR;
        c->fill_color.pattern = v;
        break;
    case N64_SETFOGCOLOR:
    case N64_SETBLENDCOLOR:
    case N64_SETPRIMCOLOR:
    case N64_SETENVCOLOR:
        c->op = op == N64_SETFOGCOLOR ? GFX_OP_FOG_COLOR : op == N64_SETBLENDCOLOR ? GFX_OP_BLEND_COLOR
              : op == N64_SETPRIMCOLOR ? GFX_OP_PRIM_COLOR : GFX_OP_ENV_COLOR;
        c->color.r = (uint8_t)(v >> 24);
        c->color.g = (uint8_t)(v >> 16);
        c->color.b = (uint8_t)(v >> 8);
        c->color.a = (uint8_t)v;
        if (op == N64_SETPRIMCOLOR) {
            c->color.min_level = (uint8_t)FIELD(w0, 8, 5);
            c->color.lod_fraction = (uint8_t)FIELD(w0, 0, 8);
        }
        break;
    case N64_SETCOMBINE:
        c->op = GFX_OP_COMBINE;
        c->combine.cycle[0].rgb_a = FIELD(w0, 20, 4);
        c->combine.cycle[0].rgb_c = FIELD(w0, 15, 5);
        c->combine.cycle[0].alpha_a = FIELD(w0, 12, 3);
        c->combine.cycle[0].alpha_c = FIELD(w0, 9, 3);
        c->combine.cycle[1].rgb_a = FIELD(w0, 5, 4);
        c->combine.cycle[1].rgb_c = FIELD(w0, 0, 5);
        c->combine.cycle[0].rgb_b = FIELD(v, 28, 4);
        c->combine.cycle[1].rgb_b = FIELD(v, 24, 4);
        c->combine.cycle[1].alpha_a = FIELD(v, 21, 3);
        c->combine.cycle[1].alpha_c = FIELD(v, 18, 3);
        c->combine.cycle[0].rgb_d = FIELD(v, 15, 3);
        c->combine.cycle[0].alpha_b = FIELD(v, 12, 3);
        c->combine.cycle[0].alpha_d = FIELD(v, 9, 3);
        c->combine.cycle[1].rgb_d = FIELD(v, 6, 3);
        c->combine.cycle[1].alpha_b = FIELD(v, 3, 3);
        c->combine.cycle[1].alpha_d = FIELD(v, 0, 3);
        break;
    case N64_SETTIMG:
    case N64_SETCIMG:
        c->op = op == N64_SETTIMG ? GFX_OP_TEXTURE_IMAGE : GFX_OP_COLOR_IMAGE;
        c->image.format = (uint8_t)FIELD(w0, 21, 3);
        c->image.size = (uint8_t)FIELD(w0, 19, 2);
        c->image.width = (uint16_t)(FIELD(w0, 0, 12) + 1);
        c->image.image = p;
        break;
    case N64_SETZIMG:
        c->op = GFX_OP_DEPTH_IMAGE;
        c->image.image = p;
        break;
    case N64_NOOP:
    case N64_RDPHALF_2:     /* the texture rectangle's steps: the record of the G_RDPHALF_1 before it holds them */
    case N64_LOAD_UCODE:    /* the microcode is F3DEX2's alone */
    case N64_DMA_IO:
    case N64_SPECIAL_1:
    case N64_SPECIAL_2:
    case N64_SPECIAL_3:
    case N64_RDPLOADSYNC:   /* the RDP's pipeline syncs: nothing to wait for */
    case N64_RDPPIPESYNC:
    case N64_RDPTILESYNC:
    case N64_RDPFULLSYNC:
    case N64_SETKEYGB:      /* the chroma key and the YUV conversion: not emulated */
    case N64_SETKEYR:
    case N64_SETCONVERT:
        break;
    default:
        return 1;
    }
    return 0;
}

void pc_gfx_list_from_n64(Gfx *out, const GfxWords *in, int n) {
    for (int i = 0; i < n; i++)
        pc_gfx_from_n64(&out[i], i > 0 ? &in[i - 1] : NULL, &in[i], i + 1 < n ? &in[i + 1] : NULL);
}
