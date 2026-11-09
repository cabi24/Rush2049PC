#include "game/game.h"

typedef struct {
    u8 pad000[0xE8];
    s32 unkE8;
    u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad0EC_308[0x35B - 0x308];
    u8 unk35B;
    u8 pad35C[0x380 - 0x35C];
    void *unk380;
    u8 pad384[0x3A3 - 0x384];
    u8 unk3A3;
    u8 pad3A4[0x3B8 - 0x3A4];
} GameCar;

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

VIEW(M2C_UNK, targetInfo);
#define targetInfo targetInfo_view
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

void StartMine(void *arg0) {
    s32 pad;
    GameCar *car;
    f32 delta[3];
    f32 local[3];
    if (PC_MODEL_CRASHFLAG(M2C_FIELD(arg0, s8 *, 0x5C)) == 0) {
        car = PC_CAR(M2C_FIELD(arg0, s8 *, 0x5C));
        if (M2C_FIELD(car, s8 *, CAR_OFS(0x384)) == 5) {
            delta[0] = M2C_FIELD(arg0, f32 *, 0x44) - M2C_FIELD(car, f32 *, 8);
            delta[1] = M2C_FIELD(arg0, f32 *, 0x48) - M2C_FIELD(car, f32 *, 12);
            delta[2] = M2C_FIELD(arg0, f32 *, 0x4C) - M2C_FIELD(car, f32 *, 16);
            BodyVector(delta, local, (u8 *)car + 0x2C);
            if (local[2] >= 0.0f) {
                BattleDamage(PC_CAR(M2C_FIELD(arg0, s8 *, 0x65)), car, 0x118);
            } else {
                BattleDamage(PC_CAR(M2C_FIELD(arg0, s8 *, 0x65)), car, 0x320);
            }
        } else {
            BattleDamage(PC_CAR(M2C_FIELD(arg0, s8 *, 0x65)), car, 0x320);
        }
        M2C_FIELD(arg0, u8 *, 4) &= 0xFFFD;
        /* the target type's start sound, targetInfo[type].startSnd (N64 +0x1C) */
        PlayTargetSound(*(s32 *)((u8 *)&targetInfo + M2C_FIELD(arg0, s16 *, 0x10) * TINFO_OFS(0x30) + TINFO_OFS(0x1C)), M2C_FIELD(arg0, s8 *, 0x5C), (u8 *)arg0 + 0x38, 2);
    }
}
