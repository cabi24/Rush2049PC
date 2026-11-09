/* streaks.c: the light streaks behind battle shots, frame by frame (MAT3 streaks.c, UpdateStreaks(), with
 * _AnimateStreakPolys() and _NewStreakPoly() compiled into it).
 *
 * A streak (gStreakArray, NewStreak) follows a shot; each step it lays a ribbon segment (a four-vertex polygon,
 * gStreakPolyArray) from its last point to the shot's new one, and each segment fades out and is freed. Runtime
 * image B (battle and stunt races). */
#include "game/game.h"

typedef struct Record88 Record88;   /* a scene polygon (ZOID_NewPoly) */
typedef struct tStreak tStreak;
typedef struct tStreakPoly tStreakPoly;
/* A streak (MAT3 tStreak; types.h's PcTrail): flags STREAK_*, the time to its next segment, its shot's new and last
 * points and the ribbon's back edge (backVerts: [0] the lower corner, [1] the upper). */
struct tStreak { tStreak *next; u32 prev, flags; f32 delay; f32 newpos[3], lastpos[3], backVerts[2][3]; };
/* A ribbon segment (MAT3 tStreakPoly; types.h's PcFade): its polygon, the seconds it has left, the time to its next
 * fade step, its colour and the streak it belongs to. */
struct tStreakPoly { tStreakPoly *next; u32 prev; Record88 *poly; f32 lifeTime, incTime; u8 clr[4]; tStreak *s; };


/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): a fade whose interval is 0 steps once a frame, 1/30 s on the N64; at
 * 60 it takes the other branch's 1/30 s, which the frame time counts down to a step every two frames. */
#define PC_FRAME_INTERVAL(x) (pc_fps60 ? 0.0333333f : (x))

/* Once a frame, first the ribbon segments: each counts its life down and is freed at the end; in its last 0.2 s
 * (at once when its streak is done) it fades, its alpha down 23 every 1/30 s (every frame for STREAK_THIN), and is
 * freed when the alpha is spent. Then the streaks: a done one is freed; while its delay runs the ribbon's back edge
 * stays at the shot's start (the streak's half width above and below it); after that, each time its shot has moved
 * (STREAK_NEWPOS) it lays a segment from the back edge to the new point, which becomes the back edge, and waits 1/30 s
 * (0.1 s with three or four players, whose segments also last half as long). */
