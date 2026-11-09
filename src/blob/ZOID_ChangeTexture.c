#include "game/game.h"
#define ModelList ModelListView   /* this file's own ModelList: include/game's ModelList is another type */

/* PC: Model is a WAD's OBHD record, which keeps its N64 layout (port/assets/wad.c): dl is a 32-bit window address. */
#define PART_DL u32
typedef struct { u16 value; u16 flags; u8 pad4[4]; PART_DL dl; u8 padC[4]; } Part;
typedef struct Model { u8 pad0[0x16]; s16 count; Part part[4]; } Model;
typedef struct { Model *models; s32 count; } ModelList;

VIEW(ModelList[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view

s32 ZOID_ChangeTexture(u16 id, s16 part, u16 value)
{
    Model *m;
    s32 i;

    m = &gRomObjDefPtrs[id >> 10].models[id & 0x3FF];
    if (part >= 0) {
        if (part >= m->count) {
            return 0;
        }
        m->part[part].value = value;
        m->part[part].flags |= 0x8000;
    } else {
        for (i = 0; i < m->count; i++) {
            m->part[i].value = value;
            /* Original game: part, not i: with part < 0 this marks m->part[-1] (the bytes before the part table,
             * the model's header) on every pass and leaves the parts unmarked. MAT3 zoid has the same slip; the
             * callers seen pass part 0, so it may never run. Kept as retail. */
            m->part[part].flags |= 0x8000;
        }
    }
    return 1;
}
