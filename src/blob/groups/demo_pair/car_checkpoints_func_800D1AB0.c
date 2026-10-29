/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef float f32;
typedef short s16;
typedef int s32;
typedef signed char s8;

typedef struct {
    char pad0[0x7C6];
    s16 slot;
    char pad7C8[0x20];
    s8 unk7E8;
    char pad7E9[0x1F];
} Model; /* 0x808 */

typedef struct {
    char pad0[0xEE];
    s8 place;
    s8 unkEF;
    char padF0[0x10];
    f32 distance;
#ifdef TARGET_PC
    char pad104[0x110 - 0x104]; struct pc_visual visuals[21]; char pad104_308[0x359 - 0x308];
#else
    char pad104[0x255];
#endif
    s8 unk359;
#ifdef TARGET_PC
    char pad35A[0x380 - 0x35A]; void *input; char pad35A_384[0x3B8 - 0x384];
#else
    char pad35A[0x5E];
#endif
} GameCar; /* 0x3B8 */

extern s8 D_80152744;
extern s16 D_80152734;
#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Model D_8014A250[];
#define PC_MODEL_LAP(i) D_8014A250[i].unk7E8
#define PC_MODEL_SLOT(i) D_8014A250[i].slot
#endif
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern GameCar D_80152818[];
#define PC_CAR_DISTANCE(i) D_80152818[i].distance
#define PC_CAR_FINISHED(i) D_80152818[i].unkEF
#define PC_CAR_PLACE(i) D_80152818[i].place
#define PC_CAR_STATE(i) D_80152818[i].unk359
#endif

void func_800D1AB0(void) {
    s16 i;
    s16 j;
    s16 num_left;
    s16 num_done;
    s32 high_index;
    s16 place[6];
    s32 dist_tab[6];
    s16 index;

    num_done = 0;
    num_left = 0;
    for (i = 0; i < D_80152744; i++) {
        if (PC_CAR_STATE(PC_MODEL_SLOT(i)) > 0 && PC_MODEL_LAP(i) < D_80152734) {
            num_done++;
        } else if (PC_CAR_FINISHED(PC_MODEL_SLOT(i)) != 1) {
            place[num_left++] = PC_MODEL_SLOT(i);
        }
    }
    if (num_left != 0) {
        for (i = 0; i < num_left; i++) {
            dist_tab[place[i]] = PC_CAR_DISTANCE(place[i]);
        }
        for (i = 0; i < num_left; i++) {
            high_index = i;
            for (j = i + 1; j < num_left; j++) {
                index = place[j];
                if (dist_tab[index] > dist_tab[place[high_index]]) {
                    high_index = j;
                }
            }
            PC_CAR_PLACE(place[high_index]) = i + D_80152744 - num_left - num_done;
            place[high_index] ^= place[i];
            place[i] ^= place[high_index];
            place[high_index] ^= place[i];
        }
    }
}

