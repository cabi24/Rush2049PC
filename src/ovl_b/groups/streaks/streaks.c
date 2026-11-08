/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image B trail callback: age active fade nodes, release expired nodes, and
 * emit a four-corner quad when an eligible trail requests another segment.
 * No whole-function arcade ancestor has been established.
 *
 * Matching reconstruction: x/y/z are genuine origin-coordinate carriers;
 * lower is written before upper for each coordinate. Their declaration order
 * retains the native corners home at sp+140. The compiled-out pool-active read
 * raises the existing pool-address web's priority (30/17 -> 40/17), restoring
 * its native s4 allocation; it has no emitted load or behavioral side effect.
 *
 * Frame-only shaping, allowed by the repository's rule: the two unused
 * s32 locals each reserve four bytes. Without both, the complete instruction
 * body differs only in its 216-byte frame versus native 224. Adding both before
 * corners gives M100/T28/P4 instead of M92/T28/P4, retaining every other word
 * and the native corners home. Original identities are unknown. No pad array,
 * fake callee signature, stand-in, volatile, assembly, or compiler change.
 *
 * Assumptions: finite timer/coordinate values; well-formed acyclic active lists,
 * valid pool/node/object pointers, and valid backing for the external tables.
 * External allocation/free/object helpers retain their ordinary native ABI;
 * their implementation and full gameplay are outside this matching proof.
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Record88 Record88;
typedef struct Trail Trail;
typedef struct Fade Fade;
struct Trail { Trail *next; u32 unknown04, flags; f32 lifetime; f32 origin[3], previous[3], lower[3], upper[3]; };
struct Fade { Fade *next; u32 unknown04; Record88 *object; f32 lifetime, interval; u8 color[4]; Trail *source; };
/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct FadePool { u8 dbl; s32 numElements, elementSize; void *list; Fade *active; void *store; } FadePool;
typedef struct TrailPool { u8 dbl; s32 numElements, elementSize; void *list; Trail *active; void *store; } TrailPool;
extern FadePool gStreakPolyList;
extern TrailPool gStreakList;
extern f32 gFrameSeconds;
extern s16 gNumPlayers;
extern u8 gStreakColors[4][4];
extern void ZOID_RemovePoly(Record88 *);
extern void FreeStatLLE(void *,void *);
extern void *GetStatLLE(void *);
extern Record88 *ZOID_NewPoly(s32,f32 *,u16,u8 *,u16,s32);
extern void ZOID_UpdatePoly(Record88 *,s32,f32 *,u16,u8 *,u16,s32);
/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): a fade whose interval is 0 steps once a frame, 1/30 s on the N64; at
 * 60 it takes the other branch's 1/30 s, which the frame time counts down to a step every two frames. */
#define PC_FRAME_INTERVAL(x) (pc_fps60 ? 0.0333333f : (x))

