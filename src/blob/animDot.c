/* animDot: a car's dot on the track map, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

/* A model record (MAT3 tModelData) as this reads it. */
typedef struct Model {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[920];
    streetyp *lasttp[4];
    u8 pad3B0[92];
    const s16 *torquecurve;
    u8 pad410[560];
    s8 crash;
    u8 pad641[0x648 - 0x641];
    surftyp *movingSurface;
    u8 pad64C[0x6C4 - 0x64C];
    s16 hit_target;
    u8 pad6C6[1996 - 1734];
    s8 mode;
    u8 pad7CD[2015 - 1997];
    s8 hide;
    u8 pad7E0[2027 - 2016];
    s8 collidable;
    u8 tail[2056 - 2028];
} Model;

/* The dot of the car at place animID & 0xF of the map's drawing order (dot_order_list, hud_updateFrame) on the track
 * map (hud_mapPos, kMapSize square, the track's path bounds fitted in it; mirrored with gMirrorWaves): a human's own
 * colour frame (its slot), a drone's frame 7; it flashes (alpha 64 every 8 retraces) while the car is dead,
 * crashing, not collidable, hidden or hit. Hidden with 5 windows or more, without a car there, or when the HUD hides
 * the map. Returns 1 (or Hidden's answer). */
s32 animDot(tBlit *blt)
{
    Model *m;
    tCarData *car;
    s16 b, flash, slot;
    s16 size;
    u32 id;
    s32 world_width, world_height, map_width, map_height, xo, yo;

    id = blt->animID;
    slot = dot_order_list[id & 0xf];
    if (Hidden(blt, gNumWindows >= 5 || slot == -1 || !gShowTrackMap))
        return 1;
    m = PC_MODEL(slot);
    car = PC_CAR(slot);

    world_width = gPathMax[0] - gPathMin[0];
    world_height = gPathMax[2] - gPathMin[2];
    size = blt->height;
    if (world_height < world_width) {
        map_width = kMapSize - 8;
        map_height = (s32)(world_height * (u32)(kMapSize - 8)) / world_width;
    } else {
        map_height = kMapSize - 8;
        map_width = (s32)(world_width * (u32)(kMapSize - 8)) / world_height;
    }
    xo = (kMapSize - map_width - 8) / 2 + 4;
    yo = (kMapSize - map_height - 8) / 2 + 4;
    blt->x = hud_mapPos[gNumWindows - 1].x + xo - (size + kMapSize) / 2;
    blt->y = hud_mapPos[gNumWindows - 1].y + yo - (size + kMapSize) / 2;
    if (gMirrorWaves) {
        blt->x += (car->dr_pos[0] - gPathMin[0]) * map_width / world_width;
    } else {
        blt->x += ((map_width - 1) - (car->dr_pos[0] - gPathMin[0]) * map_width / world_width);
    }
    blt->y += (car->dr_pos[2] - gPathMin[2]) * map_height / world_height;
    gBlitInstanceList[blt->blidx].flags |= BLITINST_ALPHA_COMPARE;
    if (size) { }
    flash = car->crash_state || m->crash || !m->collidable || m->hide || m->hit_target != -1;
    if (car->dead > 0)
        flash = 0;
    if (slot < gNumCars) {
        blt->alpha = ((gScheduler.retraceCount & 8) && flash) ? 64 : 255;
        if (m->mode == 2)
            SelectBlit(blt, slot, size, size);
        else if (m->mode == 1)
            SelectBlit(blt, 7, size, size);
    } else {
        return Hidden(blt, 1);
    }
    UpdateBlit(blt);
    return 1;
}
