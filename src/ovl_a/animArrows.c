/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Image A [0x803AE940,0x803AECC4), 900 bytes: front-end list-control
 * BLIT callback. Native-authoritative C reconstruction; no exact arcade
 * donor or original variable-name claim. Packed descriptor nibbles are
 * snapshotted before callbacks, while geometry and globals are read after
 * side-effecting helpers. Geometry is a four-point row, texture names have
 * two states per entry, and texture records have a 32-byte stride.
 * ResourceTables uses the accessed prefix independently established by
 * src/blob/groups/slot_sound/AdjustTextWavePointers.c. BLIT uses its accessed prefix.
 * Table bounds and actual resource contents are not inferred from strides.
 * Float-to-u32 behavior is C-defined only for representable finite values;
 * byte assignment then applies unsigned narrowing. Signed-halfword stores
 * follow the target implementation when the mathematical result is wider.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State;
    s16 Width, Height; u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID;
    uintptr_t reserved30; u16 Texture;
} Blit;
typedef struct Point { s16 x, y; } Point;
typedef struct ResourceIndices { u16 unused, first; } ResourceIndices;
typedef struct ResourceTables {
    u16 count0; u32 *ptrs0; u16 count1, count2;
    ResourceIndices *indices; char **strings;
} ResourceTables;
/* PC: the sprite's host layout (particle_knot's Sprite), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Texture { void *pal, *tex; u16 model; s16 x, y; u16 mode; s16 w, h; u8 color; u8 flags; s8 hidden; u8 pad17; s16 t0, s0, t1, s1; } Texture;
extern s32 trackscr_buttonsInited;
extern s16 trackscr_gFirstOption, trackscr_gNumOptions, trackscr_gActiveOption, trackscr_gOpts[];
extern s8 trackscr_gActiveOptionEnabled;
extern char *trackscr_arrowNames[][2];
extern Point trackscr_arrowPos[][4];
extern ResourceTables gText;
extern Texture gBlitInstanceList[];
extern s8 Hidden(Blit *, s32);
extern void RenameBlit(Blit *, char *, s32);
extern float TB_GetFlashTextCycle(void);
extern void HAL_SetFont(s32);
extern s32 HAL_GetTextWidth(char *, s32);
extern void UpdateBlit(Blit *);
s32 animArrows(Blit *blt)
{
    s16 pulse = blt->AnimID & 15;
    s16 item = (blt->AnimID >> 4) & 15;
    s16 group = (blt->AnimID >> 8) & 15;
    s16 texture = (blt->AnimID >> 12) & 15;
    s16 active;
    if (Hidden(blt, trackscr_buttonsInited == 1)) return 1;
    if (group == 1) {
        if (Hidden(blt, item == 2)) return 1;
    }
    if (group == 0) {
        if (item == 2) {
            if (Hidden(blt, trackscr_gFirstOption == 0)) return 1;
        } else if (item == 3) {
            if (Hidden(blt, trackscr_gNumOptions - trackscr_gFirstOption < 5)) return 1;
        }
    }
    active = trackscr_gActiveOptionEnabled || item >= 2;
    RenameBlit(blt, trackscr_arrowNames[texture][active], 1);
    if (pulse) blt->Alpha = (u32)(TB_GetFlashTextCycle() * 255.0f);
    blt->X = trackscr_arrowPos[group][item].x - blt->Width / 2;
    blt->Y = trackscr_arrowPos[group][item].y - blt->Height / 2;
    if (group == 0) {
        HAL_SetFont(11);
        if (item == 0) {
            blt->X -= HAL_GetTextWidth(gText.strings[trackscr_gOpts[trackscr_gActiveOption] + gText.indices->first], -1);
        }
        if (item == 0 || item == 1) blt->Y += (trackscr_gActiveOption - trackscr_gFirstOption) * 13;
    }
    if (item == 1) blt->Flip = 1;
    else if (item == 3) gBlitInstanceList[blt->Texture].flags |= 8;
    UpdateBlit(blt);
    return 1;
}
