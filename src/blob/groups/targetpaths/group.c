/* Target paths (MAT3 targetpaths.c): the track's objects that move along paths, the trams, the trains, the
 * doors, the lifts: each is a target (tTarget) whose record (tTargetStructPath) says where on its path it is. The
 * paths are the track WAD's PTHD records (tBasicTargetPath, gTargetPaths), each a list of points with a position,
 * an orientation, a scale and the time to the next; a path with collision (flag 0x20) moves its collision object
 * with the target (MoveCollisionObject, RotateCollisionObject). An environment visual animates each one every frame
 * (AnimatePathTarget), and each plays its start, move and stop sounds. The PTHD records, the targets and their
 * records keep the N64's layout on the PC: their pointers are 32-bit window addresses (PC_N64P, PC_N64W;
 * docs/port/POINTERS.md 10). */
#include "game/game.h"

float fabsf(float);
float sqrtf(float);

/* A target path (MAT3 tBasicTargetPath, the track WAD's PTHD record; waves.c has its own view). N64 layout. */
typedef struct tBasicTargetPath { /* 0x24 bytes */
    char name[16];  /* 0x00 */
    s32 flags;      /* 0x10 1 back and forth, 2 a loop, 4 in forward races, 8 in backward races, 0x10 the target
                     * doesn't collide, 0x20 a collision object moves with it, 0x40 worked by a trigger
                     * (trigPtr), 0x80 stops at its end, 0x100 stopped, 0x8000 its object definition never
                     * changes, 0x100000 showing OBJ_PATH_MOVING, 0x200000 showing OBJ_PATH_STOPPED */
    s16 numPts;     /* 0x14 */
    s16 collIdx;    /* 0x16 its collision object, 0 none */
    u32 trigPtr;    /* 0x18 the path that triggers it, a window address */
    u32 pts;        /* 0x1C its points (TargetPathPt), a window address */
    u32 parent;     /* 0x20 the record of the target on it (tTargetStructPath), a window address */
} tBasicTargetPath;

/* The N64-layout records' links, as host pointers: a target's record, a record's path, a path's points, trigger and
 * target record, and the target an environment visual animates (its data). */
#define TARGET_STATE(t) ((tTargetStructPath *) PC_N64P((t)->data))
#define STATE_PATH(s) ((tBasicTargetPath *) PC_N64P((s)->pathData))
#define PATH_TRIGGER(p) ((tBasicTargetPath *) PC_N64P((p)->trigPtr))
#define PATH_PARENT(p) ((tTargetStructPath *) PC_N64P((p)->parent))
#define VISUAL_TARGET(v) ((tTarget *) PC_N64P((v)->data))

/* The object definitions (gObjList) a worked path target shows, moving and stopped. */
#define OBJ_PATH_MOVING 348
#define OBJ_PATH_STOPPED 349

