#include "game/game.h"
#define ModelList ModelListView   /* this file's own ModelList: include/game's ModelList is another type */

typedef struct ResSlot {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ s8 state;
    /* 0x02 */ u8 active;
    /* 0x03 */ s8 loaded;
    /* 0x04 */ u8 mode;
    /* 0x05 */ u8 pad5[7];
    /* 0x0C */ void *data;
    /* 0x10 */ s32 unk10;
} ResSlot;

/* PC: the OBHD record keeps its N64 layout (port/assets/wad.c); dl is a 32-bit RDRAM-window address. */
#define PART_DL u32
typedef struct { u8 pad0[8]; PART_DL dl; u8 padC[4]; } Part;
typedef struct Model { u8 pad0[0x18]; Part part[4]; } Model;
typedef struct { Model *models; s32 count; } ModelList;

VIEW(ResSlot[], gWaveInstance);
#define gWaveInstance gWaveInstance_view
VIEW(ModelList[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view

void MirrorWaveInstance(s32 index)
{
    Model *m;
    s32 i;
    s32 j;
    s32 count;

    ValidateWaveInstance(index, 1, 1);
    if (gWaveInstance[index].loaded) {
        if (gWaveInstance[index].mode != (gMirrorWaves != 0)) {
            m = gRomObjDefPtrs[index].models;
            count = gRomObjDefPtrs[index].count;
            for (i = 0; i < count; i++) {
                for (j = 0; j < 4; j++) {
                    if (m->part[j].dl) {
                        display_list_traverse(m->part[j].dl, 0, 0, 2, flip_triangle_winding);
                    }
                }
                m++;
            }
            gWaveInstance[index].mode = (gMirrorWaves != 0);
        }
    }
}
