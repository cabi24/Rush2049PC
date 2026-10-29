/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F7F3C = FindWinner (MAT3 GameSource/src/gamerecords.c, GC 0x80015E30): fills gRankedPlayer
 * (D_80143F54) with 0..gNumWindows-1, bubble-sorts it by gRace (4 = kStunt: gStuntPlayer[].totalPoints,
 * D_80152038 stride 0x78 +0x14; 6 = kBattle: gGameCar[].battle.Points (+0x3A3), then gTeamPoints D_8015256C
 * [gCarTeam D_8012E67C[]]; else gGameCar[].place (+0xEE), higher first) and counts ties (D_80150B60) /
 * marks tied[] (D_80150B68) against the leader. N64 differences from GC: no `ties = 0; tied[] = 0` prologue,
 * the last arm is a plain else (GC `gRace != kPractice`). Slots are loaded before keys (a/b/ia/ib locals:
 * the MAT3 inline compare is structurally wrong in IDO).
 * Base: best (lineage, 117 words colour-only). shaping (disclosed):
 *  - `if (ia + ib) {}` twice in the stunt arm's inner sort loop (after ib, and at the end of the body): the
 *    compiled-out reads form one PRE web with no emitted code that is live across the loop body and takes t4,
 *    so the sort loop's end pointer, &gStuntPlayer and 120 take t5/s0/s1 as in retail (no single holder works);
 *  - `& 0xFF` on the first swap store of every sort loop: a redundant mask costs one ugen temp-ring pop with
 *    no instruction (workbench L65), which puts every following loop's t6-t9 ring phase where retail has it.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct { u8 pad0[0x7C6]; s16 idx; u8 pad1[0x808 - 0x7C8]; } Rec;
typedef struct { u8 pad0[20]; s32 key; u8 pad1[0x78 - 24]; } Ent;
#ifdef TARGET_PC
typedef struct { u8 pad0[238]; s8 b238; u8 pad1[0x110 - 0xEF]; struct pc_visual visuals[21]; u8 pad1_308[0x380 - 0x308]; void *input; u8 pad1_384[0x3A3 - 0x384]; s8 b931; u8 pad2[952 - 932]; } Car;
#else
typedef struct { u8 pad0[238]; s8 b238; u8 pad1[931 - 239]; s8 b931; u8 pad2[952 - 932]; } Car;
#endif
extern s16 D_80151AD0;
extern s8 D_80143F54[];
extern s32 D_8014A110;
extern s16 D_8014A108;
#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Rec D_8014A250[];
#define PC_MODEL_SLOT(i) D_8014A250[i].idx
#endif
extern Ent D_80152038[];
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Car D_80152818[];
#define PC_CAR_PLACE(i) D_80152818[i].b238
#define PC_CAR_POINTS(i) D_80152818[i].b931
#endif
extern s8 gCarTeam[];
extern s8 D_8015256C[];
extern s32 D_80150B60;
extern s8 D_80150B68[];

void FindWinner(void)
{
  s32 i;
  s32 j;
  s8 a;
  s8 b;
  for (i = 0; i < D_80151AD0; i++) {
    D_80143F54[i] = i;
  }
  if (D_8014A110 == 4) {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < D_8014A108 - 1; i++) {
      for (j = 0; j < D_8014A108 - 1; j++) {
        a = D_80143F54[j];
        b = D_80143F54[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (ia + ib) {}
        if (D_80152038[ia].key < D_80152038[ib].key) {
          D_80143F54[j + 1] = a & 0xFF;
          D_80143F54[j] = b;
        }
        if (ia + ib) {}
      }
    }
    for (i = 1; i < D_8014A108; i++) {
      f = PC_MODEL_SLOT(D_80143F54[0]);
      c = PC_MODEL_SLOT(D_80143F54[i]);
      if (D_80152038[f].key == D_80152038[c].key) {
        D_80150B60++;
        D_80150B68[i] = 1;
      }
    }
  } else if (D_8014A110 == 6) {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < D_8014A108 - 1; i++) {
      for (j = 0; j < D_8014A108 - 1; j++) {
        a = D_80143F54[j];
        b = D_80143F54[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (PC_CAR_POINTS(ia) < PC_CAR_POINTS(ib)) {
          D_80143F54[j + 1] = a & 0xFF;
          D_80143F54[j] = b;
        }
      }
    }
    for (i = 0; i < D_8014A108 - 1; i++) {
      for (j = 0; j < D_8014A108 - 1; j++) {
        a = D_80143F54[j];
        b = D_80143F54[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (D_8015256C[gCarTeam[ia]] < D_8015256C[gCarTeam[ib]]) {
          D_80143F54[j + 1] = a & 0xFF;
          D_80143F54[j] = b;
        }
      }
    }
    for (i = 1; i < D_8014A108; i++) {
      f = PC_MODEL_SLOT(D_80143F54[0]);
      c = PC_MODEL_SLOT(D_80143F54[i]);
      if (PC_CAR_POINTS(f) == PC_CAR_POINTS(c)) {
        D_80150B60++;
        D_80150B68[i] = 1;
      }
    }
  } else {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < D_8014A108 - 1; i++) {
      for (j = 0; j < D_8014A108 - 1; j++) {
        a = D_80143F54[j];
        b = D_80143F54[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (PC_CAR_PLACE(ib) < PC_CAR_PLACE(ia)) {
          D_80143F54[j + 1] = a & 0xFF;
          D_80143F54[j] = b;
        }
      }
    }
    for (i = 1; i < D_8014A108; i++) {
      f = PC_MODEL_SLOT(D_80143F54[0]);
      c = PC_MODEL_SLOT(D_80143F54[i]);
      if (PC_CAR_PLACE(f) == PC_CAR_PLACE(c)) {
        D_80150B60++;
        D_80150B68[i] = 1;
      }
    }
  }
}