/* unitmatrix's fourth row, the origin: the sounds' orientation */
VIEW(s32, unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view

VIEW(s32[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(struct pc_visual *, gEnvList);
#define gEnvList gEnvList_view

extern tTarget *CollisionTargetPtrs[];

VIEW(u16, gObjList);
#define gObjList gObjList_view

void AudUpdateEmitter(s32 tag, intptr_t pos, intptr_t dir, f32 maxVol, f32 pitch);

s32 AudEmitEffect(void *pos, intptr_t dir, f32 fade, f32 a, f32 b, f32 c, s32 id, s32 slot, s32 replace, s32 vol);

struct pc_visual *GrabEnvEntry(void);

/* Starts sound `id` at the target's position (unless game sounds are off or it is -1, which give -1), fading over
 * `fadeDist` with replace mode `replace`; returns the emitter. */
static __inline s32 target_emit_sound(tTarget *target, f32 fadeDist, s32 id, s32 replace) {
    if (gGameSoundsEnabled == 0) {
        return -1;
    }
    if (id == -1) {
        return -1;
    }
    return AudEmitEffect(&target->mat.pos, (s32) &unitmatrix_origin, fadeDist, 0.0f, 1.0f, 0.0f, id, 0, replace, 0x80);
}

/* The target starts moving: its kind's start sound replaces whatever it was playing. */
void PlayStartSound(tTarget *target) {
    TargInfo *info;

    AudStopEmitter(target->sound);
    info = &targetInfo[target->infoIdx];
    if (info->sound != -1) {
        target->sound = target_emit_sound(target, info->fadeDist, info->sound, info->replace);
    } else {
        target->sound = -1;
    }
    target->sndState = 0;
}

/* The target stops: its kind's stop sound, once (not again while stopped), and not over a start sound still
 * playing. */
void PlayStopSound(tTarget *target) {
    TargInfo *info;

    if (target->sndState != 2 && (target->sndState != 0 || AudEmitterPlaying(target->sound) == 0)) {
        AudStopEmitter(target->sound);
        info = &targetInfo[target->infoIdx];
        if (info->stopSnd != -1) {
            target->sound = target_emit_sound(target, info->fadeDist, info->stopSnd, info->replace);
        } else {
            target->sound = -1;
        }
        target->sndState = 2;
    }
}

/* Sets the target's orientation on its path: its point's (a point flagged 0x20, or one with no time to the next,
 * holds it), else turning evenly (InterpQuats) towards the next point's by the time on the stretch. A path with a
 * collision object turns it too, but in the obstacle course only within 150 feet of the first car. */
void PlacePathTargetQuat(tTarget *target) {
    f32 out[4];
    f32 d[3];
    tTargetStructPath *state;
    tBasicTargetPath *path;
    TargetPathPt *pt;
    TargetPathPt *pts;
    f32 t;
    f32 dur;
    s32 next;
    s32 idx;
    s32 near;
    f32 dist;

    state = TARGET_STATE(target);
    path = STATE_PATH(TARGET_STATE(target));
    idx = state->ptOn;
    pt = &PATH_PTS(path)[idx];
    pts = PATH_PTS(path);
    if (pt->flags & 0x20) {
        goto plain;
    }
    if (state->flags & TPATH_BACKWARD) {
        t = pt->timeToNext;
        t -= state->timeElapsed;
    } else {
        t = state->timeElapsed;
    }
    dur = pt->timeToNext;
    if (dur == 0.0f) {
plain:
        CreateQuatMat(target->mat.uvs[0], pt->quat, 1);
    } else {
        next = idx + 1;
        if (next >= path->numPts && (path->flags & 2)) {
            next = 0;
        }
        InterpQuats(t / dur, pt->quat, pts[next].quat, out);
        CreateQuatMat(target->mat.uvs[0], out, 1);
    }
    if (path->flags & 0x20) {
        near = 1;
        if (gRace == kObstacle) {
            d[0] = target->mat.pos[0] - ((f32 *) PC_CARS)[2];
            d[1] = target->mat.pos[1] - ((f32 *) PC_CARS)[3];
            d[2] = target->mat.pos[2] - ((f32 *) PC_CARS)[4];
            dist = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (22500.0f < dist) {
                near = 0;
            }
        }
        if (near != 0) {
            RotateCollisionObject(path->collIdx, target->mat.pos, state->lastUVS, target->mat.uvs[0]);
        }
    }
}

#define path_abs(x) ((x) < 0 ? -(x) : (x))

/* Keeps the target's moving sound going: while moving, the emitter follows the target, its volume and pitch by its
 * speed against the path's top speed for a moving sound 1 (two other sounds have fixed ones); after a stop it
 * starts over with the start sound; once the start sound is over, the moving sound loops. The two empty
 * statements are the original's (the N64 code tests them and does nothing). */
void PlayMoveSound(tTarget *target) {
    tTargetStructPath *state;
    f32 ratio;
    s32 maxVel;
    s32 curVel;

    if (target->sndState == 1) {
        if (target->sound != -1) {
            if (targetInfo[target->infoIdx].moveSnd == 1) {
                state = TARGET_STATE(target);
                maxVel = state->maxVel;
                curVel = state->curVel;
                if (!maxVel) {
                    if (curVel) maxVel = curVel;
                    else maxVel = 1;
                }
                if ((maxVel < 0 && curVel > 0) || (maxVel > 0 && curVel < 0)) ;
                ratio = (f32)path_abs(curVel) / (f32)path_abs(maxVel);
                ratio = ratio / 2.0f + 0.5f;
                if (ratio < 0.0f || ratio > 1.0f) {
                    if ((s32)ratio < 0 || (s32)ratio > 1) ;
                    if (ratio < 0.0f) ratio = 0.0f;
                    else ratio = 1.0f;
                }
            } else ratio = 1.0f;
            if (targetInfo[target->infoIdx].moveSnd == 0x12)
                AudUpdateEmitter(target->sound, (intptr_t) target->mat.pos, (intptr_t) &unitmatrix_origin, 0.8f, 1.0f);
            else if (targetInfo[target->infoIdx].moveSnd == 0x61)
                AudUpdateEmitter(target->sound, (intptr_t) target->mat.pos, (intptr_t) &unitmatrix_origin, 1.0f, 0.75f);
            else AudUpdateEmitter(target->sound, (intptr_t) target->mat.pos, (intptr_t) &unitmatrix_origin, 0.75f * ratio + 0.25f, ratio);
        }
    } else if (target->sndState == 2) PlayStartSound(target);
    else if (!AudEmitterPlaying(target->sound)) {
        AudStopEmitter(target->sound);
        if (targetInfo[target->infoIdx].moveSnd != -1) {
            target->sound = target_emit_sound(target, targetInfo[target->infoIdx].fadeDist, targetInfo[target->infoIdx].moveSnd, targetInfo[target->infoIdx].replace);
            if (targetInfo[target->infoIdx].moveSnd == 1)
                AudUpdateEmitter(target->sound, (intptr_t) target->mat.pos, (intptr_t) &unitmatrix_origin, 0.0f, 1.0f);
        } else target->sound = -1;
        target->sndState = 1;
    }
}

/* Puts the target at point `idx` of its path, standing still: heading forwards, or backwards from the last point
 * of a back-and-forth path; with its point's orientation and scale, which its collision object starts from. A path
 * that stops (or is triggered) is stopped and records this target as its own; one worked by a trigger is marked
 * to start again. */
void ResetPath(tTarget *target, s32 idx) {
    tTargetStructPath *state;
    tBasicTargetPath *path;
    s32 i;

    state = TARGET_STATE(target);
    path = STATE_PATH(state);
    if ((path->flags & 1) && idx >= path->numPts - 1) {
        state->flags = TPATH_BACKWARD;
    } else {
        state->flags = TPATH_FORWARD;
    }
    state->ptOn = idx;
    state->timeElapsed = 0.0f;
    state->curVel = 0.0f;
    target->mat.pos[0] = PATH_PTS(path)[state->ptOn].pos[0];
    target->mat.pos[1] = PATH_PTS(path)[state->ptOn].pos[1];
    target->mat.pos[2] = PATH_PTS(path)[state->ptOn].pos[2];
    state->lastPos[0] = PATH_PTS(path)[state->ptOn].pos[0];
    state->lastPos[1] = PATH_PTS(path)[state->ptOn].pos[1];
    state->lastPos[2] = PATH_PTS(path)[state->ptOn].pos[2];
    if (path->flags & 0xC0) {
        path->flags |= 0x100;
        path->parent = PC_N64W(state);
    }
    if (path->flags & 0x5000) {
        path->flags |= 0x400;
    }
    CreateQuatMat(target->mat.uvs, PATH_PTS(path)[state->ptOn].quat, 1);
    CopyUV(target->mat.uvs, state->lastUVS);
    TransposeUV(state->lastUVS);
    for (i = 0; i < 3; i++) {
        target->mat.uvs[0][i] = PATH_PTS(path)[state->ptOn].scale[0] * target->mat.uvs[0][i];
        target->mat.uvs[1][i] = PATH_PTS(path)[state->ptOn].scale[1] * target->mat.uvs[1][i];
        target->mat.uvs[2][i] = PATH_PTS(path)[state->ptOn].scale[2] * target->mat.uvs[2][i];
    }
}

/* Sets the target's position on its path from the time on its stretch: along the stretch's direction, either
 * evenly (a point flagged 0x10000008: the stretch's length in its time) or with the speed changing evenly from this
 * point's to the next's; the offset is rounded to 1/32 of a foot. Its speed (curVel) is the distance moved this
 * frame over the frame's time. A path with a collision object moves it too, but in the obstacle course only within
 * 450 feet of the first car. A path that stops at its end doesn't move past its last point. */
void PlacePathTargetPos(tTarget *target) {
    f32 v[3];
    f32 w[3];
    f32 u[3];
    tTargetStructPath *state;
    tBasicTargetPath *path;
    TargetPathPt *pt;
    TargetPathPt *pts;
    s32 idx;
    s32 n;
    s32 next;
    s32 near;
    f32 t;
    f32 d;
    f32 a;
    f32 c;
    f32 dv;
    s32 i;
    s32 j;

    state = TARGET_STATE(target);
    idx = state->ptOn;
    path = STATE_PATH(state);
    pt = &PATH_PTS(path)[idx];
    n = path->numPts;
    pts = PATH_PTS(path);
    if (idx < n - 1 || (path->flags & 2) != 0 || !(path->flags & 0x80)) {
        if (pt->flags & 0x10000008) {
            if (state->flags & TPATH_BACKWARD) {
                t = pt->timeToNext - state->timeElapsed;
            } else {
                t = state->timeElapsed;
            }
            d = pt->distToNext * (t / pt->timeToNext);
        } else {
            next = idx + 1;
            if (next >= n && (path->flags & 2)) {
                next = 0;
            }
            a = pt->speed;
            d = pts[next].speed;
            dv = d - a;
            if (state->flags & TPATH_BACKWARD) {
                t = pt->timeToNext - state->timeElapsed;
            } else {
                t = state->timeElapsed;
            }
            c = (t * t * dv) / (pt->timeToNext * 2);
            if (d < a) {
                d = a * t + c;
            } else {
                d = a * t + c;
            }
        }
        v[0] = pt->norm[0];
        v[1] = pt->norm[1];
        v[2] = pt->norm[2];
        state->lastPos[0] = target->mat.pos[0];
        state->lastPos[1] = target->mat.pos[1];
        state->lastPos[2] = target->mat.pos[2];
        v[0] = v[0] * d;
        v[1] = v[1] * d;
        v[2] = v[2] * d;
        for (i = 0; &v[i] < &v[3]; i++) {
            j = v[i] * 32.0f;
            v[i] = j * 0.03125f;
        }
        target->mat.pos[0] = pt->pos[0] + v[0];
        target->mat.pos[1] = pt->pos[1] + v[1];
        target->mat.pos[2] = pt->pos[2] + v[2];
        v[0] = target->mat.pos[0] - state->lastPos[0];
        v[1] = target->mat.pos[1] - state->lastPos[1];
        v[2] = target->mat.pos[2] - state->lastPos[2];
        state->curVel = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) / *(volatile f32 *) &gFrameSeconds;
        if (path->flags & 0x20) {
            near = 1;
            if (gRace == kObstacle) {
                w[0] = target->mat.pos[0] - ((f32 *) PC_CARS)[2];
                w[1] = target->mat.pos[1] - ((f32 *) PC_CARS)[3];
                w[2] = target->mat.pos[2] - ((f32 *) PC_CARS)[4];
                d = w[0] * w[0] + w[1] * w[1] + w[2] * w[2];
                if (202500.0f < d) {
                    near = 0;
                }
            }
            if (near != 0) {
                MoveCollisionObject(path->collIdx, v);
            }
        }
    }
}

/* A scene node (ObjInstanceList), host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;

/* Gives scene node `slot` object definition `v` (MAT3 ZOID_SetObjectDef, which the N64 compiler inlined here; this
 * file's own copy). */
void ZOID_SetObjectDef_TargetPaths(s32 slot, u16 v) {
    ((PcNode *) &ObjInstanceList)[slot].model = v;
}

/* A path target's start function (its kind's startFunc): the first time, stops it on its path. In a ghost race a
 * path with a collision object is set where its points flagged 0x1000000 put it, its collision object moved and
 * turned to match, and nothing else. Otherwise it is registered for GetTargetVelocity (with a collision object),
 * takes its path's top speed, starts at the first point flagged 1 (marking it 0x1000, used), sets up its
 * animation's frames (object 361: 4 frames a quarter second apart, 365: 20 of 0.0425 s), is shown stopped if its
 * path is worked by a trigger, and gets an environment visual to animate it (AnimatePathTarget). The local colour
 * and the unused locals are the N64 code's (it never reads them). */
void StartPathTarget(tTarget *target) {
    u16 *objects = (u16 *) &gObjList;   /* the object definitions by kind */
    s32 unused[11];
    u8 col[4] = { 255, 0, 0, 255 };
    TargInfo *info;
    tBasicTargetPath *path;
    tTargetStructPath *state;
    s32 i;
    f32 mx;
    struct pc_visual *node;
    s32 unused1;
    s32 unused2;

    target->sndState = 2;
    info = &targetInfo[target->infoIdx];
    state = TARGET_STATE(target);
    path = STATE_PATH(TARGET_STATE(target));
    if (!(target->Flags & TARGET_SET_UP)) {
        state->flags = 0;
        state->timeElapsed = 0.0f;
        if (gRace == kGhost && path->collIdx > 0) {
            tBasicTargetPath *s;
            s32 j;
            f32 v[3];
            f32 delta[3];
            f32 mat[9];

            s = STATE_PATH(TARGET_STATE(target));
            for (j = 0; j < s->numPts; j++) {
                TargetPathPt *pt = &PATH_PTS(s)[j];

                if (PATH_PTS(s)[j].flags & 0x1000000) {
                    target->mat.pos[0] = pt->pos[0];
                    target->mat.pos[1] = pt->pos[1];
                    target->mat.pos[2] = pt->pos[2];
                    CreateQuatMat(target->mat.uvs, pt->quat, 1);
                }
            }
            if (s->flags & 0x20) {
                v[0] = PATH_PTS(s)[0].pos[0];
                v[1] = PATH_PTS(s)[0].pos[1];
                v[2] = PATH_PTS(s)[0].pos[2];
                CreateQuatMat(mat, PATH_PTS(s)[0].quat, 1);
                TransposeUV(mat);
                delta[0] = target->mat.pos[0] - v[0];
                delta[1] = target->mat.pos[1] - v[1];
                delta[2] = target->mat.pos[2] - v[2];
                MoveCollisionObject(s->collIdx, delta);
                RotateCollisionObject(s->collIdx, target->mat.pos, mat, target->mat.uvs);
            }
        } else {
            if (path->flags & 0x20) {
                CollisionTargetPtrs[numCollisionTargetPtrs++] = target;
            }
            mx = 0.0f;
            for (i = 0; i < path->numPts; i++) {
                if (mx < PATH_PTS(path)[i].speed) {
                    mx = PATH_PTS(path)[i].speed;
                }
            }
            state->maxVel = mx;
            for (i = 0; i < path->numPts; i++) {
                if ((PATH_PTS(path)[i].flags & 1) && !(PATH_PTS(path)[i].flags & 0x1000)) {
                    ResetPath(target, i);
                    PATH_PTS(path)[i].flags |= 0x1000;
                    i = path->numPts;
                }
            }
            if (info->objListIdx == 361) {
                target->frames = 4;
            } else if (info->objListIdx == 365) {
                target->frames = 20;
            }
            if ((path->flags & 0x40) && !(path->flags & 0x4000)) {
                path->flags &= ~0x100000;
                path->flags |= 0x200400;
                if (!(path->flags & 0x8000)) {
                    ZOID_SetObjectDef_TargetPaths(target->dlIdx, objects[OBJ_PATH_STOPPED]);
                }
            }
            node = GrabEnvEntry();
            if (node != NULL) {
                node->index = 0;
                node->func = info->visFunc;
                node->data = PC_N64W(target);
                if (info->objListIdx == 361) {
                    node->timeStamp = 0.25f;
                } else if (info->objListIdx == 365) {
                    node->timeStamp = 0.0425f;
                }
                node->next = gEnvList;
                gEnvList = node;
            }
        }
        target->Flags |= TARGET_SET_UP;
    }
}

/* A path target's environment visual (MAT3 _AnimatePathTarget), every frame of a race or the demo while not paused;
 * `flag` 0 releases the visual. First a path worked by a trigger follows it: it starts, stops, turns round or swaps
 * its object definition (moving, stopped) as its trigger path stops and starts. Then a path not stopped advances by
 * the frame's time over as many points as that takes: forwards to the end, then round again (a loop), back (back
 * and forth) or stop (stops at its end); backwards to point 0, then forwards again or stop; a point flagged 0x40
 * stops it. Then it is placed (PlacePathTargetPos, PlacePathTargetQuat, unless its point holds them) and its sound
 * kept up; an animated kind steps its object definition through its frames; a mini model (collFuncIdx 4) follows
 * its velocity; its scale changes evenly between points. A target no longer active goes: its scene object, its
 * node and this visual. */
void AnimatePathTarget(struct pc_visual *node, s16 flag) {
    u16 *objects = (u16 *) &gObjList;   /* the object definitions by kind */
    tTarget *target;
    tTargetStructPath *state;
    tBasicTargetPath *path;
    tBasicTargetPath *trig;
    tTargetStructPath *trigState;
    TargInfo *info;
    u16 m;
    s32 idx;
    s32 i;
    s32 nx;
    s8 moved;
    f32 dv[3];
    f32 sv[3];
    f32 f;
    f32 tt;
    f32 fr;

    moved = 0;
    if (flag == 0) {
        ReleaseEnviron(node, 1);
        return;
    }
    if (((gGameState & 0x7C0000) || (gGameState & GS_DEMO)) && gPauseState == 0) {
        target = VISUAL_TARGET(node);
        state = TARGET_STATE(target);
        path = STATE_PATH(state);
        if (path->flags & 0x40) {
            if (path->flags & 0x4000) {
                if (!(path->flags & 0x200)) {
                    if (!(path->flags & 0x400)) {
                        if (state->flags & TPATH_FORWARD) {
                            state->flags &= ~TPATH_FORWARD;
                            state->flags |= TPATH_BACKWARD;
                            state->timeElapsed = PATH_PTS(path)[state->ptOn].timeToNext - state->timeElapsed;
                        }
                        if (path->flags & 0x100) {
                            PlayStartSound(target);
                        }
                        path->flags = path->flags & ~0x100;
                    }
                } else {
                    if (path->flags & 0x400) {
                        path->flags = path->flags & ~0x500;
                        PlayStartSound(target);
                    }
                    path->flags = path->flags & ~0x200;
                }
            } else {
                if ((path->flags & 0x2000) && (path->flags & 0x100000)) {
                    if (PATH_TRIGGER(path)->flags & 0x100) {
                        path->flags &= ~0x100000;
                        path->flags |= 0x200400;
                        if (!(path->flags & 0x8000)) {
                            ZOID_SetObjectDef_TargetPaths(target->dlIdx, objects[OBJ_PATH_STOPPED]);
                        }
                        PlayStopSound(target);
                        return;
                    }
                }
                if ((path->flags & 0x400) && (path->flags & 0x200)) {
                    trig = PATH_TRIGGER(path);
                    if (trig->flags & 0x100) {
                        trig->flags &= ~0x100;
                    } else if (path->flags & 0x1000) {
                        trigState = PATH_PARENT(trig);
                        if (trigState->flags & TPATH_BACKWARD) {
                            trigState->flags &= ~TPATH_BACKWARD;
                            trigState->flags |= TPATH_FORWARD;
                            trigState->timeElapsed = PATH_PTS(trig)[trigState->ptOn].timeToNext - trigState->timeElapsed;
                        } else {
                            trigState->flags &= ~TPATH_FORWARD;
                            trigState->flags |= TPATH_BACKWARD;
                            trigState->timeElapsed = PATH_PTS(trig)[trigState->ptOn].timeToNext - trigState->timeElapsed;
                        }
                    }
                    path->flags = path->flags & ~0x700;
                    if (path->flags & 0x1000) {
                        if (path->flags & 0x200000) {
                            path->flags &= ~0x200000;
                            path->flags |= 0x100000;
                            if (!(path->flags & 0x8000)) {
                                ZOID_SetObjectDef_TargetPaths(target->dlIdx, objects[OBJ_PATH_MOVING]);
                            }
                            PlayStartSound(target);
                        } else {
                            path->flags &= ~0x100000;
                            path->flags |= 0x200000;
                            if (!(path->flags & 0x8000)) {
                                ZOID_SetObjectDef_TargetPaths(target->dlIdx, objects[OBJ_PATH_STOPPED]);
                            }
                            PlayStopSound(target);
                        }
                    } else if (path->flags & 0x200000) {
                        path->flags &= ~0x200000;
                        path->flags |= 0x100000;
                        if (!(path->flags & 0x8000)) {
                            ZOID_SetObjectDef_TargetPaths(target->dlIdx, objects[OBJ_PATH_MOVING]);
                        }
                        PlayStartSound(target);
                    }
                }
            }
        }
        if (!(path->flags & 0x100)) {
            i = 0;
            state->timeElapsed += *(f32 *)(uintptr_t)&gFrameSeconds;
            idx = state->ptOn;
            do {
                fr = state->timeElapsed;
                f = PATH_PTS(path)[idx].timeToNext;
                if (f <= fr) {
                    state->timeElapsed = fr - f;
                    if (state->flags & TPATH_BACKWARD) {
                        if (idx == 0) {
                            if (path->flags & 0x4000) {
                                ResetPath(target, 0);
                                i = 1;
                                PlayStopSound(target);
                            } else {
                                state->flags &= ~TPATH_BACKWARD;
                                state->flags |= TPATH_FORWARD;
                                if (path->flags & 0x80) {
                                    path->flags |= 0x100;
                                    i = 1;
                                    state->timeElapsed = 0.0f;
                                }
                            }
                            if (path->flags & 0x20) {
                                ResetCollisionObject(path->collIdx);
                            }
                        } else {
                            state->ptOn = idx - 1;
                        }
                    } else if (idx + 1 == path->numPts) {
                        if (path->flags & 0x80) {
                            state->flags &= ~TPATH_FORWARD;
                            state->flags |= TPATH_BACKWARD;
                            state->ptOn = idx - 1;
                        } else {
                            state->ptOn = 0;
                            if (path->flags & 0x20) {
                                ResetCollisionObject(path->collIdx);
                            }
                        }
                    } else {
                        state->ptOn = idx + 1;
                        idx = state->ptOn;
                        if (PATH_PTS(path)[idx].flags & 0x40) {
                            i = 1;
                            path->flags |= 0x100;
                            state->timeElapsed = 0.0f;
                        } else if (idx + 1 == path->numPts) {
                            if (path->flags & 1) {
                                state->flags &= ~TPATH_FORWARD;
                                state->flags |= TPATH_BACKWARD;
                                state->ptOn = idx - 1;
                            } else if (path->flags & 0x4000) {
                                state->flags &= ~TPATH_FORWARD;
                                state->flags |= TPATH_BACKWARD;
                                state->ptOn = idx - 1;
                                state->timeElapsed = 0.0f;
                                path->flags |= 0x100;
                            } else if (!(path->flags & 2)) {
                                ResetPath(target, 0);
                                if (path->flags & 0x20) {
                                    ResetCollisionObject(path->collIdx);
                                }
                            }
                        }
                    }
                    idx = state->ptOn;
                } else {
                    i = 1;
                }
            } while (i == 0);
            idx = PATH_PTS(path)[idx].flags;
            if (!(idx & 2)) {
                PlacePathTargetPos(target);
                moved = 1;
                idx = PATH_PTS(path)[state->ptOn].flags;
            }
            if (!(idx & 0x10) || !(idx & 4)) {
                moved = 1;
                PlacePathTargetQuat(target);
            }
            if (moved != 0) {
                if (path->flags & 0x100) {
                    PlayStopSound(target);
                } else {
                    PlayMoveSound(target);
                }
            } else {
                PlayStopSound(target);
            }
            info = &targetInfo[target->infoIdx];
            if (info->objListIdx != -1) {
                node->timeStamp -= *(f32 *)(intptr_t)&gFrameSeconds;
                if (node->timeStamp <= 0.0f) {
                    idx = info->objListIdx;
                    if (idx == 0x169) {
                        node->timeStamp = 0.25f;
                    } else if (idx == 0x16D) {
                        node->timeStamp = 0.0425f;
                    }
                    idx = ++node->index;
                    if (idx >= target->frames) {
                        node->index = 0;
                        idx = 0;
                    }
                    flag = target->otype + idx;
                    if (flag != target->type) {
                        m = objects[flag];
                        trig = PATH_TRIGGER(path);
                        if (trig) {} /* the N64 code reads the trigger here and does nothing with it */
                        if (path->flags & 0x8000) {
                        } else {
                            ZOID_SetObjectDef_TargetPaths(target->dlIdx, m);
                        }
                        target->type = flag;
                    }
                    goto motion; /* MAT3 returns when the frame timer has not run out; so does this, past the else */
                }
            } else {
motion:
                if (info->collFuncIdx == 4) {
                    if (state->flags & TPATH_BACKWARD) {
                        dv[0] = PATH_PTS(path)[state->ptOn].norm[0] * -state->curVel;
                        dv[1] = PATH_PTS(path)[state->ptOn].norm[1] * -state->curVel;
                        dv[2] = PATH_PTS(path)[state->ptOn].norm[2] * -state->curVel;
                    } else {
                        dv[0] = PATH_PTS(path)[state->ptOn].norm[0] * state->curVel;
                        dv[1] = PATH_PTS(path)[state->ptOn].norm[1] * state->curVel;
                        dv[2] = PATH_PTS(path)[state->ptOn].norm[2] * state->curVel;
                    }
                    UpdateMiniModelStruct(target->owner, dv, target->mat.pos, target->mat.uvs);
                }
                idx = state->ptOn;
                if (!(PATH_PTS(path)[idx].flags & 0x10)) {
                    nx = idx + 1;
                    if (nx >= path->numPts) {
                        nx = 0;
                    }
                    if (state->flags & TPATH_BACKWARD) {
                        f = PATH_PTS(path)[idx].timeToNext;
                        tt = f - state->timeElapsed;
                    } else {
                        tt = state->timeElapsed;
                        f = PATH_PTS(path)[idx].timeToNext;
                    }
                    fr = tt / f;
                    if (PATH_PTS(path)[idx].timeToNext != 0.0f) {} /* likewise, a test that does nothing */
                    for (i = 0; i < 3; i++) {
                        sv[i] = PATH_PTS(path)[state->ptOn].scale[i] + (PATH_PTS(path)[nx].scale[i] - PATH_PTS(path)[state->ptOn].scale[i]) * fr;
                    }
                    for (i = 0; i < 3; i++) {
                        target->mat.uvs[0][i] *= sv[0];
                        target->mat.uvs[1][i] *= sv[1];
                        target->mat.uvs[2][i] *= sv[2];
                    }
                }
                if (!(target->Flags & TARGET_ACTIVE)) {
                    if (!(path->flags & 0x8000)) {
                        RemoveObjectTree(target->dlIdx, 0, 0);
                    }
                    FreeStatLLE(&gTargetList, (s32 *) target);
                    ReleaseEnviron(node, 1);
                }
            }
        }
    }
}
