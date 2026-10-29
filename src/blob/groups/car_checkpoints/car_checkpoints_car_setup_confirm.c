/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * NOT STRICT: 2 words reference `D_8014A250_alias` (unresolved), .
 * Arcade CarReportsGameOver (game/checkpoint.c), N64 form: car_setup_confirm(slot, score).
 * Locks the car's place, re-sorts the locked cars by score (bubble sort over parallel place[] /
 * scores[] arrays), gives finished-but-lapped cars (unk359 > 0 and model.unk7E8 < D_80152734) a
 * score of 5999.999f + (s32)(9.0f * D_801543AC) - distance and a place counted from the back, ends
 * the race clock when all humans are locked, then calls func_800D1AB0 (places of the unlocked cars).
 * The human count test compares &model[index] with the address 0x8014A250 through a web that is NOT
 * the model array base used for indexing (retail materialises lui/addiu in the loop instead of using
 * s0): every spelling on `D_8014A250` itself is commoned with the base. A second symbol for the same
 * address reproduces all 221 words.
 * Quirks: place[7] (frame), scores before ftemp; the final loop indexes place[i] directly.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    /* 0x000 */ char pad0[0x7C6];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ char pad7C8[4];
    /* 0x7CC */ s8 drone_type;
    /* 0x7CD */ char pad7CD[0x1B];
    /* 0x7E8 */ s8 unk7E8;
    /* 0x7E9 */ char pad7E9[0x1F];
} Model; /* 0x808 */

typedef struct {
    /* 0x000 */ char pad0[0xEE];
    /* 0x0EE */ s8 place;
    /* 0x0EF */ s8 place_locked;
    /* 0x0F0 */ f32 score;
    /* 0x0F4 */ char padF4[0xC];
    /* 0x100 */ f32 distance;
#ifdef TARGET_PC
    /* 0x104 */ char pad104[0x110 - 0x104]; struct pc_visual visuals[21]; char pad104_308[0x359 - 0x308];
#else
    /* 0x104 */ char pad104[0x255];
#endif
    /* 0x359 */ s8 unk359;
#ifdef TARGET_PC
    /* 0x35A */ char pad35A[0x380 - 0x35A]; void *input; char pad35A_384[0x3B8 - 0x384];
#else
    /* 0x35A */ char pad35A[0x5E];
#endif
} GameCar; /* 0x3B8 */

extern s8 D_80152744;
extern s16 D_80152734;
#ifdef TARGET_PC
#else
extern Model D_8014A250[];
#endif
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern GameCar D_80152818[];
#define PC_CAR_DISTANCE(i) D_80152818[i].distance
#define PC_CAR_FINISHED(i) D_80152818[i].place_locked
#define PC_CAR_PLACE(i) D_80152818[i].place
#define PC_CAR_STATE(i) D_80152818[i].unk359
#define PC_CAR_TIME(i) D_80152818[i].score
#endif
#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Model D_8014A250_alias[]; /* second symbol for 0x8014A250, see header */
#define PC_MODEL(i) &D_8014A250[i]
#define PC_MODELS D_8014A250_alias
#define PC_MODEL_DRONE_TYPE(i) D_8014A250[i].drone_type
#define PC_MODEL_LAP(i) D_8014A250[i].unk7E8
#define PC_MODEL_SLOT(i) D_8014A250[i].slot
#endif
extern s32 D_8014A110;
extern s16 D_8014A108;
extern s8 D_80152718;
extern f32 D_801543AC;
extern s16 D_801543CA;

f32 viGetTimeToDeadline(void);
void viAddTicks(f32);
void func_800D1AB0(void);

#ifdef TARGET_PC
/* PC: the locked cars in score order, by slot, and their scores, as many as the race's cars (up to PC_MAX_LINKS,
 * pc_game_types.h, docs/port/MORECARS.md 2.5); the N64's are 7 and 6, the 7 for its frame. */
#define PLACE_N (PC_MAX_LINKS + 1)
#define SCORES_N PC_MAX_LINKS
#else
#define PLACE_N 7
#define SCORES_N 6
#endif
void car_setup_confirm(s32 slot, f32 score) {
    s16 i, j, index, temp, num_locked, place[PLACE_N], num_humans_locked;
    f32 scores[SCORES_N];
    f32 ftemp;

    PC_CAR_FINISHED(slot) = 1;
    PC_CAR_TIME(slot) = score;
    num_humans_locked = 0;
    for (i = 0, num_locked = -1; i < D_80152744; i++) {
        index = PC_MODEL_SLOT(i);
        if (PC_CAR_FINISHED(index) == 1) {
            place[++num_locked] = index;
            scores[num_locked] = PC_CAR_TIME(index);
            if (PC_MODEL_DRONE_TYPE(index) == 2) {
                if (D_8014A110 != 2 || PC_MODELS == PC_MODEL(index)) {
                    num_humans_locked++;
                }
            }
            if (PC_CAR_STATE(index) > 0 && PC_MODEL_LAP(index) < D_80152734) {
                scores[num_locked] = 5999.999f + (f32) (s32) (9.0f * D_801543AC) - PC_CAR_DISTANCE(index);
            }
        }
    }
    if (PC_MODEL_DRONE_TYPE(slot) == 2) {
        if ((num_humans_locked == D_8014A108 || (D_8014A110 == 2 && num_humans_locked == 1)) && D_80152718 == 0) {
            D_80152718 = 1;
            viAddTicks(1.0f - viGetTimeToDeadline());
        }
    }
    for (i = 0; i < num_locked; i++) {
        for (j = 0; j < num_locked - i; j++) {
            if (scores[j] > scores[j + 1]) {
                ftemp = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = ftemp;
                temp = place[j];
                place[j] = place[j + 1];
                place[j + 1] = temp;
            }
        }
    }
    for (i = 0; i < num_locked + 1; i++) {
        if (PC_CAR_STATE(place[i]) > 0 && PC_MODEL_LAP(place[i]) < D_80152734) {
            PC_CAR_PLACE(place[i]) = D_801543CA - num_locked + i - 1;
        } else {
            PC_CAR_PLACE(place[i]) = i;
        }
    }
    func_800D1AB0();
}
