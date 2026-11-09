/* AnimateVelRotGeneric: the flight of a knocked-over cone or gas pump (MAT3 targets.c _AnimateVelRotGeneric). It
 * is the environment visual StartCone sets up: each frame the debris falls under gravity and tumbles, and when its
 * time runs out it is removed. */
#include "game/game.h"

/* The debris' motion (MAT3 tTargetStructVelRot), the target's per-kind data. */
typedef struct VelRot {
    f32 angV[3];            /* 0x00 its spin, radians a second about x, y, z */
    f32 vel[3];             /* 0x0C its velocity, feet a frame */
} VelRot;

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
enum n64p_VelRot { n64p_VelRot_ };
enum n64p_Target { n64p_Target_ };
typedef struct Target {
    u8 pad00[0xC];
    s32 dlIdx;              /* 0x0C its scene object: the handle, whose low half is the object's index */
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[2];
    f32 uvs[3][3];          /* 0x14 mat: its orientation */
    f32 matPos[3];          /* 0x38 mat: its position */
    u8 pad44[0x6C - 0x44];
    enum n64p_VelRot data;  /* 0x6C its VelRot (a window address) */
} Target;

/* The environment visual (MAT3 tVisual) in its PC layout: next and func are host pointers, data the target's
 * window address. */
typedef struct Visual {
    void *next;
    u8 pad08[0x10 - 8];
    enum n64p_Target data;  /* 0x10 */
    f32 timeStamp;          /* 0x14 the seconds of flight left (StartCone gives 5) */
    void (*func)();
} Visual;
#define N64P(x) _Generic((x), enum n64p_VelRot: (VelRot *)PC_N64P(x), enum n64p_Target: (Target *)PC_N64P(x), \
    default: (x))

/* targetInfo[].Flags: the target's scene object goes when its animation ends (MAT3 TInfo(t)->Flags & 0x2000). */
#define TINFO_REMOVE_WHEN_DONE 0x2000

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* The end of the flight: remove the scene object if the kind says so, free the target and release the visual. */
static void debris_remove(Visual *v, Target *t, TargInfo *info) {
    if (info->Flags & TINFO_REMOVE_WHEN_DONE) {
        RemoveObjectTree(t->dlIdx, 0, 0);
    }
    FreeStatLLE(&gTargetList, t);
    ReleaseEnviron(v, 1);
}

/* One frame of the debris (op 1), or release its visual (op 0). Its velocity gains gravity (velrot_gravity, scaled
 * by 0.15) and moves it, a fixed step a frame; its spin turns it by the frame's length; and its time is counted
 * down. Nothing moves while the game is paused. */
void AnimateVelRotGeneric(Visual *v, s16 op) {
    VelRot *motion;
    Target *t;
    f32 angle[3];
    f32 *vel;
    s16 i;

    if (op == 0) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    t = N64P(v->data);
    motion = N64P(t->data);
    vel = motion->vel;
    for (i = 0; i < 3; i++) {
        vel[i] += velrot_gravity[i] * 0.15f;
        t->matPos[i] += vel[i] + velrot_gravity[i] * 0.15f;
    }
    angle[0] = motion->angV[0] * gFrameSeconds;
    angle[1] = motion->angV[1] * gFrameSeconds;
    angle[2] = motion->angV[2] * gFrameSeconds;
    RotateUV(angle, t->uvs);
    v->timeStamp -= gFrameSeconds;
    if (v->timeStamp <= 0.0f) {
        debris_remove(v, t, &targetInfo[t->infoIdx]);
    }
}
