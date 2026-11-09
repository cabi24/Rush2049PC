#include "game/game.h"

/* PC: Resource32 is a WAD's texture (TXHD: data = texBase at +24) or palette record (PLHD: bank = palBase at +20),
 * which keep their N64 layout: the pointers are 32-bit RDRAM-window addresses (port/assets/wad.c). */
#define WAD_PTR_BASE u32
#define WAD_PTR(name) name
/* A row's palette or texture list is written through (TextureAnimation): N64PTR0(T, a) is the pointer at N64 address a
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). An item's output stays the 32-bit address the display list takes. */
#define N64PTR0(T, a) ((T)PC_N64P(a))
typedef struct Resource32 { u8 opaque[20]; WAD_PTR_BASE WAD_PTR(bank), WAD_PTR(data); u32 flags; } Resource32;
typedef struct Row20 { char *name; u8 opaque[5]; s8 kind; u8 gap[2]; f32 time; void *data; } Row20;

typedef struct Sequence20 { s16 count, maximum, unused, current; f32 time, default_time; Item12 *items; } Sequence20;

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view
extern Resource32 *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, int);
extern Resource32 *MBOX_FindPalette_WORK(char *, u16 *, s8, s8, int);

void InitTextureAnimation(void) {
    Row20 *row;
    Sequence20 *sequence;
    Resource32 *resource;
    int i;
    u16 id;
    int unused1;
    int unused2;

    if ((gGameState & 8) && !expansion_pak_present) return;
    row = PaletteLists[gTrackNum];
    if (!row) goto second;
    while (row->name) {
        row->time = 0.0333333f;
        if (row->kind >= 9) row->data = N64PTR0(void *, MBOX_FindTexture_WORK(row->name, &id, 0, gNumModels - 1, 1)->data);
        else {
            resource = MBOX_FindPalette_WORK(row->name, &id, 0, gNumModels - 1, 1);
            if (resource) row->data = N64PTR0(void *, resource->bank);
        }
        row++;
    }
second:
    if (gRestartGame) return;
    sequence = TextureSwapLists[gTrackNum];
    if (!sequence) return;
    while (sequence->count) {
        for (i = 0; i < sequence->count; i++) {
            if (expansion_pak_present || gTrackNum >= 6) {
                sequence->items[i].resource = MBOX_FindTexture_WORK(sequence->items[i].name, &id, 0, gNumModels - 1, 1);
            } else if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6 && sequence->items != texture_swap_shared_frames) {
                sequence->items[i].resource = MBOX_FindTexture_WORK(sequence->items[i].name, &id, 0, gNumModels - 1, 1);
            } else continue;
            resource = sequence->items[i].resource;
            if (resource->flags & 0x08000000) sequence->items[i].output = resource->data;
            else sequence->items[i].output = displaylist_get_texture_image(N64PTR0(void *, resource->data));
            sequence->time = sequence->default_time;
            sequence->current = sequence->maximum;
        }
        sequence++;
    }
}
