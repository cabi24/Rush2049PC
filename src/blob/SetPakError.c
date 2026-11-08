/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (also identical at -O2) */
/*
 * NOT a strict match: every text word equals retail, but the scorer cannot
 * verify the four references to this function's own .data object
 * (`MATCH (4 section-relative relocations unverified ...)`).
 *
 * Historical label SetPakError is misleading: this arms a rumble/"event"
 * slot for a player (N64-only).  gPakError is a 4-player x 2-slot table of
 * 0x2C-byte records.  On first use every slot is cleared (mode 0, busy 1).
 * The first free slot of `player` (mode == 0; if both are taken the index
 * runs to 2, as in retail) gets: busy 0, deadline = (s32)(3.0f * gRetraceRate)
 * + scheduler frame count, enabled 1, mode, a snapshot of the four per-port
 * values from GetControllerActive/GetControllerStickEdges, and the four s16 arguments plus
 * the controller-present flag gPaks[player].present.  SetControllerActive /
 * player_mode_set are then cleared for all ports and re-set for all ports
 * (mode 1 or 2) or for this player only.  mode == 10 also sets pak_recognize_error_seen.
 *
 * What mattered (all recovered from the code):
 *   - the "initialised" flag at 0x80116DB8 is a function-local static (it is
 *     referenced by no other function; retail word is 0 in .data).  As an
 *     extern its address is kept in a register and `mode` leaves a1;
 *   - the scheduler frame counter is volatile (unfolded `lui/addiu; lw 636`);
 *   - busy/enabled are s8 like the two flags, so every stored 1 and the
 *     `mode == 1` test share one constant web (s3);
 *   - the slot is always written as gPakError[player][j].  Through a pointer
 *     local (slot = ...; slot->x) the instructions are the same but ugen no
 *     longer emits `.noalias reg,$sp`, and as1 then keeps the argument-home
 *     loads behind the slot stores (22 aligned rows / 59 words);
 *   - the controller flag goes through the local `present` (a uopt web in v0;
 *     written inline it is a ugen temp and the tail differs by 21 words).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    /* 0x00 */ s8 busy;
    /* 0x01 */ s8 enabled;
    /* 0x02 */ s8 unk2;
    /* 0x03 */ s8 player;
    /* 0x04 */ u8 primary[4];
    /* 0x08 */ u8 secondary[4];
    /* 0x0C */ u8 previous[4];
    /* 0x10 */ s8 present;
    /* 0x14 */ s32 mode;
    /* 0x18 */ s16 arg[4];
    /* 0x20 */ u8 pad20[8];
    /* 0x28 */ u32 deadline;
} Slot; /* 0x2C */

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

typedef struct {
    u8 pad0[0x27C];
    volatile u32 unk27C;
} Sched;

extern s8 pak_recognize_error_seen;
extern Slot gPakError[4][2];
extern Controller gPaks[];
extern f32 gRetraceRate;
extern Sched gScheduler;

s8 GetControllerActive(s32 arg0);
s8 GetControllerStickEdges(s32 arg0);
void SetControllerActive(s32 arg0, s32 arg1);
void SetControllerStickEdges(s32 arg0, s32 arg1);

void SetPakError(s32 player, s32 mode, s32 arg2, s32 arg3, s32 arg4, s32 arg5) {
    s32 i;
    s32 j;
    s8 present;
    static s8 sInited = 0;

    if (mode == 10) {
        pak_recognize_error_seen = 1;
    }
    if (!sInited) {
        for (j = 0; j < 4; j++) {
            for (i = 0; i < 2; i++) {
                gPakError[j][i].mode = 0;
                gPakError[j][i].busy = 1;
            }
        }
        sInited = 1;
    }
    for (j = 0; j < 2; j++) {
        if (gPakError[player][j].mode == 0) {
            break;
        }
    }
    /* Original game: when both of the pak's records are in use the loop ends with j == 2 and the error is written
     * into the record past the row: the next pak's first one, or past gPakError for pak 3. MAT3 pakerr.c has the
     * same loop over three records. Kept as retail. */
    gPakError[player][j].busy = 0;
    gPakError[player][j].deadline = (s32)(3.0f * gRetraceRate) + gScheduler.unk27C;
    gPakError[player][j].enabled = 1;
    gPakError[player][j].unk2 = 0;
    gPakError[player][j].mode = mode;
    for (i = 0; i < 4; i++) {
        gPakError[player][j].primary[i] = GetControllerActive(i);
        gPakError[player][j].secondary[i] = GetControllerStickEdges(i);
        gPakError[player][j].previous[i] = 0;
    }
    SetControllerActive(-1, 0);
    SetControllerStickEdges(-1, 0);
    if (mode == 1) {
        SetControllerActive(-1, 1);
        SetControllerStickEdges(-1, 1);
    } else {
        gPakError[player][j].player = player;
        if (mode == 2) {
            SetControllerActive(-1, 1);
            SetControllerStickEdges(-1, 1);
        } else {
            SetControllerActive(gPakError[player][j].player, 1);
            SetControllerStickEdges(gPakError[player][j].player, 1);
        }
    }
    present = gPaks[player].present;
    gPakError[player][j].arg[0] = arg2;
    gPakError[player][j].present = present;
    gPakError[player][j].arg[1] = arg3;
    gPakError[player][j].arg[2] = arg4;
    gPakError[player][j].arg[3] = arg5;
}
