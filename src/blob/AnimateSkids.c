/* AnimateSkids.c: fade out the finished skid marks and free the faded ones (MAT3 skids.c, AnimateSkids()).
 *
 * A skid mark is a four-vertex polygon laid along a tire's track while it skids (UpdateSkids); once the skid stops
 * (StopSkid) the mark is "active" and fades here, once a frame, until it is gone. */
#include "game/game.h"

/* A skid mark, an element of gSkidList (skids.c's tSkid, the N64's 36-byte form; PC: its two links and the polygon
 * handle are pointer-sized, PcEffectObj in types.h). */
typedef struct tSkid tSkid;
struct tSkid {
    tSkid *next;
    tSkid *prev;        /* the doubly linked list's back link (GetStatLLE), pointer-sized here */
    f32 lastTime;       /* gSeconds when the skid last grew: the fade starts from it */
    intptr_t poly;      /* the mark's polygon (ZOID_NewPoly) */
    f32 pos[3];         /* the mark's middle (StopSkid) */
    u8 color[4];        /* r, g, b, alpha */
    s32 active;         /* set once the skid has stopped: the mark fades */
};
extern void ZOID_UpdatePoly(intptr_t, s32, s32, s32, u8 *, s32, s32);
extern void ZOID_RemovePoly(intptr_t);

/* PC: a float to u32 that is negative while a mark's lastTime is ahead of the clock. Undefined in C, and the hosts
 * differ: x86-64 and IDO's code for the N64 go through a signed conversion and keep the bits (-125 is 0xFFFFFF83, no
 * fade), aarch64's fcvtzu gives 0 (the fade at full). So through s32 below 0, as the N64 (docs/port/SANITIZE.md). */
static u32 pc_f2u(f32 x) { return x < 0.0f ? (u32)(s32)x : (u32)x; }
#define F2U pc_f2u

/* Fade every stopped skid mark: its alpha falls from 192 by 255 over the fade time, counted from when the skid
 * last grew, and a mark whose alpha would reach 0 is removed and its element freed. The fade time is 2 seconds plus
 * 2/100 s for each free mark, so marks last longer while few are in use. (MAT3 fades from 128, starts the ramp at
 * 127 and caps at 128; the N64 starts it at 63 and caps at 192.) */
void AnimateSkids(void) {
    tSkid *s, *next;
    s32 numfree = 0;
    u32 xlu;
    f32 t;
    for (s = gSkidList.store; s; s = s->next) numfree++;
    s = gSkidList.head;
    while (s) {
        if (!s->active) {
            s = s->next;
            continue;
        }
        {
            t = (gSeconds - s->lastTime) / (((f32)numfree + (f32)numfree)/100.0f + 2.0f);
            xlu = F2U(t * 255.0f + 63.0f);
            if (xlu < 255) {
                s->color[3] = 255 - xlu;
                if (s->color[3] > 192) s->color[3] = 192;
                ZOID_UpdatePoly(s->poly, 4, 0, 0, s->color, 0, 0);
                s = s->next;
            } else {
                next = s->next;
                ZOID_RemovePoly(s->poly);
                FreeStatLLE(&gSkidList, s);
                s = next;
            }
        }
    }
}
