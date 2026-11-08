/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * StartListeners: post one four-vector packet per active player and set the vsync blend.
 * Unless gGameState bit 3 is set: for each player record gPlayers[0..gNumPlayers) (0x4C bytes)
 *   message (+0x44) = AudAddListener(&unitmatrix_origin, &unitmatrix_origin, unitmatrix[2], unitmatrix[1]) (the
 *   identity's z and y axes), except
 *   in mode gRace == 2 for records whose byte +0 is set, which get -1;
 * then AudSetMasterVol((f32)gOptions[13] / 10.0f, 0.1f, 0, 1), the effects volume (the same blend EnableGameSounds
 * computes).
 * AudSetMasterVol is internal (IPA: blend/amount in f20/f22, the flags in s1/s2), so this function only
 * installs in the vsync group. Retail's pointer loop with a recomputed end is IDO's
 * test replacement of the index loop below (as in animControllerDimInternal/RecordGhost).
 * No shaping devices. Own .rodata: 0.1f at 0x801241AC (0x3DCCCCCD).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

typedef struct Vec3 { f32 x, y, z; } Vec3;
typedef struct Packet Packet;
typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 pad1[0x43];
    /* 0x44 */ s32 message;
    /* 0x48 */ void *ref;
} PlayerRec; /* 0x4C */
/* PC: an input record is 80 bytes; +0x44 stays 32 bits: a packet is a heap block, so its window address
 * (docs/port/POINTERS.md 10; pc_prelude.h's PC_N64A, docs/port/NOFIXED.md 2.3). */
#define PACKET(p) (s32)PC_N64A(p)

extern PlayerRec gPlayers[];
extern s32 gGameState;
extern s32 gRace;
extern s16 gNumPlayers;
extern Vec3 unitmatrix_origin;
extern f32 unitmatrix[][3];   /* the identity matrix's rows: the x, y and z axes */
/* The game options, a byte each (Settings): [13] the effects volume, 0..10. */
extern s8 gOptions[];

Packet *AudAddListener(Vec3 *a, Vec3 *b, Vec3 *c, Vec3 *d);
void AudSetMasterVol(f32 blend, f32 amount, s32 first, s32 second);

void StartListeners(void)
{
    s32 i;

    if (!(gGameState & 8)) {
        for (i = 0; i < gNumPlayers; i++) {
            if (gRace != 2 || gPlayers[i].unk0 == 0)
                gPlayers[i].message = PACKET(AudAddListener(&unitmatrix_origin, &unitmatrix_origin, (Vec3 *) unitmatrix[2], (Vec3 *) unitmatrix[1]));
            else gPlayers[i].message = -1;
        }
        AudSetMasterVol((f32)gOptions[13] / 10.0f, 0.1f, 0, 1);
    }
}
