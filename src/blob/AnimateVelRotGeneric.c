/* AnimateVelRotGeneric: the flight of a knocked-over cone or gas pump (MAT3 targets.c _AnimateVelRotGeneric). It
 * is the environment visual StartCone sets up: each frame the debris falls under gravity and tumbles, and when its
 * time runs out it is removed. */
#include "game/game.h"

/* The debris' motion (MAT3 tTargetStructVelRot), the target's per-kind data (a record of its kind's). */
typedef struct VelRot {
    f32 angV[3];            /* 0x00 its spin, radians a second about x, y, z */
    f32 vel[3];             /* 0x0C its velocity, feet a frame */
} VelRot;

/* targetInfo[].Flags: the target's scene object goes when its animation ends (MAT3 TInfo(t)->Flags & 0x2000). */
#define TINFO_REMOVE_WHEN_DONE 0x2000

/* The end of the flight: remove the scene object if the kind says so, free the target and release the visual. */
static void debris_remove(struct pc_visual *v, tTarget *t, TargInfo *info) {
    if (info->Flags & TINFO_REMOVE_WHEN_DONE) {
        RemoveObjectTree(t->dlIdx, 0, 0);
    }
    FreeStatLLE(&gTargetList, t);
    ReleaseEnviron(v, 1);
}

/* One frame of the debris (op 1), or release its visual (op 0). Its velocity gains gravity (velrot_gravity, scaled
 * by 0.15) and moves it, a fixed step a frame; its spin turns it by the frame's length; and its time is counted
 * down. Nothing moves while the game is paused. The visual's data is the target; its timeStamp the seconds of
 * flight left (StartCone gives 5). */
void AnimateVelRotGeneric(struct pc_visual *v, s16 op) {
    VelRot *motion;
    tTarget *t;
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
    t = (tTarget *) v->data;
    motion = t->data;
    vel = motion->vel;
    for (i = 0; i < 3; i++) {
        vel[i] += velrot_gravity[i] * 0.15f;
        t->mat.pos[i] += vel[i] + velrot_gravity[i] * 0.15f;
    }
    angle[0] = motion->angV[0] * gFrameSeconds;
    angle[1] = motion->angV[1] * gFrameSeconds;
    angle[2] = motion->angV[2] * gFrameSeconds;
    RotateUV(angle, t->mat.uvs);
    v->timeStamp -= gFrameSeconds;
    if (v->timeStamp <= 0.0f) {
        debris_remove(v, t, &targetInfo[t->infoIdx]);
    }
}
