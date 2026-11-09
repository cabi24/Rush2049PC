/* animDot: a car's dot on the track map, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

/* A car record as this reads it: its position, whether it is dead or resurrecting. */
typedef struct Car {
    u8 pad0[8];
    f32 position[3];
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x358 - 0x308];
    s8 dead;
    s8 active;
    u8 tail[0x380 - 0x35A]; void *input; u8 tail_384[0x3B8 - 0x384];
} Car;

/* A model record (MAT3 tModelData) as this reads it. */
typedef struct Model {
    u8 pad0[1600];
    s8 crash;
    u8 pad641[1732 - 1601];
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
    Car *car;
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
        blt->x += (car->position[0] - gPathMin[0]) * map_width / world_width;
    } else {
        blt->x += ((map_width - 1) - (car->position[0] - gPathMin[0]) * map_width / world_width);
    }
    blt->y += (car->position[2] - gPathMin[2]) * map_height / world_height;
    gBlitInstanceList[blt->blidx].flags |= BLITINST_ALPHA_COMPARE;
    if (size) { }
    flash = car->dead || m->crash || !m->collidable || m->hide || m->hit_target != -1;
    if (car->active > 0)
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
