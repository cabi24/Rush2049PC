/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct { u8 p0[148]; u8 idx; u8 p1[3]; } A152;
#ifdef TARGET_PC
typedef struct { u8 p[0x110 - 0x0]; struct pc_visual visuals[21]; u8 p_308[0x35C - 0x308]; u8 f860; u8 f861; u8 q[0x380 - 0x35E]; void *input; u8 q_384[0x3B8 - 0x384]; } B952;
#else
typedef struct { u8 p[860]; u8 f860; u8 f861; u8 q[90]; } B952;
#endif
extern s16 gNumWindows;
extern A152 gCamera[];
extern float D_801106B4[];
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern B952 D_80152818[];
#define PC_CAR(i) &D_80152818[i]
#endif
#ifdef TARGET_PC
/* This file defines D_80110680, the finish cameras' state, on the N64; on the PC it is pc_finish_cam, with no N64
 * address (pc_game_types.h, docs/port/DECOUPLE.md 6), and the clear resets every slot it has. */
#define FINISH_CAM_SLOTS PC_MAX_LINKS
#else
u8 D_80110680[6];
#define PC_FINISH_CAM(i) D_80110680[i]
#define FINISH_CAM_SLOTS 6
#endif
void func_800E9234(B952 *b);
void func_800FAD50(B952 *b);
void render_post_process(void) {
    s32 i;
    B952 *b;
    for (i = 0; i < gNumWindows; i++) {
        b = PC_CAR(gCamera[i].idx);
        b->f860 = i;
        func_800E9234(b);
        b->f861 = 7;
        func_800FAD50(b);
        D_801106B4[i] = 0.0f;
    }
    for (i = 0; i < FINISH_CAM_SLOTS; i++) {
        PC_FINISH_CAM(i) = 0;
    }
}
