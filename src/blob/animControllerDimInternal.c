/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animControllerDimInternal (0x80094FE8; historical label): for every active input record gPlayers[0..
 * gNumPlayers) (0x4C bytes, controller port at +1) check that the port's controller is present
 * (gPaks[port].present, the 0x304-byte pak/controller records, see src/blob/PS_RumbleStop.c);
 * if the all-present flag differs from spr->unk1A, store it there and re-apply the pad configuration
 * (UpdateBlit). Always returns 1.
 *
 * Shaping: the presence read is an inlined static helper returning the s8 flag. Written inline
 * (with or without named locals) the loop's webs tie and uopt colours the record pointer first
 * (15/41 words, register permutation only); the helper's return value raises the flag web's priority,
 * giving retail's v0/v1/a0 assignment. No stub for the helper is known.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9): Name, the
 * palette (Image) and the texture (Info) are pointer-sized, and so are AnimFunc and AnimDTA in the tail. */
typedef struct {
    /* 0x00 */ intptr_t unk0;
    /* 0x08 */ uintptr_t unk4;
    /* 0x10 */ uintptr_t unk8;
    /* 0x18 */ u16 unkC;
    /* 0x1A */ s16 unkE;
    /* 0x1C */ s16 unk10;
    /* 0x1E */ u16 unk12;
    /* 0x20 */ s16 unk14;
    /* 0x22 */ s16 unk16;
    /* 0x24 */ u8 unk18;
    /* 0x25 */ u8 unk19;
    /* 0x26 */ s8 unk1A;
    /* 0x27 */ u8 unk1B;
    /* 0x28 */ s16 unk1C;
    /* 0x2A */ s16 unk1E;
    /* 0x2C */ s16 unk20;
    /* 0x2E */ s16 unk22;
    /* 0x30 */ s16 color;
    /* 0x32 */ u16 reserved;
    /* 0x38 */ void *animFunc;
    /* 0x40 */ u32 animID;
    /* 0x48 */ intptr_t animDTA;
    /* 0x50 */ u16 index;
} Sprite;

typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 port;
    /* 0x02 */ u8 pad2[0x48 - 0x2]; void *ref;
} InputRec; /* 0x4C */

/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct {
    u8 pad0[6];
    s8 present;
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Controller;

extern s16 gNumPlayers;
extern InputRec gPlayers[];
extern Controller gPaks[];

void UpdateBlit(Sprite *spr);

static s8 controller_present(u8 port)
{
    return gPaks[port].present;
}

s32 animControllerDimInternal(Sprite *spr)
{
    s32 i;
    s32 flag;

    flag = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (controller_present(gPlayers[i].port) == 0) {
            flag = 0;
        }
    }
    if (flag != spr->unk1A) {
        spr->unk1A = flag;
        UpdateBlit(spr);
    }
    return 1;
}
