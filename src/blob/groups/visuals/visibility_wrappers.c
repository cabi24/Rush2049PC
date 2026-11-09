#include "game/game.h"

/* Genuine complete logical bodies, reconstructed from the native
 * 18/25-word routines. Private native register conventions require actual
 * source context; this file makes no ordinary singleton match claim. */
#ifndef VISUAL_TIRE_TYPES_H
#define VISUAL_TIRE_TYPES_H

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Visual {
    void *prefix;
    s16 tire,object,slot,reserved;
    f32 angle,time;
    void (*callback)(void);
} Visual;

enum n64p_f32 { n64p_f32_ };

/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_f32: (f32 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

#endif

s32 CheckVisible(Visual *v,s32 show,s32 part)
{
    if(part<0) return CheckVisibleAll(v,show);
    if(show) MBOX_ShowObject(v->object,0,1<<part);
    else MBOX_HideObject(v->object,1,1<<part);
    return show;
}
