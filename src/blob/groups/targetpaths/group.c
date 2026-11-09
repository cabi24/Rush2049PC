#include "game/game.h"

/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Camera unit cluster (builds on). All camera-unit functions in one file:
 * PlayStartSound, PlayStopSound, ResetPath, PlacePathTargetQuat, camera_look_at_point,
 * PlacePathTargetPos, ZOID_SetObjectDef_TargetPaths (the inlined slot setter = retail stub), StartPathTarget,
 * AnimatePathTarget.
 * the whole-program unit --tag score StartPathTarget ZOID_SetObjectDef_TargetPaths AnimatePathTarget PlacePathTargetQuat
 *       camera_look_at_point PlacePathTargetPos PlayStartSound PlayStopSound ResetPath
 *       --with group.c --internal ZOID_SetObjectDef_TargetPaths --neighbours
 * change to a locked member: ResetPath's parameters are (Camera *cam, s32 idx) -- retail
 * passes cam (IPA register s3) before idx (a0); the code of ResetPath and StartPathTarget
 * is unchanged (both EQUAL). Residuals and every disclosed quirk: .
 * : AnimatePathTarget body replaced by (5/719; two disclosed compiled-out
 * reads, see its header and). The other members are byte-identical to group.c.
 * : AnimatePathTarget body replaced by (EQUAL 719/719; adds the
 * disclosed `idx * 1 + 1 == sc->count` identity at the two do-loop end tests, see its header and
 *); D_80123E8C is now the natural literal 0.0425f (own.rodata) and its
 * extern declaration is dropped. Every other function is byte-identical to group.c.
 */
float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

/* : camera structures (hand-derived from the retail words) */
/* PC: a target info record (targetInfo) is 0x40 bytes: its name, file, start function and visual function (w0C)
 * are pointers. A camera (a level-object node), its state CamCtl (a heap record of its category) and the scene, a
 * track WAD's PTHD record with its key list, keep their N64 layout: their pointers are 32-bit window addresses
 * (docs/port/POINTERS.md 10). */
enum n64p_Camera { n64p_Camera_ };
enum n64p_CamCtl { n64p_CamCtl_ };
enum n64p_CamScene { n64p_CamScene_ };
enum n64p_CamKey { n64p_CamKey_ };
/* Every field is spelled out, whichever view the loader converts the ROM's records by: +0x16 and +0x17 are the
 * kind's category and element, +0x18 its value. */
typedef struct CamTbl {
    char *name, *file;
    void *start;
    void *w0C;
    s16 mode;                    /* 0x10 */
    s16 s12;
    s16 s14;
    s8 category, element;
    f32 value;
    s32 trackA;                  /* 0x1C */
    s32 s20;
    s32 trackB;                  /* 0x24 */
    s32 s28;
    f32 f2C;
} CamTbl;
VIEW(CamTbl[], targetInfo);
#define targetInfo targetInfo_view
#define N64P(x) _Generic((x), enum n64p_Camera: (struct Camera *)PC_N64P(x), enum n64p_CamCtl: (struct CamCtl *)PC_N64P(x), enum n64p_CamScene: (struct CamScene *)PC_N64P(x), enum n64p_CamKey: (CamKey *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
typedef struct CamNode {         /* pool entry from GrabEnvEntry */
    struct CamNode *next;        /* 0x00 */
    s16 s04;
    u8 pad06[6];
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
    enum n64p_Camera cam;        /* 0x0C: the camera's window address */
    f32 f10;                     /* 0x10 */
    void *w14;
} CamNode;

