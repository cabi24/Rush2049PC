/* lookat_reflect_fill.c: the RSP's LookAt record for an environment-mapped object.
 *
 * The scene draw (zoid_draw_object) fills one per drawn node flagged for environment mapping (node flag 0x80000)
 * and turns on G_TEXTURE_GEN: the RSP then takes the texture coordinates from the vertex normals projected on the
 * two LookAt directions, so the texture follows the view like a reflection. This is the LookAt half of libultra's
 * guLookAtReflectF (no matrix). */
#include "game/game.h"

/* The RSP's Light_t and LookAt (libultra gbi.h): colours and an s8 direction (-128..127 for -1..1). */
typedef struct {
    u8 col[3];
    char pad1;
    u8 colc[3];
    char pad2;
    s8 dir[3];
    char pad3;
    char pad4[4];
} Light_t;

typedef struct LookAt {
    Light_t l[2];
} LookAt;

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
/* A direction component -1..1 as the RSP's s8 fraction (128 for 1, held at 127). */
#define FTOFRAC8(x) ((s32) MIN(((x) * 128.0f), 127.0f) & 0xFF)

/* Fill the LookAt l for an eye at (xEye, yEye, zEye) looking at (xAt, yAt, zAt) with (xUp, yUp, zUp) as up: light 0
 * gets the view's right direction, light 1 its up direction (both unit vectors, made orthogonal to the look
 * direction by cross products), as guLookAtReflectF computes them; the colours are libultra's (black, and green 0x80
 * for light 1). The last two bytes of light 1's colc are left as they are (libultra writes them too). */
void lookat_reflect_fill(LookAt *l, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt,
                     f32 xUp, f32 yUp, f32 zUp) {
    f32 len, xRight, yRight, zRight, xLook, yLook, zLook;


    xLook = xAt - xEye;
    yLook = yAt - yEye;
    zLook = zAt - zEye;
    len = -invsqr(xLook * xLook + yLook * yLook + zLook * zLook);
    xLook *= len;
    yLook *= len;
    zLook *= len;

    xRight = yUp * zLook - zUp * yLook;
    yRight = zUp * xLook - xUp * zLook;
    zRight = xUp * yLook - yUp * xLook;
    len = invsqr(xRight * xRight + yRight * yRight + zRight * zRight);
    xRight *= len;
    yRight *= len;
    zRight *= len;

    xUp = yLook * zRight - zLook * yRight;
    yUp = zLook * xRight - xLook * zRight;
    zUp = xLook * yRight - yLook * xRight;
    l->l[0].dir[0] = FTOFRAC8(xRight);
    l->l[0].dir[1] = FTOFRAC8(yRight);
    l->l[0].dir[2] = FTOFRAC8(zRight);
    l->l[1].dir[0] = FTOFRAC8(xUp);
    l->l[1].dir[1] = FTOFRAC8(yUp);
    l->l[1].dir[2] = FTOFRAC8(zUp);

    l->l[0].col[0] = 0;
    l->l[0].col[1] = 0;
    l->l[0].col[2] = 0;
    l->l[0].pad1 = 0;
    l->l[0].colc[0] = 0;
    l->l[0].colc[1] = 0;
    l->l[0].colc[2] = 0;
    l->l[0].pad2 = 0;
    l->l[1].col[0] = 0;
    l->l[1].col[1] = 0x80;
    l->l[1].col[2] = 0;
    l->l[1].pad1 = 0;
    l->l[1].colc[0] = 0;
    l->l[1].colc[1] = 0x80;
}
