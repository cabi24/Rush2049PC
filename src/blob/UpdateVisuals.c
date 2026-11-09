/* UpdateVisuals.c: animate a car's visuals for a frame (MAT3 visuals.c, UpdateVisuals()). */
#include "game/game.h"

/* A car visual (tVisual) on the PC: 0x28 bytes, next, data and func pointer-sized (struct pc_visual, docs/port/CARS.md
 * 3). */
typedef struct tVisual { struct tVisual *next; s16 index, objnum, slot; uintptr_t data; f32 timeStamp; void (*func)(struct tVisual *v, s16 op); } tVisual;
/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields UpdateVisuals reads. */
typedef struct tCarData { u8 pad0[0xE8]; u32 appearance; u8 pad0EC[0x110 - 0xEC]; tVisual visuals[21]; u8 pad308[0x380 - 0x308]; void *player; u8 pad384[0x3B8 - 0x384]; } tCarData;

/* Once a frame for car `slot`: judge whether the frame rate is good enough for the costly effects (gGoodFrameRate:
 * the last frame took under 36 ms at 50 Hz, 45 ms in split screen, else 30 ms; frame_cycle_count is in CPU counts,
 * 46.875 a microsecond), start smoke (the wreck's, APPEAR_WRECK_SMOKE) and each tire's spray on water
 * (SURF_VIS_WATER, StartSmoke `fast` 1) or smoke (APPEAR_SMOKE_T0..T3), run every visual's animator with op 1
 * (update), then the skid marks. */
void UpdateVisuals(s16 slot)
{
    tCarData *car = PC_CAR(slot);
    tVisual *v = &car->visuals[0];
    s16 i;
    f32 usec;

    usec = (u64)frame_cycle_count * 64 / 3000;
    if (gRetraceRate == 50.0f) {
        gGoodFrameRate = usec < 36000.0f;
    } else if (gNumPlayers >= 2) {
        gGoodFrameRate = usec < 45000.0f;
    } else {
        gGoodFrameRate = usec < 30000.0f;
    }

    if (car->appearance & APPEAR_WRECK_SMOKE)
        StartSmoke(slot, 4, 0, 0);

    if (PC_MODEL_VISCODE(slot)[0] == SURF_VIS_WATER)
        StartSmoke(slot, 0, 1, 0);
    else if (car->appearance & APPEAR_SMOKE_T0)
        StartSmoke(slot, 0, 0, 0);
    if (PC_MODEL_VISCODE(slot)[1] == SURF_VIS_WATER)
        StartSmoke(slot, 1, 1, 0);
    else if (car->appearance & APPEAR_SMOKE_T1)
        StartSmoke(slot, 1, 0, 0);
    if (PC_MODEL_VISCODE(slot)[2] == SURF_VIS_WATER)
        StartSmoke(slot, 2, 1, 0);
    else if (car->appearance & APPEAR_SMOKE_T2)
        StartSmoke(slot, 2, 0, 0);
    if (PC_MODEL_VISCODE(slot)[3] == SURF_VIS_WATER)
        StartSmoke(slot, 3, 1, 0);
    else if (car->appearance & APPEAR_SMOKE_T3)
        StartSmoke(slot, 3, 0, 0);

    for (i = 0; i < CAR_VISUALS; ++i, ++v)
        if (v->func)
            (*v->func)(v, 1);

    UpdateSkids(slot);
}
