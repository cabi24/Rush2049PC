/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800EC914: N64 version of arcade munge_gLink_data()
 * (reference/repos/rushtherock/game/mdrive.c:463): from the per-slot link
 * records build model[].slot (the list of active cars), we_control / in_game /
 * drone_type per model, and the humans[] / drones[] / our_drones[] index
 * lists.  N64 differences: only the first D_801543CA slots are examined (the
 * rest get in_game 0), `place` is the slot index, a byte from the link record
 * is copied to game_car[i]+0xEE, the difficulty/marker/track_len block is
 * gone, D_80143FF4 is cleared at the end.
 *
 * Shaping:
 *   - D_801543CA (number of cars), D_80152768 (num_drones) and D_80153FD2
 *     (num_humans) are volatile.  D_80153FD2 is volatile in two other matched
 *     functions (func_80105B74, func_800F8EC8).  The two volatile stores keep
 *     their order in as1 (`.set volatile`), which is the last 3-word residual
 *     of earlier attempts (la t4 split around la t5);
 *   - loops 1 and 2 go through the arcade's `m`/`gc` pointer locals, loop 3
 *     indexes model[] directly (a named pointer there loses `.noalias`);
 *   - the counts in loop 3 and the zeroing share the local `j`
 * (j = n; arr[j] = index; n = j + 1), as found.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

#define MAX_LINKS 6
#define LINK_ACTIVE 0x80
#define DRONE 1
#define HUMAN 2

typedef struct {
    /* 0x000 */ u8 pad0[0x7C6];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ s16 in_game;
    /* 0x7CA */ s16 we_control;
    /* 0x7CC */ s8 drone_type;
    /* 0x7CD */ u8 pad7CD[0x3B];
} MODELDAT; /* 0x808 */

typedef struct {
    /* 0x000 */ u8 pad0[0xEE];
    /* 0x0EE */ u8 unkEE;
    /* 0x0EF */ u8 padEF[0x110 - 0xEF]; struct pc_visual visuals[21]; u8 padEF_308[0x35B - 0x308];
    /* 0x35B */ s8 place;
    /* 0x35C */ u8 pad35C[0x380 - 0x35C]; void *input; u8 pad35C_384[0x3B8 - 0x384];
} CAR_DATA; /* 0x3B8 */

typedef struct {
    /* 0x0 */ u8 pad0[5];
    /* 0x5 */ u8 unk5;
    /* 0x6 */ u8 flags;
    /* 0x7 */ u8 owner;
} Link; /* 0x8 */

extern volatile s16 D_801543CA;        /* number of cars */
extern s8 D_80152744;         /* num_active_cars */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s16 D_8015274C;        /* num_our_drones */
extern volatile s16 D_80152768; /* num_drones */
extern volatile s16 D_80153FD2; /* num_humans */
extern s16 D_80143A40[];      /* humans */
/* On the PC the drone lists are pc_drones and pc_our_drones, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 8), and the slots after the race's are cleared up to the model records' PC_MODEL_SLOTS. The
 * human list keeps its N64 storage: the humans are the players, 4 at most. */
extern s32 D_80143FF4;

void func_800EC914(void) {
    s32 i;
    s32 index;
    s32 j;
    MODELDAT *m;
    CAR_DATA *gc;

    D_80152744 = 0;
    for (i = 0; i < D_801543CA; i++) {
        m = PC_MODEL(i);
        gc = PC_CAR(i);
        m->we_control = (PC_LINK_OWNER(i) == 0);
        m->drone_type = 0;
        m->in_game = (PC_LINK_FLAGS(i) & LINK_ACTIVE) != 0;
        if (m->in_game) {
            PC_MODEL_SLOT(D_80152744++) = i;
            gc->place = i;
            gc->unkEE = PC_LINK_PLACE(i);
            m->drone_type = (PC_LINK_OWNER(i) < MAX_LINKS) ? DRONE : HUMAN;
        }
    }
    for (i = D_801543CA; i < PC_MODEL_SLOTS; i++) {
        m = PC_MODEL(i);
        m->we_control = (PC_LINK_OWNER(i) == 0);
        m->drone_type = 0;
        m->in_game = 0;
    }
    D_8015274C = 0;
    j = D_8015274C;
    D_80152768 = j;
    D_80153FD2 = j;
    for (i = 0; i < D_80152744; i++) {
        index = PC_MODEL_SLOT(i);
        if (PC_MODEL_DRONE_TYPE(index) == HUMAN) {
            j = D_80153FD2;
            D_80143A40[j] = index;
            D_80153FD2 = j + 1;
        } else {
            j = D_80152768;
            PC_DRONE(j) = index;
            D_80152768 = j + 1;
            if (PC_MODEL_WE_CONTROL(index)) {
                PC_OUR_DRONE(D_8015274C++) = index;
            }
        }
    }
    D_80143FF4 = 0;
}
