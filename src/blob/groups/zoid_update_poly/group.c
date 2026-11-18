/* zoid_update_poly/group.c: set a flat polygon's corners, texture coordinates and colour (MAT3 zoidpoly.c,
 * ZOID_UpdatePoly()).
 *
 * The N64's version gives the corners texture coordinates in the texture's own texels (10.5 fixed point, 32 a
 * texel) from its size, where MAT3 writes 0.01 and 0.99. */
#include "game/game.h"

/* Update polygon `poly`: its vertex count (`nverts`, 0 keeps it), its flags (`flags`, 0 keeps them), its texture
 * (`texidx` if `useTidx`, which sets POLY_TEXTURED), then for each vertex what is given:
 * - texture coordinates if it is textured: vertex i is corner i (corners 4-7, the corners mirrored, when the track
 *   is mirrored, gMirrorWaves, and the polygon has POLY_MIRRORED). A texture clamped in s (TEX_CLAMP_S) spans s
 *   from 0 to its width, a wrapping one from width - 1/2 to 2 width - 1/2 texels; likewise t (TEX_CLAMP_T);
 * - the position from xyz[i] (NULL keeps them): x and y as they are for a screen-space polygon (POLY_SCREEN), else
 *   x, y and z in feet times 16, the scene's units;
 * - the colour *color (bytes red, green, blue, alpha; NULL keeps it). */
void ZOID_UpdatePoly(tPoly *poly, int nverts, tVec3 *xyz, u16 texidx, u8 *color, u16 flags, int useTidx)
{
    tTexture *texture;
    int i, corner;
    if (!nverts) nverts = poly->nverts;
    else poly->nverts = nverts;
    if (flags) poly->flags = flags;
    if (useTidx) {
        poly->texidx = texidx;
        poly->flags |= POLY_TEXTURED;
    }
    if (poly->flags & POLY_TEXTURED) {
        texture = &gTexDefPtrs[poly->texidx >> 10].texDefs[poly->texidx & 0x3FF];
    } else texture = 0;
    for (i = 0; i < nverts; i++) {
        if (texture) {
            corner = i;
            if (gMirrorWaves && (poly->flags & POLY_MIRRORED)) corner = i + 4;
            switch (corner) {
            case 0:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = texture->width << 5;
                else poly->verts[i].tc[0] = (texture->width << 6) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = texture->height << 5;
                else poly->verts[i].tc[1] = (texture->height << 6) - 16;
                break;
            case 1:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = 0;
                else poly->verts[i].tc[0] = (texture->width << 5) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = texture->height << 5;
                else poly->verts[i].tc[1] = (texture->height << 6) - 16;
                break;
            case 2:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = 0;
                else poly->verts[i].tc[0] = (texture->width << 5) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = 0;
                else poly->verts[i].tc[1] = (texture->height << 5) - 16;
                break;
            case 3:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = texture->width << 5;
                else poly->verts[i].tc[0] = (texture->width << 6) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = 0;
                else poly->verts[i].tc[1] = (texture->height << 5) - 16;
                break;
            case 4:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = 0;
                else poly->verts[i].tc[0] = (texture->width << 5) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = texture->height << 5;
                else poly->verts[i].tc[1] = (texture->height << 6) - 16;
                break;
            case 5:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = texture->width << 5;
                else poly->verts[i].tc[0] = (texture->width << 6) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = texture->height << 5;
                else poly->verts[i].tc[1] = (texture->height << 6) - 16;
                break;
            case 6:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = texture->width << 5;
                else poly->verts[i].tc[0] = (texture->width << 6) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = 0;
                else poly->verts[i].tc[1] = (texture->height << 5) - 16;
                break;
            case 7:
                if (texture->flags & TEX_CLAMP_S) poly->verts[i].tc[0] = 0;
                else poly->verts[i].tc[0] = (texture->width << 5) - 16;
                if (texture->flags & TEX_CLAMP_T) poly->verts[i].tc[1] = 0;
                else poly->verts[i].tc[1] = (texture->height << 5) - 16;
                break;
            }
        }
        if (xyz) {
            if (poly->flags & POLY_SCREEN) {
                poly->verts[i].ob[0] = xyz[i].x;
                poly->verts[i].ob[1] = xyz[i].y;
            } else {
                poly->verts[i].ob[0] = xyz[i].x * 16.0f;
                poly->verts[i].ob[1] = xyz[i].y * 16.0f;
                poly->verts[i].ob[2] = xyz[i].z * 16.0f;
            }
        }
        if (color) poly->verts[i].cn.rgba = *(u32 *)color;
    }
}
