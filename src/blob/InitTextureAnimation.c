/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitTextureAnimation @ 0x800BDAA8, 744 bytes (historical label; real semantics: per-track
 * animation resource binding). It is the set-up half of TextureAnimation (the per-frame updater over
 * the same two tables). No arcade ancestor found.
 *   - Skipped when bit 3 of gGameState is set and expansion_pak_present is clear.
 *   - Palette-animation rows of the current track (PaletteLists[gTrackNum], 20-byte records ending
 *     with a NULL name): reset the delay to 0.0333333f (one 30 Hz tick) and bind the data pointer,
 *     through MBOX_FindTexture_WORK (kind >= 9, takes its field +24) or sound_bank_load (field +20, when found).
 *   - Unless gRestartGame is set, the object-animation sequences (TextureSwapLists[track], 20-byte records
 *     ending with count == 0): for every frame, look its name up with MBOX_FindTexture_WORK (only when
 *     expansion_pak_present is set, the track is >= 6, or the frames are not the shared texture_swap_shared_frames table), bind the
 *     frame's display list (the object's own +24 list when flag 0x08000000 is set, else a copy made by
 *     displaylist_get_texture_image), then reset the sequence delay/index to their initial values.
 *   Both lookups get the name, &id, 0, gNumModels - 1 and 1 (five arguments; the locked callee
 *   definitions declare four).
 *
 * Strict MATCH at -O3 (also at -O2); own .rodata (0.0333333f = 0x3D088880 at 0x80123E58) verified by the
 * scorer; EQUAL in the whole-program unit.
 *
 * Shaping facts (each needed):
 * - `if (!row) goto second;` before the row loop: with `if (row) { while ... }` uopt places the
 *   re-materialised &gNumModels / &id in the shared join block (13 words off); the goto gives retail's
 *   separate null-path and loop-exit copies;
 * - two unused int locals declared after `id`: retail's frame is 112 bytes with id at sp+94
 *   (quirk: they only reserve stack slots);
 * - the kind byte at +9 is read signed (`lb`, `kind >= 9`); TextureAnimation reads the same byte as an
 *   unsigned switch operand;
 * - the second-phase condition keeps the redundant `!expansion_pak_present` test (retail tests it again).
 */
typedef unsigned char u8; typedef signed char s8; typedef unsigned short u16; typedef signed short s16;
typedef unsigned int u32; typedef float f32;

/* PC: Resource32 is a WAD's texture (TXHD: data = texBase at +24) or palette record (PLHD: bank = palBase at +20),
 * which keep their N64 layout: the pointers are 32-bit RDRAM-window addresses (port/assets/wad.c). */
#define WAD_PTR_BASE u32
#define WAD_PTR(name) name
/* A row's palette or texture list is written through (TextureAnimation): N64PTR0(T, a) is the pointer at N64 address a
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). An item's output stays the 32-bit address the display list takes. */
#define N64PTR0(T, a) ((T)PC_N64P(a))
typedef struct Resource32 { u8 opaque[20]; WAD_PTR_BASE WAD_PTR(bank), WAD_PTR(data); u32 flags; } Resource32;
typedef struct Row20 { char *name; u8 opaque[5]; s8 kind; u8 gap[2]; f32 time; void *data; } Row20;
typedef struct Item12 { char *name; Resource32 *resource; void *output; } Item12;
typedef struct Sequence20 { s16 count, maximum, unused, current; f32 time, default_time; Item12 *items; } Sequence20;

extern Row20 *PaletteLists[];
extern Sequence20 *TextureSwapLists[];
extern Item12 texture_swap_shared_frames[];
extern u32 gGameState;
extern s8 expansion_pak_present, gRestartGame, gTrackNum;
extern u8 gNumModels;
extern Resource32 *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, int);
extern Resource32 *MBOX_FindPalette_WORK(char *, u16 *, s8, s8, int);
extern u32 displaylist_get_texture_image(void *);   /* the SETTIMG address of a TXLD list (a u32 in its definition) */

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