void UpdateStreaks(void)
{
    Fade *fade, *next_fade, *new_fade;
    Trail *trail, *next_trail;
    f32 width;
    f32 x;
    s32 unused_local_0, unused_local_1;
    f32 corners[4][3];
    f32 y, z;
    for (fade = gStreakPolyList.active; fade; fade = next_fade) {
        if (gStreakPolyList.active) {}
        next_fade = fade->next;
        fade->lifetime -= gFrameSeconds;
        if (fade->lifetime <= 0.0f) {
remove_fade:
            ZOID_RemovePoly(fade->object);
            FreeStatLLE(&gStreakPolyList, fade);
        } else {
            if (fade->source->flags & 0x1000) fade->lifetime = 0.2f;
            if (fade->lifetime <= 0.2f) {
                fade->interval -= gFrameSeconds;
                if (fade->interval <= 0.0f) {
                    if (fade->source->flags & 8) fade->interval = PC_FRAME_INTERVAL(0.0f);
                    else fade->interval = 0.0333333f;
                    if (fade->color[3] < 23) {
                        goto remove_fade;
                    } else {
                        fade->color[3] -= 23;
                        ZOID_UpdatePoly(fade->object, 0, 0, 0, fade->color, 0, 0);
                    }
                }
            }
        }
    }
    for (trail = gStreakList.active; trail; trail = next_trail) {
        next_trail = trail->next;
        if (trail->flags & 0x1000) {
            trail->flags &= ~0x1000;
            FreeStatLLE(&gStreakList, trail);
        } else if (trail->lifetime >= 0.0f) {
            trail->lifetime -= gFrameSeconds;
            if (!(trail->flags & 0x2000) && (trail->flags & 0x10)) {
                x = trail->origin[0];
                y = trail->origin[1];
                z = trail->origin[2];
                trail->lower[0] = x;
                trail->upper[0] = x;
                trail->lower[1] = y;
                trail->upper[1] = y;
                trail->lower[2] = z;
                trail->upper[2] = z;
                if (trail->flags & 1) width = 0.75f;
                else if (trail->flags & 2) width = 0.5f;
                else if (trail->flags & 4) width = 0.35f;
                else width = 0.025f;
                trail->lower[1] -= width;
                trail->upper[1] += width;
            }
        } else if (trail->flags & 0x10) {
            if (!(trail->flags & 0x2000)) trail->flags |= 0x2000;
            if (gNumPlayers >= 3) trail->lifetime = 0.1f;
            else trail->lifetime = 0.0333333f;
            trail->flags &= ~0x10;
            new_fade = GetStatLLE(&gStreakPolyList);
            if (new_fade) {
                if (trail->flags & 1) {
                    new_fade->lifetime = 0.75f;
                    width = 0.75f;
                    new_fade->color[0] = gStreakColors[0][0];
                    new_fade->color[1] = gStreakColors[0][1];
                    new_fade->color[2] = gStreakColors[0][2];
                    new_fade->color[3] = gStreakColors[0][3];
                } else if (trail->flags & 2) {
                    new_fade->lifetime = 0.5f;
                    width = 0.5f;
                    new_fade->color[0] = gStreakColors[1][0];
                    new_fade->color[1] = gStreakColors[1][1];
                    new_fade->color[2] = gStreakColors[1][2];
                    new_fade->color[3] = gStreakColors[1][3];
                } else if (trail->flags & 4) {
                    new_fade->lifetime = 0.25f;
                    width = 0.35f;
                    new_fade->color[0] = gStreakColors[2][0];
                    new_fade->color[1] = gStreakColors[2][1];
                    new_fade->color[2] = gStreakColors[2][2];
                    new_fade->color[3] = gStreakColors[2][3];
                } else {
                    new_fade->lifetime = 0.16f;
                    width = 0.025f;
                    new_fade->color[0] = gStreakColors[3][0];
                    new_fade->color[1] = gStreakColors[3][1];
                    new_fade->color[2] = gStreakColors[3][2];
                    new_fade->color[3] = gStreakColors[3][3];
                }
                if (gNumPlayers >= 3) new_fade->lifetime *= 0.5f;
                if (trail->flags & 8) new_fade->interval = 0.0f;
                else new_fade->interval = 0.0333333f;
                new_fade->source = trail;
                corners[0][0] = trail->lower[0];
                corners[0][1] = trail->lower[1];
                corners[0][2] = trail->lower[2];
                corners[1][0] = trail->upper[0];
                corners[1][1] = trail->upper[1];
                corners[1][2] = trail->upper[2];
                corners[2][0] = trail->origin[0];
                corners[2][1] = trail->origin[1];
                corners[2][2] = trail->origin[2];
                corners[3][0] = trail->origin[0];
                corners[3][1] = trail->origin[1];
                corners[3][2] = trail->origin[2];
                corners[2][1] += width;
                corners[3][1] -= width;
                new_fade->object = ZOID_NewPoly(4, &corners[0][0], 0, new_fade->color, 0x120f, 0);
                if (new_fade->object) {
                    trail->upper[0] = corners[2][0];
                    trail->upper[1] = corners[2][1];
                    trail->upper[2] = corners[2][2];
                    trail->lower[0] = corners[3][0];
                    trail->lower[1] = corners[3][1];
                    trail->lower[2] = corners[3][2];
                    trail->previous[0] = trail->origin[0];
                    trail->previous[1] = trail->origin[1];
                    trail->previous[2] = trail->origin[2];
                } else {
                    FreeStatLLE(&gStreakPolyList, new_fade);
                }
            }
        }
    }
}