void UpdateStreaks(void)
{
    tStreakPoly *sp, *nextsp, *newsp;
    tStreak *s, *nexts;
    f32 w;
    f32 x;
    f32 xyz[4][3];
    f32 y, z;
    for (sp = gStreakPolyList.head; sp; sp = nextsp) {
        nextsp = sp->next;
        sp->lifeTime -= gFrameSeconds;
        if (sp->lifeTime <= 0.0f) {
remove_fade:
            ZOID_RemovePoly(sp->poly);
            FreeStatLLE(&gStreakPolyList, sp);
        } else {
            if (sp->s->flags & STREAK_DONE) sp->lifeTime = 0.2f;
            if (sp->lifeTime <= 0.2f) {
                sp->incTime -= gFrameSeconds;
                if (sp->incTime <= 0.0f) {
                    if (sp->s->flags & STREAK_THIN) sp->incTime = PC_FRAME_INTERVAL(0.0f);
                    else sp->incTime = 0.0333333f;
                    if (sp->clr[3] < 23) {
                        goto remove_fade;
                    } else {
                        sp->clr[3] -= 23;
                        ZOID_UpdatePoly(sp->poly, 0, 0, 0, sp->clr, 0, 0);
                    }
                }
            }
        }
    }
    for (s = gStreakList.head; s; s = nexts) {
        nexts = s->next;
        if (s->flags & STREAK_DONE) {
            s->flags &= ~STREAK_DONE;
            FreeStatLLE(&gStreakList, s);
        } else if (s->delay >= 0.0f) {
            s->delay -= gFrameSeconds;
            if (!(s->flags & STREAK_STARTED) && (s->flags & STREAK_NEWPOS)) {
                x = s->newpos[0];
                y = s->newpos[1];
                z = s->newpos[2];
                s->backVerts[0][0] = x;
                s->backVerts[1][0] = x;
                s->backVerts[0][1] = y;
                s->backVerts[1][1] = y;
                s->backVerts[0][2] = z;
                s->backVerts[1][2] = z;
                if (s->flags & STREAK_WIDE) w = 0.75f;
                else if (s->flags & STREAK_MEDIUM) w = 0.5f;
                else if (s->flags & STREAK_NARROW) w = 0.35f;
                else w = 0.025f;
                s->backVerts[0][1] -= w;
                s->backVerts[1][1] += w;
            }
        } else if (s->flags & STREAK_NEWPOS) {
            if (!(s->flags & STREAK_STARTED)) s->flags |= STREAK_STARTED;
            if (gNumPlayers >= 3) s->delay = 0.1f;
            else s->delay = 0.0333333f;
            s->flags &= ~STREAK_NEWPOS;
            /* the new segment (MAT3 _NewStreakPoly) */
            newsp = GetStatLLE(&gStreakPolyList);
            if (newsp) {
                if (s->flags & STREAK_WIDE) {
                    newsp->lifeTime = 0.75f;
                    w = 0.75f;
                    newsp->clr[0] = gStreakColors[0][0];
                    newsp->clr[1] = gStreakColors[0][1];
                    newsp->clr[2] = gStreakColors[0][2];
                    newsp->clr[3] = gStreakColors[0][3];
                } else if (s->flags & STREAK_MEDIUM) {
                    newsp->lifeTime = 0.5f;
                    w = 0.5f;
                    newsp->clr[0] = gStreakColors[1][0];
                    newsp->clr[1] = gStreakColors[1][1];
                    newsp->clr[2] = gStreakColors[1][2];
                    newsp->clr[3] = gStreakColors[1][3];
                } else if (s->flags & STREAK_NARROW) {
                    newsp->lifeTime = 0.25f;
                    w = 0.35f;
                    newsp->clr[0] = gStreakColors[2][0];
                    newsp->clr[1] = gStreakColors[2][1];
                    newsp->clr[2] = gStreakColors[2][2];
                    newsp->clr[3] = gStreakColors[2][3];
                } else {
                    newsp->lifeTime = 0.16f;
                    w = 0.025f;
                    newsp->clr[0] = gStreakColors[3][0];
                    newsp->clr[1] = gStreakColors[3][1];
                    newsp->clr[2] = gStreakColors[3][2];
                    newsp->clr[3] = gStreakColors[3][3];
                }
                if (gNumPlayers >= 3) newsp->lifeTime *= 0.5f;
                if (s->flags & STREAK_THIN) newsp->incTime = 0.0f;
                else newsp->incTime = 0.0333333f;
                newsp->s = s;
                /* the back edge's two corners, then the new point's, w above and below it */
                xyz[0][0] = s->backVerts[0][0];
                xyz[0][1] = s->backVerts[0][1];
                xyz[0][2] = s->backVerts[0][2];
                xyz[1][0] = s->backVerts[1][0];
                xyz[1][1] = s->backVerts[1][1];
                xyz[1][2] = s->backVerts[1][2];
                xyz[2][0] = s->newpos[0];
                xyz[2][1] = s->newpos[1];
                xyz[2][2] = s->newpos[2];
                xyz[3][0] = s->newpos[0];
                xyz[3][1] = s->newpos[1];
                xyz[3][2] = s->newpos[2];
                xyz[2][1] += w;
                xyz[3][1] -= w;
                newsp->poly = ZOID_NewPoly(4, &xyz[0][0], 0, newsp->clr, 0x120f, 0);
                if (newsp->poly) {
                    s->backVerts[1][0] = xyz[2][0];
                    s->backVerts[1][1] = xyz[2][1];
                    s->backVerts[1][2] = xyz[2][2];
                    s->backVerts[0][0] = xyz[3][0];
                    s->backVerts[0][1] = xyz[3][1];
                    s->backVerts[0][2] = xyz[3][2];
                    s->lastpos[0] = s->newpos[0];
                    s->lastpos[1] = s->newpos[1];
                    s->lastpos[2] = s->newpos[2];
                } else {
                    FreeStatLLE(&gStreakPolyList, newsp);
                }
            }
        }
    }
}
