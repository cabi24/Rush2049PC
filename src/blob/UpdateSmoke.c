/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * UpdateSmoke @ 0x800CADA4, 2,420 bytes (from best.c, 377/605). Names are historical labels.
 *
 * Not a HUD: the per-frame update of the exhaust/smoke puff list gSmokeList_head (released into pool gSmokeList),
 * a relative of arcade game/visuals.c AnimateSmoke. Attached puffs (flags & 0x10) follow the car's
 * exhaust (orientation from the player direction, scale by speed, position = exhaust + velocity, +1.25 up,
 * frame timer); type 5 is the car glow (pulsing resource scale, position = player position + velocity.x * row0
 * + velocity.z * row2 + gCarMiddles[model] * row1); free puffs age, fade (alpha -= 9 below life 1), type 4
 * jitters with Random, animate, grow (+0.03 for type >= 6, +0.05 below: had these two swapped) and
 * move by velocity * frame time.
 *
 * Shaping this match depends on (each measured):
 *  - the jitter is arcade Random(max) = Random (kept, locked) on libc rand game_rand (kept, locked),
 *    defined here so umerge inlines them as it does in the unit: `Random(0.3f) - 0.15f`, `Random(0.25f)`,
 * `Random(0.15f) - 0.075f`; their inlined locals account for 8 of the frame-filler words padded;
 *  - attached path: `puff->position += velocity` is an inlined 3-statement static vadd (single call site);
 *  - both `player = &gGameCar[car]` assignments go through a (u32) cast: without it ugen emits
 *    `.noalias $18,$sp` and as1 hoists the player loads above the stack-vector stores;
 *  - the type-5 body is wrapped in `do { ... } while (0)` (a block-structured macro in the original, most
 *    likely): it moves the closing `.alias` of the model pointer (a0) before the arm's jump, so as1's
 *    cross-block hoisting stays on (`lui t7` into the bne delay slot, `lui at,0x3f80`);
 *  - in type 5, `scale` is not a variable: gCarMiddles[D_8014A250[car].model] is written in each of the three
 *    row-1 terms (one load, CSE'd), which gives retail's `mul.s f, $f0, row` operand order; the stack vector
 *    adds are written `position[k] = position[k] + ...` (operand order of the add);
 * - `gFrameSeconds` is volatile, with a discarded read (`lw zero`); frame filler pad1[2], pad2[5] .
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first
 * on a little-endian host (docs/port/ENDIAN.md). */
typedef union Id { s32 handle; struct { s16 index, high; } part; } Id;
typedef struct Puff {
    struct Puff *next;  /* 0 */
    Mat3 matrix;        /* 4 */
    Vec3 position;      /* 40 */
    Id id;              /* 52 */
    u8 pad56[4];
    f32 life;           /* 60 */
    f32 timer;          /* 64 */
    Vec3 velocity;      /* 68 */
    u32 flags;          /* 80 */
    s16 car;            /* 84 */
    s16 frame;          /* 86 */
} Puff;
typedef struct Player {
    u8 pad0[8];
    Vec3 position;      /* 8 */
    Vec3 direction;     /* 20 */
    u8 pad32[12];
    Mat3 matrix;        /* 44 */
    u8 pad80[36];
    Vec3 exhaust[4];    /* 116 */
    u8 pad164[84];
    s16 speed;          /* 248 */
    u8 pad250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad250_308[0x35C - 0x308];
    s8 slot;            /* 860 */
    s8 view;            /* 861 */
    u8 pad862[0x380 - 0x35E]; void *input; u8 pad862_384[0x3B8 - 0x384];
} Player;
typedef struct Car {
    u8 pad0[8];
    u8 model;           /* 8 */
    u8 pad9[1555];
    u16 exhaust[4];     /* 1564 */
    u8 pad1572[28];
    s8 enabled;         /* 1600 */
    u8 pad1601[455];
} Car;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Resource {
    u32 flags, w4;
    void *obj;
    f32 scale;          /* N64 12 */
    f32 w;
    s16 frame;          /* N64 20 */
    s16 child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    u8 prim[3];
    u8 alpha;           /* N64 63 */
    u8 pad64[4];
} Resource;
extern Puff *gSmokeList_head;
extern s32 gSmokeList;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the tire-effect latches are pc_tire_fx, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx
extern s32 rand_seed;
extern Resource ObjInstanceList[];
extern u16 gObjList_smoke[2][9];
extern f32 gCarMiddles[];
extern s8 expansion_pak_present, gTrackNum;
extern volatile f32 gFrameSeconds;
extern void MBOX_HideObject(s32, s32, u32);
extern void MBOX_ShowObject(s32, s32, u32);
extern f32 NormalVector(f32 *);
extern void LookInDir(f32 *, f32 [3][3]);
extern void CopyUV(void *, void *);
extern void RemoveObjectTree(s16, s32, s32);
extern void FreeStatLLE(void *, void *);
extern f32 fabsf(f32);
#pragma intrinsic(fabsf)
/* PC: game.frame_rate 60 (pc_game_fps.h). The glow's pulse and the puffs' fade, jitter and growth are per frame of
 * two retraces on the N64: scaled to the frame, or run once a 30 Hz frame; PC_ON30(c) is c on a frame that
 * completes a 30 Hz frame, false on the others. */
#define PC_ON30(c) ((c) && (!pc_fps60 || pc_frame_steps))

f32 Random(f32 max);

static void resource_set_frame(s16 index, u16 frame)
{
    ObjInstanceList[index].frame = frame;
}

static void vadd(f32 *a, f32 *b, f32 *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

void UpdateSmoke(void)
{
    Puff *puff;
    s16 car;
    s16 type;
    s16 speed;
    s16 aspeed;
    f32 scale;
    s32 i, j;
    Player *player;
    Vec3 position;
    Vec3 direction;
    s32 pad1[2];
    Puff *next;
    s32 pad2[5];
    Mat3 matrix;

    for (puff = gSmokeList_head; puff != 0; puff = next) {
        next = puff->next;
        car = puff->car;
        type = (puff->flags >> 17) & 0xF;
        if (puff->flags & 0x10) {
            player = (Player *)(uintptr_t)PC_CAR(car);
            speed = player->speed >> 2;
            aspeed = fabsf((f32)speed);
            if (player->view < 2) {
                MBOX_HideObject(puff->id.handle, 1, 1 << player->slot);
            } else {
                MBOX_ShowObject(puff->id.handle, 0, 1 << player->slot);
            }
            if (PC_MODEL_VISCODE(car)[type] != 2 || aspeed < 10) {
                switch (type) {
                case 0:
                    gCarSmokeDir[car] &= ~0x100;
                    break;
                case 1:
                    gCarSmokeDir[car] &= ~0x200;
                    break;
                case 2:
                    gCarSmokeDir[car] &= ~0x400;
                    break;
                case 3:
                    gCarSmokeDir[car] &= ~0x800;
                    break;
                }
                goto kill;
            }
            direction[0] = player->direction[0];
            direction[1] = player->direction[1];
            direction[2] = player->direction[2];
            NormalVector(direction);
            LookInDir(direction, matrix);
            CopyUV(matrix, puff->matrix);
            if (aspeed < 90) {
                if (speed < 0) {
                    scale = aspeed / 70.0f * 0.75f + 0.25f;
                } else {
                    scale = aspeed / 90.0f * 0.85f + 0.15f;
                }
                for (i = 0; i < 3; i++) {
                    for (j = 0; j < 3; j++) {
                        puff->matrix[i][j] *= scale;
                    }
                }
            }
            puff->position[0] = player->exhaust[type][0];
            puff->position[1] = player->exhaust[type][1];
            puff->position[2] = player->exhaust[type][2];
            vadd(puff->position, puff->velocity, puff->position);
            puff->position[1] += 1.25f;
            puff->timer -= gFrameSeconds;
            if (puff->timer <= 0.0f) {
                resource_set_frame(puff->id.part.index, gObjList_smoke[1][puff->frame]);
                puff->frame++;
                if (puff->frame >= 8) {
                    puff->frame = 0;
                }
                puff->timer = 0.0333333f;
            }
        } else if (type == 5) {
            do {
                if (!PC_MODEL_CRASHFLAG(car)) {
                    goto kill;
                }
                if (puff->frame == -1) {
                    if (ObjInstanceList[puff->id.handle].scale < puff->life) {
                        ObjInstanceList[puff->id.handle].scale += PC_FSCALE(0.04f);
                    } else {
                        puff->frame = 0;
                    }
                } else {
                    if (puff->frame < 8) {
                        ObjInstanceList[puff->id.handle].scale -= PC_FSCALE(0.03f);
                    } else {
                        ObjInstanceList[puff->id.handle].scale += PC_FSCALE(0.03f);
                    }
                    if (PC_ON30(puff->frame >= 14)) {
                        puff->frame = 0;
                    } else {
                        PC_EACH30(puff->frame++;)
                    }
                }
                player = (Player *)(uintptr_t)PC_CAR(car);
                position[0] = player->position[0];
                position[1] = player->position[1];
                position[2] = player->position[2];
                position[0] = position[0] + player->matrix[0][0] * puff->velocity[0];
                position[1] = position[1] + player->matrix[0][1] * puff->velocity[0];
                position[2] = position[2] + player->matrix[0][2] * puff->velocity[0];
                position[0] = position[0] + player->matrix[2][0] * puff->velocity[2];
                position[1] = position[1] + player->matrix[2][1] * puff->velocity[2];
                position[2] = position[2] + player->matrix[2][2] * puff->velocity[2];
                position[0] = position[0] + player->matrix[1][0] * gCarMiddles[PC_MODEL_BODY(car)];
                position[1] = position[1] + player->matrix[1][1] * gCarMiddles[PC_MODEL_BODY(car)];
                position[2] = position[2] + player->matrix[1][2] * gCarMiddles[PC_MODEL_BODY(car)];
                puff->position[0] = position[0];
                puff->position[1] = position[1];
                puff->position[2] = position[2];
            } while (0);
        } else {
            puff->life -= gFrameSeconds;
            if (puff->life <= 0.0f) {
kill:
                RemoveObjectTree(puff->id.part.index, 0, 0);
                FreeStatLLE(&gSmokeList, puff);
                continue;
            }
            gFrameSeconds;
            if (puff->life < 1.0f) {
                if (ObjInstanceList[puff->id.handle].alpha >= 9) {
                    PC_EACH30(ObjInstanceList[puff->id.handle].alpha -= 9;)
                }
            }
            if (type == 4) {
                PC_EACH30(puff->position[0] += Random(0.3f) - 0.15f;)
                PC_EACH30(puff->position[1] += Random(0.25f);)
                PC_EACH30(puff->position[2] += Random(0.15f) - 0.075f;)
                ObjInstanceList[puff->id.handle].scale += PC_FSCALE(0.05f);
            }
            puff->timer -= gFrameSeconds;
            if (puff->timer <= 0.0f) {
                if (expansion_pak_present || gTrackNum >= 6) {
                    resource_set_frame(puff->id.part.index, gObjList_smoke[0][puff->frame]);
                    if (puff->flags & 1) {
                        puff->frame++;
                        if (puff->frame >= 8) {
                            puff->frame = 0;
                        }
                    } else {
                        puff->frame--;
                        if (puff->frame < 0) {
                            puff->frame = 7;
                        }
                    }
                }
                puff->timer = 0.0333333f;
            }
            if (type >= 6) {
                ObjInstanceList[puff->id.handle].scale += PC_FSCALE(0.03f);
            } else {
                ObjInstanceList[puff->id.handle].scale += PC_FSCALE(0.05f);
            }
            puff->position[0] += puff->velocity[0] * gFrameSeconds;
            puff->position[1] += puff->velocity[1] * gFrameSeconds;
            puff->position[2] += puff->velocity[2] * gFrameSeconds;
        }
    }
}
