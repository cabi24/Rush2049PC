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

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct BNode { struct BNode *next; s16 active; u8 pad[6]; u32 owner; f32 timer; void *stat; } BNode;
/* The target info records (targetInfo) are 0x40 bytes, their visual function a pointer (docs/port/POINTERS.md 10). */
#define TI_VISUAL(t) M2C_FIELD(t, void **, TINFO_OFS(0xC))
#define N64P(x) ((void *)PC_N64P(x))
#define N64W(x) PC_N64W(x)
VIEW(BNode *, gEnvList);
#define gEnvList gEnvList_view

/* callees outside the group */
BNode *GrabEnvEntry();

void StartCactus(void *arg0) {
    BNode *temp_v0;
    void *sp30;
    GameCar *temp_v0_2;
    BNode **head;
    f32 vector[3];

    sp30 = (M2C_FIELD(arg0, s16 *, 0x10) * TINFO_OFS(0x30)) + (u8 *) &targetInfo;
    M2C_FIELD(arg0, s16 *, 0x5A) = 7;
    temp_v0 = GrabEnvEntry();
    if (temp_v0 != NULL) {
        temp_v0->active = 0;
        temp_v0->stat = TI_VISUAL(sp30);
        temp_v0->owner = N64W(arg0);
        temp_v0->timer = (f32) cactus_frame_time;
        temp_v0_2 = PC_CAR(M2C_FIELD(arg0, s8 *, 0x5C));
        vector[0] = M2C_FIELD(temp_v0_2, f32 *, 0x14) * 0.125f;
        vector[1] = M2C_FIELD(temp_v0_2, f32 *, 0x18) * 0.125f;
        vector[2] = M2C_FIELD(temp_v0_2, f32 *, 0x1C) * 0.125f;
        LookInDir(&vector[0], (u8 *) arg0 + 0x14);
        M2C_FIELD(arg0, u8 *, 4) = (u8) (M2C_FIELD(arg0, u8 *, 4) & 0xFFF9);
        head = &gEnvList;
        temp_v0->next = *head;
        *head = temp_v0;
        /* the target type's start sound, targetInfo[type].startSnd (N64 +0x1C) */
        PlayTargetSound(*(s32*)((u8 *) &targetInfo + (M2C_FIELD(arg0, s16 *, 0x10) * TINFO_OFS(0x30)) + TINFO_OFS(0x1C)), M2C_FIELD(arg0, s8 *, 0x5C), (u8 *) arg0 + 0x38, 2);
    }
}