typedef struct CamKey {          /* 0x44 bytes */
    f32 pos[3];                  /* 0x00 */
    f32 dir[3];                  /* 0x0C */
    f32 scale[3];                /* 0x18 */
    f32 rot[4];                  /* 0x24 */
    f32 f34;
    f32 dur;                     /* 0x38 */
    f32 f3C;
    s32 flags;                   /* 0x40 */
} CamKey;
typedef struct CamScene {
    u8 pad00[0x10];
    s32 flags;                   /* 0x10 */
    s16 count;                   /* 0x14 */
    s16 id;                      /* 0x16 */
    enum n64p_CamScene link;     /* 0x18 */
    enum n64p_CamKey keys;       /* 0x1C */
    enum n64p_CamCtl cur;        /* 0x20 */
} CamScene;
typedef struct CamCtl {
    enum n64p_CamScene scene;    /* 0x00 */
    f32 t;                       /* 0x04 */
    u8 pad08[4];
    s16 idx;                     /* 0x0C */
    u16 mode;                    /* 0x0E */
    f32 f10;                     /* 0x10 */
    f32 f14;
    f32 look[3];                 /* 0x18 */
    f32 mat[9];                  /* 0x24 */
} CamCtl;
typedef struct Camera {
    u8 pad00[4];
    u8 flags;                    /* 0x04 */
    /* PC: the slot is the low half of the camera node's handle (an s32 at +0xC), at +0xC on a little-endian
     * host, not +0xE: the view reads the word (docs/port/POINTERS.md 10). */
    u8 pad05[7];
    s32 slot;                    /* 0x0C */
    s16 tbl;                     /* 0x10 */
    u8 pad12[2];
    f32 m[3][3];                 /* 0x14 */
    f32 pos[3];                  /* 0x38 */
    u8 pad44[0xC];
    s16 s50;
    u8 pad52[6];
    s16 s58;
    s16 s5A;
    u8 pad5C[4];
    s32 handle;                  /* 0x60 */
    s8 state;                    /* 0x64 */
    s8 s65;
    u8 pad66[6];
    enum n64p_CamCtl ctl;        /* 0x6C */
} Camera;

VIEW(s32, gGameState);
#define gGameState gGameState_view

typedef struct ModelRecord {
    u8 pad000[0x7C6];
    u16 unk7C6;
    u8 pad7C8[0x7E8 - 0x7C8];
    u8 unk7E8;
} ModelRecord;
extern ModelRecord model;

