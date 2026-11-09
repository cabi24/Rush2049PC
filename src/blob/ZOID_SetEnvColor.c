#include "game/game.h"

/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* ZOID_SetEnvColor: replacement source for the locked single. Struct form against the
 * ObjInstanceList table ObjInstanceList (MAT3 zoid.c / zoidtexture.c / visuals.c); matches standalone and in
 * the whole-program unit, and its inlined copies share one table base inside AnimateRocketFlame/AnimateFrame
 * (the m2c singles use &ObjInstanceList_mat/&D_8012E73C bases, which split that web). One statement per line:
 * as1 schedules by source line, a one-line body schedules the param home store differently. */
typedef union Color { unsigned char b[4]; unsigned int w; } Color;
typedef struct Node Node;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInstance {
    unsigned int flags;         /* 0x00 */
    unsigned int w4;
    Node *mat;                  /* 0x08 */
    float scale, w;             /* 0x10 */
    unsigned short index;       /* 0x18 */
    short child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;   /* 0x20 */
    unsigned int color;         /* 0x60 */
    unsigned int envcolor;      /* 0x64 */
} ObjInstance;

VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* ZOID_SetEnvColor */
void ZOID_SetEnvColor(short objnum, Color *col)
{
    ObjInstance *obj;
    obj = &ObjInstanceList[objnum];
    obj->envcolor = col->w;
}
