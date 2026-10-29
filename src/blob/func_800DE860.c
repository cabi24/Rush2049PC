/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * NOT a strict match: the scorer prints "MATCH (8 section-relative relocations
 * unverified: .rodata+0x0 ... .rodata+0xc ...)". Text words are identical;
 * the unverified references are the four float literals, emitted in the order
 * 0.1f, 1500.0f, 0.97f, 0.03f = retail words at 0x80124310/14/18/1C.
 *
 * N64 descendant of arcade set_catchup/no_catchup
 * (reference/repos/rushtherock/game/model.c): rubber-band catch-up scale at
 * car +0x400. k1/k2 are arcade max_catchup / cuzone as locals.
 *
 * What closed the old float-allocation residual (see
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * - literals instead of `extern f32 D_8012431x` (externs are loop-hoisted);
 * - arcade literal types: `k1 + 1.0f` but `... / k2 + 1` and `(1 - s) ... + 1`
 *   with int 1, which gives the two separate 1.0 webs (f14 and f20).
 * `(1 - s) / 2.0f + 1` matches as well as `* 0.5f`.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;

typedef struct {
    u8 pad0[7];
    u8 flag;
} Slot;

typedef struct {
    /* 0x000 */ u8 pad0[0x400];
    /* 0x400 */ f32 scale;
    /* 0x404 */ u8 pad404[0x7C6 - 0x404];
    /* 0x7C6 */ s16 order;
    /* 0x7C8 */ s16 active;
    /* 0x7CA */ s16 pad7CA;
    /* 0x7CC */ s8 kind;
    /* 0x7CD */ u8 pad7CD[0x7E6 - 0x7CD];
    /* 0x7E6 */ s16 rank;
    /* 0x7E8 */ u8 pad7E8[4];
    /* 0x7EC */ f32 outA;
    /* 0x7F0 */ f32 outB;
    /* 0x7F4 */ u8 pad7F4[0x808 - 0x7F4];
} Car;

typedef struct {
    /* 0x000 */ u8 pad0[8];
    /* 0x008 */ f32 pos[3];
    /* 0x014 */ u8 pad14[0xEE - 0x14];
    /* 0x0EE */ s8 place;
    /* 0x0EF */ u8 pad0EF[0x100 - 0xEF];
    /* 0x100 */ f32 dist;
#ifdef TARGET_PC
    /* 0x104 */ u8 pad104[0x110 - 0x104]; struct pc_visual visuals[21]; u8 pad104_308[0x356 - 0x308];
#else
    /* 0x104 */ u8 pad104[0x356 - 0x104];
#endif
    /* 0x356 */ s16 slot;
    /* 0x358 */ u8 pad358;
    /* 0x359 */ s8 state;
#ifdef TARGET_PC
    /* 0x35A */ u8 pad35A[0x380 - 0x35A]; void *input; u8 pad35A_384[0x3B8 - 0x384];
#else
    /* 0x35A */ u8 pad35A[0x3B8 - 0x35A];
#endif
} Rec;

#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Car D_8014A250[];
#define PC_MODEL_ACTIVE(i) D_8014A250[i].active
#define PC_MODEL_DRONE_TYPE(i) D_8014A250[i].kind
#define PC_MODEL_SCALE(i) D_8014A250[i].scale
#endif
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Rec player_array[];
#define PC_CAR_DISTANCE(i) player_array[i].dist
#define PC_CAR_STATE(i) player_array[i].state
#endif
extern s16 active_player_count;
extern s8 D_80152030;
extern s8 D_80150F14;
#ifdef TARGET_PC
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Slot D_80153E88[];
#define PC_LINK_OWNER(i) D_80153E88[i].flag
#endif

void func_800DE860(void)
{
    s16 i;
    s16 best;
    s32 j;
    f32 s;
    f32 d;
    f32 bd;
    f32 k1;
    f32 k2;

    if ((active_player_count == 1 && D_80152030 < 5) || D_80150F14 == 0) {
        for (j = 0; j < 6; j++) {
            if (PC_LINK_OWNER(j) == 0 || PC_LINK_OWNER(j) == 6) {
                PC_MODEL_SCALE(j) = 1.0f;
            }
        }
    } else {
        best = -1;
        for (i = 0; i < 6; i++) {
            if (PC_MODEL_ACTIVE(i) != 0 && PC_CAR_STATE(i) < 2 &&
                (PC_MODEL_DRONE_TYPE(i) == 2 || D_80152030 == 5)) {
                if (best == -1) {
                    best = i;
                } else if (PC_CAR_DISTANCE(best) < PC_CAR_DISTANCE(i)) {
                    best = i;
                }
            }
        }
        k1 = 0.1f;
        k2 = 1500.0f;
        bd = PC_CAR_DISTANCE(best);
        for (i = 0; i < 6; i++) {
            if (PC_MODEL_ACTIVE(i) != 0 && PC_CAR_STATE(i) < 2 &&
                (PC_MODEL_DRONE_TYPE(i) == 2 || D_80152030 == 5)) {
                d = bd - PC_CAR_DISTANCE(i);
                if (d > k2) {
                    s = k1 + 1.0f;
                } else {
                    s = d * k1 / k2 + 1;
                }
                if (D_80150F14 == 1) {
                    s = (1 - s) * 0.5f + 1;
                }
                PC_MODEL_SCALE(i) = PC_MODEL_SCALE(i) * 0.97f + 0.03f * s;
            }
        }
    }
}