VIEW(s32, unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view

VIEW(s32[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(struct CamNode *, gEnvList);
#define gEnvList gEnvList_view

extern struct Camera *CollisionTargetPtrs[];

VIEW(u16, gObjList);
#define gObjList gObjList_view

VIEW(s32, gTargetList);
#define gTargetList gTargetList_view

void AudUpdateEmitter(s32 arg0, intptr_t arg1, intptr_t arg2, f32 arg3, f32 arg4);

s32 AudEmitEffect(void *arg0, intptr_t arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6, s32 arg7, s32 arg8, s32 arg9);

struct CamNode *GrabEnvEntry(void);

/* the pair of tests around AudEmitEffect is an inlined helper */
static __inline s32 camera_track_entry(Camera *cam, f32 f2C, s32 id, s32 s28) {
    if (gGameSoundsEnabled == 0) {
        return -1;
    }
    if (id == -1) {
        return -1;
    }
    return AudEmitEffect(&cam->pos, (s32) &unitmatrix_origin, f2C, 0.0f, 1.0f, 0.0f, id, 0, s28, 0x80);
}

void PlayStartSound(Camera *cam) {
    CamTbl *t;

    AudStopEmitter(cam->handle);
    t = &targetInfo[cam->tbl];
    if (t->trackA != -1) {
        cam->handle = camera_track_entry(cam, t->f2C, t->trackA, t->s28);
    } else {
        cam->handle = -1;
    }
    cam->state = 0;
}

void PlayStopSound(Camera *cam) {
    CamTbl *t;

    if (cam->state != 2 && (cam->state != 0 || AudEmitterPlaying(cam->handle) == 0)) {
        AudStopEmitter(cam->handle);
        t = &targetInfo[cam->tbl];
        if (t->trackB != -1) {
            cam->handle = camera_track_entry(cam, t->f2C, t->trackB, t->s28);
        } else {
            cam->handle = -1;
        }
        cam->state = 2;
    }
}


void PlacePathTargetQuat(Camera *cam) {
    f32 out[4];
    f32 d[3];
    CamCtl *ctl;
    CamScene *sc;
    CamKey *k;
    CamKey *keys;
    f32 t;
    f32 dur;
    s32 next;
    s32 idx;
    s32 near;
    f32 dist;

    ctl = N64P(cam->ctl);
    sc = N64P(N64P(cam->ctl)->scene);
    idx = ctl->idx;
    k = &N64P(sc->keys)[idx];
    keys = N64P(sc->keys);
    if (k->flags & 0x20) {
        goto plain;
    }
    if (ctl->mode & 8) {
        t = k->dur;
        t -= ctl->t;
    } else {
        t = ctl->t;
    }
    dur = k->dur;
    if (dur == 0.0f) {
plain:
        CreateQuatMat(cam->m[0], k->rot, 1);
    } else {
        next = idx + 1;
        if (next >= sc->count && (sc->flags & 2)) {
            next = 0;
        }
        InterpQuats(t / dur, k->rot, keys[next].rot, out);
        CreateQuatMat(cam->m[0], out, 1);
    }
    if (sc->flags & 0x20) {
        near = 1;
        if (gRace == 5) {
            d[0] = cam->pos[0] - ((f32 *) PC_CARS)[2];
            d[1] = cam->pos[1] - ((f32 *) PC_CARS)[3];
            d[2] = cam->pos[2] - ((f32 *) PC_CARS)[4];
            dist = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (22500.0f < dist) {
                near = 0;
            }
        }
        if (near != 0) {
            RotateCollisionObject(sc->id, cam->pos, ctl->mat, cam->m[0]);
        }
    }
}

/* camera_look_at_point = targetpaths.c _PlayMoveSound: the Dreamcast decomp's text, which keeps two
 * N64-era empty statements GC dropped (`if ((maxVel < 0 && curVel > 0) || (maxVel > 0 && curVel < 0)) ;` = retail's
 * empty `bgez v1`, and `if ((int)ratio < 0 || (int)ratio > 1) ;` in the clamp); path_abs as a macro (MAT3's static
 * inline function costs 167 words); `0.75f * ratio` operand order (retail mul.s $f10,$f6,$f0). */
#define path_abs(x) ((x) < 0 ? -(x) : (x))

void PlayMoveSound(Camera *cam) {
    CamCtl *dPtr;
    f32 ratio;
    s32 maxVel;
    s32 curVel;

    if (cam->state == 1) {
        if (cam->handle != -1) {
            if (targetInfo[cam->tbl].s20 == 1) {
                dPtr = N64P(cam->ctl);
                maxVel = dPtr->f14;
                curVel = dPtr->f10;
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
            if (targetInfo[cam->tbl].s20 == 0x12)
                AudUpdateEmitter(cam->handle, (intptr_t) cam->pos, (intptr_t) &unitmatrix_origin, 0.8f, 1.0f);
            else if (targetInfo[cam->tbl].s20 == 0x61)
                AudUpdateEmitter(cam->handle, (intptr_t) cam->pos, (intptr_t) &unitmatrix_origin, 1.0f, 0.75f);
            else AudUpdateEmitter(cam->handle, (intptr_t) cam->pos, (intptr_t) &unitmatrix_origin, 0.75f * ratio + 0.25f, ratio);
        }
    } else if (cam->state == 2) PlayStartSound(cam);
    else if (!AudEmitterPlaying(cam->handle)) {
        AudStopEmitter(cam->handle);
        if (targetInfo[cam->tbl].s20 != -1) {
            cam->handle = camera_track_entry(cam, targetInfo[cam->tbl].f2C, targetInfo[cam->tbl].s20, targetInfo[cam->tbl].s28);
            if (targetInfo[cam->tbl].s20 == 1)
                AudUpdateEmitter(cam->handle, (intptr_t) cam->pos, (intptr_t) &unitmatrix_origin, 0.0f, 1.0f);
        } else cam->handle = -1;
        cam->state = 1;
    }
}

void ResetPath(Camera *cam, s32 idx) {
    CamCtl *ctl;
    CamScene *sc;
    s32 i;

    ctl = N64P(cam->ctl);
    sc = N64P(ctl->scene);
    if ((sc->flags & 1) && idx >= sc->count - 1) {
        ctl->mode = 8;
    } else {
        ctl->mode = 4;
    }
    ctl->idx = idx;
    ctl->t = 0.0f;
    ctl->f10 = 0.0f;
    cam->pos[0] = N64P(sc->keys)[ctl->idx].pos[0];
    cam->pos[1] = N64P(sc->keys)[ctl->idx].pos[1];
    cam->pos[2] = N64P(sc->keys)[ctl->idx].pos[2];
    ctl->look[0] = N64P(sc->keys)[ctl->idx].pos[0];
    ctl->look[1] = N64P(sc->keys)[ctl->idx].pos[1];
    ctl->look[2] = N64P(sc->keys)[ctl->idx].pos[2];
    if (sc->flags & 0xC0) {
        sc->flags |= 0x100;
        sc->cur = N64W(ctl);
    }
    if (sc->flags & 0x5000) {
        sc->flags |= 0x400;
    }
    CreateQuatMat(cam->m, N64P(sc->keys)[ctl->idx].rot, 1);
    CopyUV(cam->m, ctl->mat);
    TransposeUV(ctl->mat);
    for (i = 0; i < 3; i++) {
        cam->m[0][i] = N64P(sc->keys)[ctl->idx].scale[0] * cam->m[0][i];
        cam->m[1][i] = N64P(sc->keys)[ctl->idx].scale[1] * cam->m[1][i];
        cam->m[2][i] = N64P(sc->keys)[ctl->idx].scale[2] * cam->m[2][i];
    }
}

void PlacePathTargetPos(Camera *cam) {
    f32 v[3];
    f32 w[3];
    f32 u[3];
    CamCtl *ctl;
    CamScene *sc;
    CamKey *k;
    CamKey *keys;
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

    ctl = N64P(cam->ctl);
    idx = ctl->idx;
    sc = N64P(ctl->scene);
    k = &N64P(sc->keys)[idx];
    n = sc->count;
    keys = N64P(sc->keys);
    if (idx < n - 1 || (sc->flags & 2) != 0 || !(sc->flags & 0x80)) {
        if (k->flags & 0x10000008) {
            if (ctl->mode & 8) {
                t = k->dur - ctl->t;
            } else {
                t = ctl->t;
            }
            d = k->f34 * (t / k->dur);
        } else {
            next = idx + 1;
            if (next >= n && (sc->flags & 2)) {
                next = 0;
            }
            a = k->f3C;
            d = keys[next].f3C;
            dv = d - a;
            if (ctl->mode & 8) {
                t = k->dur - ctl->t;
            } else {
                t = ctl->t;
            }
            c = (t * t * dv) / (k->dur * 2);
            if (d < a) {
                d = a * t + c;
            } else {
                d = a * t + c;
            }
        }
        v[0] = k->dir[0];
        v[1] = k->dir[1];
        v[2] = k->dir[2];
        ctl->look[0] = cam->pos[0];
        ctl->look[1] = cam->pos[1];
        ctl->look[2] = cam->pos[2];
        v[0] = v[0] * d;
        v[1] = v[1] * d;
        v[2] = v[2] * d;
        for (i = 0; &v[i] < &v[3]; i++) {
            j = v[i] * 32.0f;
            v[i] = j * 0.03125f;
        }
        cam->pos[0] = k->pos[0] + v[0];
        cam->pos[1] = k->pos[1] + v[1];
        cam->pos[2] = k->pos[2] + v[2];
        v[0] = cam->pos[0] - ctl->look[0];
        v[1] = cam->pos[1] - ctl->look[1];
        v[2] = cam->pos[2] - ctl->look[2];
        ctl->f10 = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) / *(volatile f32 *) &gFrameSeconds;
        if (sc->flags & 0x20) {
            near = 1;
            if (gRace == 5) {
                w[0] = cam->pos[0] - ((f32 *) PC_CARS)[2];
                w[1] = cam->pos[1] - ((f32 *) PC_CARS)[3];
                w[2] = cam->pos[2] - ((f32 *) PC_CARS)[4];
                d = w[0] * w[0] + w[1] * w[1] + w[2] * w[2];
                if (202500.0f < d) {
                    near = 0;
                }
            }
            if (near != 0) {
                MoveCollisionObject(sc->id, v);
            }
        }
    }
}

/*
 * ZOID_SetObjectDef_TargetPaths (retail: caller-less `jr ra; nop` stub between AnimatePathTarget and StartPathTarget):
 * the slot setter ObjInstanceList[slot].x14 = v (0x44-byte records), inlined by umerge at every call site
 * (AnimatePathTarget x4, StartPathTarget x1) and therefore deleted.  Must stay INTERNAL (non-static,
 * not kept) so that the stub remains.  Its two parameters give retail's v0/v1 pair; with its two
 * locals it reserves the four frame slots per call site that both callers' frames need.
 */
/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. The node's
 * model id (N64 +0x14, D_8012E714 of node 0). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
void ZOID_SetObjectDef_TargetPaths(s32 slot, u16 v) {
    ((PcNode *) &ObjInstanceList)[slot].model = v;
}

/*
 * StartPathTarget(cam): first-frame set-up of a scripted camera.  state = 2; unless already started
 * (cam->flags & 1): reset the controller (mode 0, t 0); in gameplay mode 2 with a scene id > 0 snap the
 * camera to every key flagged 0x1000000 and do the first-person set-up from key 0; otherwise register the
 * camera (CollisionTargetPtrs[numCollisionTargetPtrs++]), store the largest key f3C in ctl->f14, build the view matrix of the
 * first key flagged 1 but not 0x1000 (and flag it 0x1000), set the s5A period from the table's s14
 * (361 -> 4, 365 -> 20), start the scene (flags 0x100000 -> 0x200400 and the entity slot value
 * definition gObjList[349] unless 0x8000) and link a new update node from GrabEnvEntry (period 0.25 / 0.0425).  Then
 * cam->flags |= 1.  N64-only (scripted attract/replay cameras); no arcade ancestor.
 * Quirks, all needed for the 248-byte frame (every stack offset checked against retail):
 *  - `u8 col[4] = {255, 0, 0, 255}`: initialised local aggregate; IDO copies it from an anonymous .data
 *    word (retail 0x8011750C = 0xFF0000FF) to 200(sp) and never reads it (debug colour whose user is
 *    compiled out).  Own .data: verified by the scorer/splice, not a reference to a named global.
 *  - DISCLOSED: `s32 unused[11]` (top 44 bytes, 204..247) and `unused1`/`unused2` (two slots between the
 *    function's locals and the mode-2 block) are unused locals.  They are the exact frame residual;
 *    nothing natural found explains them (block-scoped and inlined locals are always allocated below the
 *    function-level ones; natural splits of the existing variables change the code).
 *  - the mode-2 locals are block-scoped, `k` in its own loop-body block (allocated after `mat`).
 */
void StartPathTarget(Camera *cam) {
    u16 *objects = (u16 *) &gObjList;   /* the object definitions by kind (u16[]): a path target's when stopped at 349 */
    s32 unused[11];
    u8 col[4] = { 255, 0, 0, 255 };
    CamTbl *tbl;
    CamScene *sc;
    CamCtl *ctl;
    s32 i;
    f32 mx;
    CamNode *node;
    s32 unused1;
    s32 unused2;

    cam->state = 2;
    tbl = &targetInfo[cam->tbl];
    ctl = N64P(cam->ctl);
    sc = N64P(N64P(cam->ctl)->scene);
    if (!(cam->flags & 1)) {
        ctl->mode = 0;
        ctl->t = 0.0f;
        if (gRace == 2 && sc->id > 0) {
            CamScene *s;
            s32 j;
            f32 v[3];
            f32 delta[3];
            f32 mat[9];

            s = N64P(N64P(cam->ctl)->scene);
            for (j = 0; j < s->count; j++) {
                CamKey *k = &N64P(s->keys)[j];

                if (N64P(s->keys)[j].flags & 0x1000000) {
                    cam->pos[0] = k->pos[0];
                    cam->pos[1] = k->pos[1];
                    cam->pos[2] = k->pos[2];
                    CreateQuatMat(cam->m, k->rot, 1);
                }
            }
            if (s->flags & 0x20) {
                v[0] = N64P(s->keys)[0].pos[0];
                v[1] = N64P(s->keys)[0].pos[1];
                v[2] = N64P(s->keys)[0].pos[2];
                CreateQuatMat(mat, N64P(s->keys)[0].rot, 1);
                TransposeUV(mat);
                delta[0] = cam->pos[0] - v[0];
                delta[1] = cam->pos[1] - v[1];
                delta[2] = cam->pos[2] - v[2];
                MoveCollisionObject(s->id, delta);
                RotateCollisionObject(s->id, cam->pos, mat, cam->m);
            }
        } else {
            if (sc->flags & 0x20) {
                CollisionTargetPtrs[numCollisionTargetPtrs++] = cam;
            }
            mx = 0.0f;
            for (i = 0; i < sc->count; i++) {
                if (mx < N64P(sc->keys)[i].f3C) {
                    mx = N64P(sc->keys)[i].f3C;
                }
            }
            ctl->f14 = mx;
            for (i = 0; i < sc->count; i++) {
                if ((N64P(sc->keys)[i].flags & 1) && !(N64P(sc->keys)[i].flags & 0x1000)) {
                    ResetPath(cam, i);
                    N64P(sc->keys)[i].flags |= 0x1000;
                    i = sc->count;
                }
            }
            if (tbl->s14 == 361) {
                cam->s5A = 4;
            } else if (tbl->s14 == 365) {
                cam->s5A = 20;
            }
            if ((sc->flags & 0x40) && !(sc->flags & 0x4000)) {
                sc->flags &= ~0x100000;
                sc->flags |= 0x200400;
                if (!(sc->flags & 0x8000)) {
                    ZOID_SetObjectDef_TargetPaths(cam->slot, objects[349]);
                }
            }
            node = GrabEnvEntry();
            if (node != NULL) {
                node->s04 = 0;
                node->w14 = tbl->w0C;
                node->cam = N64W(cam);
                if (tbl->s14 == 361) {
                    node->f10 = 0.25f;
                } else if (tbl->s14 == 365) {
                    node->f10 = 0.0425f;
                }
                node->next = gEnvList;
                gEnvList = node;
            }
        }
        cam->flags |= 1;
    }
}

/* AnimatePathTarget (0x800C0AC0, 2,876 B): the camera-script key stepper.  Advances ctl->t by the frame time, walks
 * the scene's key list (sc->keys[], sc->count) forwards (mode 4) or backwards (mode 8) with wrap / ping-pong /
 * stop flags, then runs the spline / free-look / look-at updates, the per-node texture-index animation
 * (gObjList table, ZOID_SetObjectDef_TargetPaths setter) and the mode-4 key-direction drift.
 * : EQUAL in the whole-program unit (719/719 words, frame 248/248). Compile inside the camera
 * cluster (groups/targetpaths/group.c).
 * Shaping carried over from: ResetPath(cam, idx) parameter order;
 * m = gObjList[..] before the flag test and the empty-then `if (flags & 0x8000) {} else {setter}`; one variable
 * per register (idx = key flags / tb->s14 / s04 counter / block_99 index; fr = do-loop t and fraction; f = dur);
 * `cam->m[k][i] *= sv[k]`; `node->f10 -= dt`.
 * From: the s50 texture index is the parameter `flag` reused (s16), for its
 * formal-parameter copy relation (a1); DISCLOSED compiled-out read `lk = sc->link; if (lk) {}` after m's load
 * (a v1 holder: m -> a0, tb -> a2); DISCLOSED compiled-out read `if (sc->keys[idx].dur != 0.0f) {}` after the
 * division (tt -> f14).  Both are still needed (without them: 8 / 4 words).
 * New, DISCLOSED shaping device: both do-loop end tests are written `idx * 1 + 1 == sc->count`. The
 * `* 1` folds away (no code) but makes the idx side an expression, not a plain variable, when uopt's
 * readnxtinst builds the compare.  For equ/neq it keeps the operand order only when op1 is a plain variable and
 * otherwise swaps, and it moves a `var + const` constant to the other side; so `idx + 1 == count` becomes
 * equ(idx, count - 1) and `count == idx + 1` becomes equ(idx + 1, count), both idx-first.  With the identity op
 * the tree is equ(count, idx*1 + 1): ugen evaluates count first (t6 / t9), as retail does.  `(idx ^ 0)` and
 * `(idx >> 0)` give the same object. Traced.
 * No implicit int, no unused locals, no pad arrays.  0.0425f is AnimatePathTarget's own .rodata word at
 * 0x80123E8C (retail bits 0x3D2E147B, checked against the image).
 */
void AnimatePathTarget(CamNode *node, s16 flag) {
    u16 *objects = (u16 *) &gObjList;   /* the object definitions by kind (u16[]): a path target's when it starts (348), stops (349) */
    Camera *cam;
    CamCtl *ctl;
    CamScene *sc;
    CamScene *lk;
    CamCtl *lc;
    CamTbl *tb;
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
    if (((gGameState & 0x7C0000) || (gGameState & 8)) && gPauseState == 0) {
        cam = N64P(node->cam);
        ctl = N64P(cam->ctl);
        sc = N64P(ctl->scene);
        if (sc->flags & 0x40) {
            if (sc->flags & 0x4000) {
                if (!(sc->flags & 0x200)) {
                    if (!(sc->flags & 0x400)) {
                        if (ctl->mode & 4) {
                            ctl->mode &= ~4;
                            ctl->mode |= 8;
                            ctl->t = N64P(sc->keys)[ctl->idx].dur - ctl->t;
                        }
                        if (sc->flags & 0x100) {
                            PlayStartSound(cam);
                        }
                        sc->flags = sc->flags & ~0x100;
                    }
                } else {
                    if (sc->flags & 0x400) {
                        sc->flags = sc->flags & ~0x500;
                        PlayStartSound(cam);
                    }
                    sc->flags = sc->flags & ~0x200;
                }
            } else {
                if ((sc->flags & 0x2000) && (sc->flags & 0x100000)) {
                    if (N64P(sc->link)->flags & 0x100) {
                        sc->flags &= ~0x100000;
                        sc->flags |= 0x200400;
                        if (!(sc->flags & 0x8000)) {
                            ZOID_SetObjectDef_TargetPaths(cam->slot, objects[349]);
                        }
                        PlayStopSound(cam);
                        return;
                    }
                }
                if ((sc->flags & 0x400) && (sc->flags & 0x200)) {
                    lk = N64P(sc->link);
                    if (lk->flags & 0x100) {
                        lk->flags &= ~0x100;
                    } else if (sc->flags & 0x1000) {
                        lc = N64P(lk->cur);
                        if (lc->mode & 8) {
                            lc->mode &= ~8;
                            lc->mode |= 4;
                            lc->t = N64P(lk->keys)[lc->idx].dur - lc->t;
                        } else {
                            lc->mode &= ~4;
                            lc->mode |= 8;
                            lc->t = N64P(lk->keys)[lc->idx].dur - lc->t;
                        }
                    }
                    sc->flags = sc->flags & ~0x700;
                    if (sc->flags & 0x1000) {
                        if (sc->flags & 0x200000) {
                            sc->flags &= ~0x200000;
                            sc->flags |= 0x100000;
                            if (!(sc->flags & 0x8000)) {
                                ZOID_SetObjectDef_TargetPaths(cam->slot, objects[348]);
                            }
                            PlayStartSound(cam);
                        } else {
                            sc->flags &= ~0x100000;
                            sc->flags |= 0x200000;
                            if (!(sc->flags & 0x8000)) {
                                ZOID_SetObjectDef_TargetPaths(cam->slot, objects[349]);
                            }
                            PlayStopSound(cam);
                        }
                    } else if (sc->flags & 0x200000) {
                        sc->flags &= ~0x200000;
                        sc->flags |= 0x100000;
                        if (!(sc->flags & 0x8000)) {
                            ZOID_SetObjectDef_TargetPaths(cam->slot, objects[348]);
                        }
                        PlayStartSound(cam);
                    }
                }
            }
        }
        if (!(sc->flags & 0x100)) {
            i = 0;
            ctl->t += *(f32 *)(uintptr_t)&gFrameSeconds;
            idx = ctl->idx;
            do {
                fr = ctl->t;
                f = N64P(sc->keys)[idx].dur;
                if (f <= fr) {
                    ctl->t = fr - f;
                    if (ctl->mode & 8) {
                        if (idx == 0) {
                            if (sc->flags & 0x4000) {
                                ResetPath(cam, 0);
                                i = 1;
                                PlayStopSound(cam);
                            } else {
                                ctl->mode &= ~8;
                                ctl->mode |= 4;
                                if (sc->flags & 0x80) {
                                    sc->flags |= 0x100;
                                    i = 1;
                                    ctl->t = 0.0f;
                                }
                            }
                            if (sc->flags & 0x20) {
                                ResetCollisionObject(sc->id);
                            }
                        } else {
                            ctl->idx = idx - 1;
                        }
                    } else if (idx * 1 + 1 == sc->count) {
                        if (sc->flags & 0x80) {
                            ctl->mode &= ~4;
                            ctl->mode |= 8;
                            ctl->idx = idx - 1;
                        } else {
                            ctl->idx = 0;
                            if (sc->flags & 0x20) {
                                ResetCollisionObject(sc->id);
                            }
                        }
                    } else {
                        ctl->idx = idx + 1;
                        idx = ctl->idx;
                        if (N64P(sc->keys)[idx].flags & 0x40) {
                            i = 1;
                            sc->flags |= 0x100;
                            ctl->t = 0.0f;
                        } else if (idx * 1 + 1 == sc->count) {
                            if (sc->flags & 1) {
                                ctl->mode &= ~4;
                                ctl->mode |= 8;
                                ctl->idx = idx - 1;
                            } else if (sc->flags & 0x4000) {
                                ctl->mode &= ~4;
                                ctl->mode |= 8;
                                ctl->idx = idx - 1;
                                ctl->t = 0.0f;
                                sc->flags |= 0x100;
                            } else if (!(sc->flags & 2)) {
                                ResetPath(cam, 0);
                                if (sc->flags & 0x20) {
                                    ResetCollisionObject(sc->id);
                                }
                            }
                        }
                    }
                    idx = ctl->idx;
                } else {
                    i = 1;
                }
            } while (i == 0);
            idx = N64P(sc->keys)[idx].flags;
            if (!(idx & 2)) {
                PlacePathTargetPos(cam);
                moved = 1;
                idx = N64P(sc->keys)[ctl->idx].flags;
            }
            if (!(idx & 0x10) || !(idx & 4)) {
                moved = 1;
                PlacePathTargetQuat(cam);
            }
            if (moved != 0) {
                if (sc->flags & 0x100) {
                    PlayStopSound(cam);
                } else {
                    PlayMoveSound(cam);
                }
            } else {
                PlayStopSound(cam);
            }
            tb = &targetInfo[cam->tbl];
            if (tb->s14 != -1) {
                node->f10 -= *(f32 *)(intptr_t)&gFrameSeconds;
                if (node->f10 <= 0.0f) {
                    idx = tb->s14;
                    if (idx == 0x169) {
                        node->f10 = 0.25f;
                    } else if (idx == 0x16D) {
                        node->f10 = 0.0425f;
                    }
                    idx = ++node->s04;
                    if (idx >= cam->s5A) {
                        node->s04 = 0;
                        idx = 0;
                    }
                    flag = cam->s58 + idx;
                    if (flag != cam->s50) {
                        m = objects[flag];
                        lk = N64P(sc->link);
                        if (lk) {}
                        if (sc->flags & 0x8000) {
                        } else {
                            ZOID_SetObjectDef_TargetPaths(cam->slot, m);
                        }
                        cam->s50 = flag;
                    }
                    goto block_99;
                }
            } else {
block_99:
                if (tb->mode == 4) {
                    if (ctl->mode & 8) {
                        dv[0] = N64P(sc->keys)[ctl->idx].dir[0] * -ctl->f10;
                        dv[1] = N64P(sc->keys)[ctl->idx].dir[1] * -ctl->f10;
                        dv[2] = N64P(sc->keys)[ctl->idx].dir[2] * -ctl->f10;
                    } else {
                        dv[0] = N64P(sc->keys)[ctl->idx].dir[0] * ctl->f10;
                        dv[1] = N64P(sc->keys)[ctl->idx].dir[1] * ctl->f10;
                        dv[2] = N64P(sc->keys)[ctl->idx].dir[2] * ctl->f10;
                    }
                    UpdateMiniModelStruct(cam->s65, dv, cam->pos, cam->m);
                }
                idx = ctl->idx;
                if (!(N64P(sc->keys)[idx].flags & 0x10)) {
                    nx = idx + 1;
                    if (nx >= sc->count) {
                        nx = 0;
                    }
                    if (ctl->mode & 8) {
                        f = N64P(sc->keys)[idx].dur;
                        tt = f - ctl->t;
                    } else {
                        tt = ctl->t;
                        f = N64P(sc->keys)[idx].dur;
                    }
                    fr = tt / f;
                    if (N64P(sc->keys)[idx].dur != 0.0f) {}
                    for (i = 0; i < 3; i++) {
                        sv[i] = N64P(sc->keys)[ctl->idx].scale[i] + (N64P(sc->keys)[nx].scale[i] - N64P(sc->keys)[ctl->idx].scale[i]) * fr;
                    }
                    for (i = 0; i < 3; i++) {
                        cam->m[0][i] *= sv[0];
                        cam->m[1][i] *= sv[1];
                        cam->m[2][i] *= sv[2];
                    }
                }
                if (!(cam->flags & 2)) {
                    if (!(sc->flags & 0x8000)) {
                        RemoveObjectTree(cam->slot, 0, 0);
                    }
                    FreeStatLLE(&gTargetList, (s32 *) cam);
                    ReleaseEnviron(node, 1);
                }
            }
        }
    }
}

/* stand-in caller: keeps PlayStartSound out of line under -O3 */

/* stand-in caller: keeps PlayStopSound out of line under -O3 */

/* stand-in caller: keeps PlacePathTargetQuat out of line under -O3 */

/* stand-in caller: keeps camera_look_at_point out of line under -O3 */
