#include "game/game.h"

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

/* ZOID_FindMatrix */
Node *ZOID_FindMatrix(short objnum)
{
    return ObjInstanceList[objnum].mat;
}
