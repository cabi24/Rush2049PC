#include "game/game.h"

typedef struct Blit {
    const char *Name;
    void *image;
    void *Info;
    s16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right;
    s16 color;
    u16 unknown26;
    s32 (*AnimFunc)(struct Blit *);
    u32 AnimID;
    uintptr_t unk30;
    u16 render_slot;
} Blit;

typedef struct Car {
    u8 pad0[8];
    f32 position[3];
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x358 - 0x308];
    s8 dead;
    s8 active;
    u8 tail[0x380 - 0x35A]; void *input; u8 tail_384[0x3B8 - 0x384];
} Car;

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

typedef struct MapOrigin { s32 x, y; } MapOrigin;
/* PC: the sprite's host layout (particle_knot's Sprite), as every view has it (docs/port/POINTERS.md 9). */
typedef struct RenderRec { void *pal, *tex; u16 model; s16 x, y; u16 mode; s16 w, h; u8 color; u8 flags; s8 hidden; u8 pad17; s16 t0, s0, t1, s1; } RenderRec;
typedef struct Clock { u8 pad0[636]; u32 tick; } Clock;

VIEW(MapOrigin[], hud_mapPos);
#define hud_mapPos hud_mapPos_view

VIEW(RenderRec[], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

VIEW(volatile Clock, gScheduler);
#define gScheduler gScheduler_view

s32 animDot(Blit *blt)
{
    Model *m;
    Car *car;
    s16 b, flash, slot;
    s16 size;
    u32 id;
    s32 world_width, world_height, map_width, map_height, xo, yo;

    id = blt->AnimID;
    slot = dot_order_list[id & 0xf];
    if (Hidden(blt, gNumWindows >= 5 || slot == -1 || !gShowTrackMap))
        return 1;
    m = PC_MODEL(slot);
    car = PC_CAR(slot);

    world_width = gPathMax.x - gPathMin.x;
    world_height = gPathMax.z - gPathMin.z;
    size = blt->Height;
    if (world_height < world_width) {
        map_width = kMapSize - 8;
        map_height = (s32)(world_height * (u32)(kMapSize - 8)) / world_width;
    } else {
        map_height = kMapSize - 8;
        map_width = (s32)(world_width * (u32)(kMapSize - 8)) / world_height;
    }
    xo = (kMapSize - map_width - 8) / 2 + 4;
    yo = (kMapSize - map_height - 8) / 2 + 4;
    blt->X = hud_mapPos[gNumWindows - 1].x + xo - (size + kMapSize) / 2;
    blt->Y = hud_mapPos[gNumWindows - 1].y + yo - (size + kMapSize) / 2;
    if (gMirrorWaves) {
        blt->X += (car->position[0] - gPathMin.x) * map_width / world_width;
    } else {
        blt->X += ((map_width - 1) - (car->position[0] - gPathMin.x) * map_width / world_width);
    }
    blt->Y += (car->position[2] - gPathMin.z) * map_height / world_height;
    gBlitInstanceList[blt->render_slot].flags |= 1;
    if (size) { }
    flash = car->dead || m->crash || !m->collidable || m->hide || m->hit_target != -1;
    if (car->active > 0)
        flash = 0;
    if (slot < gNumCars) {
        blt->Alpha = ((gScheduler.tick & 8) && flash) ? 64 : 255;
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
