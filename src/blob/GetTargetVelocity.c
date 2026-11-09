#include "game/game.h"

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
/* PC: a camera (a level-object node), its state CamCtl (a heap record of its category) and the scene, a track
 * WAD's PTHD record with its key list, keep their N64 layout: their pointers are 32-bit window addresses
 * (docs/port/POINTERS.md 10). */
enum n64p_CamCtl { n64p_CamCtl_ };
enum n64p_CamScene { n64p_CamScene_ };
enum n64p_CamKey { n64p_CamKey_ };
typedef struct CamScene {
    u8 pad00[0x10];
    s32 flags;                   /* 0x10 */
    s16 count;                   /* 0x14 */
    s16 id;                      /* 0x16 */
    enum n64p_CamScene link;     /* 0x18 */
    enum n64p_CamKey keys;       /* 0x1C */
} CamScene;
typedef struct CamCtl {
    enum n64p_CamScene scene;    /* 0x00 */
    f32 t;                       /* 0x04 */
    u8 pad08[4];
    s16 idx;                     /* 0x0C */
    u16 mode;                    /* 0x0E */
    f32 f10;                     /* 0x10 */
} CamCtl;
typedef struct Camera {
    u8 pad00[0x6C];
    enum n64p_CamCtl ctl;        /* 0x6C */
} Camera;
#define N64P(x) _Generic((x), enum n64p_CamCtl: (CamCtl *)PC_N64P(x), enum n64p_CamScene: (CamScene *)PC_N64P(x), enum n64p_CamKey: (CamKey *)PC_N64P(x), default: (x))

extern Camera *CollisionTargetPtrs[];

void GetTargetVelocity(s32 id, f32 *out) {
    s32 i;
    CamCtl *ctl;
    CamScene *sc;

    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    for (i = 0; i < numCollisionTargetPtrs; i++) {
        ctl = N64P(CollisionTargetPtrs[i]->ctl);
        sc = N64P(ctl->scene);
        if (id == sc->id) {
            if (ctl->mode & 8) {
                out[0] = N64P(sc->keys)[ctl->idx].dir[0] * -ctl->f10;
                out[1] = N64P(sc->keys)[ctl->idx].dir[1] * -ctl->f10;
                out[2] = N64P(sc->keys)[ctl->idx].dir[2] * -ctl->f10;
                return;
            }
            out[0] = N64P(sc->keys)[ctl->idx].dir[0] * ctl->f10;
            out[1] = N64P(sc->keys)[ctl->idx].dir[1] * ctl->f10;
            out[2] = N64P(sc->keys)[ctl->idx].dir[2] * ctl->f10;
            return;
        }
    }
}
