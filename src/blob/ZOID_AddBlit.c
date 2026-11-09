#include "game/game.h"

typedef struct Slot {
    void *p0;   /* 0x00 */
    void *p4;   /* 0x04 */
    s16 id;     /* 0x08 */
    s16 a;      /* 0x0A */
    s16 b;      /* 0x0C */
    s16 e;      /* 0x0E */
    s16 w;      /* 0x10 */
    s16 h;      /* 0x12 */
    u8 alpha;   /* 0x14 */
    u8 f15;     /* 0x15 */
    s8 state;   /* 0x16 */
    s8 f17;     /* 0x17 */
    s16 x18;    /* 0x18 */
    s16 x1A;    /* 0x1A */
    s16 hm1;    /* 0x1C */
    s16 wm1;    /* 0x1E */
} Slot;

VIEW(Slot[200], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

s32 ZOID_AddBlit(s32 id, void *p4, void *p0, s32 a, s32 b, s32 w, s32 h)
{
    Slot *s;
    s32 i;

    for (i = 0; i < gNumBlitInsts; i++) {
        if (gBlitInstanceList[i].state == 2) {
            break;
        }
    }
    if (i >= 200) {
        return -1;
    }
    s = &gBlitInstanceList[i];
    if (i >= gNumBlitInsts) {
        gNumBlitInsts++;
    }
    if (gMaxBlitInsts < gNumBlitInsts) {
        gMaxBlitInsts = gNumBlitInsts;
    }
    
    s->b = b;
    s->p4 = p4;
    s->p0 = p0;
    s->id = id;
    s->a = a;
    s->e = 0;
    s->alpha = 255;
    s->x18 = 0;
    s->x1A = 0;
    s->w = w;
    s->h = h;
    do {} while (0);
    s->hm1 = h - 1;
    s->wm1 = w - 1;
    s->state = 0;
    s->f15 = 0;

    return i;
}
