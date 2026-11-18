/* animDot: a car's dot on the track map, a blit animation function (MAT3 hud.c). */
#include "game/game.h"


/* The dot of the car at place animID & 0xF of the map's drawing order (dot_order_list, hud_updateFrame) on the track
 * map (hud_mapPos, kMapSize square, the track's path bounds fitted in it; mirrored with gMirrorWaves): a human's own
 * colour frame (its slot), a drone's frame 7; it flashes (alpha 64 every 8 retraces) while the car is dead,
 * crashing, not collidable, hidden or hit. Hidden with 5 windows or more, without a car there, or when the HUD hides
 * the map. Returns 1 (or Hidden's answer). */
s32 animDot(tBlit *blt)
{
    tModelData *m;
    tCarData *car;
    s16 b, flash, slot;
    s16 size;
    u32 id;
    s32 world_width, world_height, map_width, map_height, xo, yo;

    id = blt->animID;
    slot = dot_order_list[id & 0xf];
    if (Hidden(blt, gNumWindows >= 5 || slot == -1 || !gShowTrackMap))
        return 1;
    m = &pc_models[slot];
    car = &pc_cars[slot];

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
    flash = car->crash_state || m->crashflag || !m->solid || m->hide_car || m->resurrect.moving_state != -1;
    if (car->dead > 0)
        flash = 0;
    if (slot < gNumCars) {
        blt->alpha = ((gScheduler.retraceCount & 8) && flash) ? 64 : 255;
        if (m->drone_type == 2)
            SelectBlit(blt, slot, size, size);
        else if (m->drone_type == 1)
            SelectBlit(blt, 7, size, size);
    } else {
        return Hidden(blt, 1);
    }
    UpdateBlit(blt);
    return 1;
}
