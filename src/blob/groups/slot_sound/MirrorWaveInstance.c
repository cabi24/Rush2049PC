/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * MirrorWaveInstance(index): after ValidateWaveInstance(index, 1, 1) (slot validation hook), if resource slot
 * gWaveInstance[index] is loaded (s8 +3) and its cached mode byte (+4) differs from (gMirrorWaves != 0), walk the
 * slot's model list gRomObjDefPtrs[index] = { Model *models; s32 count; } (88-byte models, four 16-byte parts at
 * +0x18 with a display list at +8) and run display_list_traverse(dl, 0, 0, 2, flip_triangle_winding) on every
 * non-null part, then store the new mode byte (gMirrorWaves is re-read: the calls kill it). N64-only; no arcade
 * ancestor found.
 *
 * Must be in the unit with ValidateWaveInstance internal (slot_sound group): IPA keeps `index` in a3 across the call.
 * Shaping: the slot is indexed directly (gWaveInstance[index].x) -- a `ResSlot *slot` local adds a named
 * 4-byte slot and moves the spill of the CSE'd address from 68(sp) to 64(sp).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

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
typedef struct { u8 pad0[0x18]; Part part[4]; } Model;
typedef struct { Model *models; s32 count; } ModelList;

extern ResSlot gWaveInstance[];
extern ModelList gRomObjDefPtrs[];
extern s8 gMirrorWaves;

void ValidateWaveInstance(s32 index, s32 a, s32 b);
void display_list_traverse(u32 *dl, u32 match, u32 replace, s32 flags, s32 (*cb)(u32 *));
s32 flip_triangle_winding(u32 *);

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
