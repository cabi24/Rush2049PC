/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Model-table loader (N64-only): takes the data block of resource slot gTextWaveInst and fills the
 * table header gText from its six packed sections (u32 count followed by the items; u16 sections
 * are padded to an even count), then relocates the three pointer sections by the block address.
 *
 * `GetWaveInstanceAddr(index)` (src/blob/groups/slot_sound) is inlined by umerge: that is the jal to
 * ValidateWaveInstance with the index kept in a3 and the `D_80156D38[index].data` load. The function therefore
 * only matches in the slot_sound group (or the whole-program unit); no quirks.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    /* 0x00 */ u16 count0;
    /* 0x04 */ u32 *ptrs0;
    /* 0x08 */ u16 count1;
    /* 0x0A */ u16 count2;
    /* 0x0C */ u16 *data1;
    /* 0x10 */ u32 *ptrs2;
    /* 0x14 */ u16 count3;
    /* 0x16 */ u16 count4;
    /* 0x18 */ u16 count5;
    /* 0x1C */ u16 *data3;
    /* 0x20 */ u16 *data4;
    /* 0x24 */ u32 *ptrs5;
} ModelTables;

extern s32 gTextWaveInst;
extern ModelTables gText;

void *GetWaveInstanceAddr(s32 index);

/* PC: the bank is already in host order (port/assets/text.c). The offsets stay in the file, since a host pointer
 * doesn't fit their 4-byte slots; the three pointer sections are native char * tables built from them, and the
 * header points at those. Readers index them as char ** with host strides. */
char **pc_text_strings(s32 section, u32 *offsets, s32 count, void *base);

void AdjustTextWavePointers(void) {
    u32 *base;
    u32 *p;

    base = GetWaveInstanceAddr(gTextWaveInst);
    p = base;
    gText.count0 = *p++;
    gText.ptrs0 = (u32 *)pc_text_strings(0, p, gText.count0, base);
    p += gText.count0;
    gText.count1 = *p++;
    gText.data1 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.count1 + 1) & ~1));
    gText.count2 = *p++;
    gText.ptrs2 = (u32 *)pc_text_strings(2, p, gText.count2, base);
    p += gText.count2;
    gText.count3 = *p++;
    gText.data3 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.count3 + 1) & ~1));
    gText.count4 = *p++;
    gText.data4 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.count4 + 1) & ~1));
    gText.count5 = *p++;
    gText.ptrs5 = (u32 *)pc_text_strings(5, p, gText.count5, base);
}
