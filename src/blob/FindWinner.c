/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F7F3C = FindWinner (MAT3 GameSource/src/gamerecords.c, GC 0x80015E30): fills gRankedPlayer
 * (gRankedPlayer) with 0..gNumWindows-1, bubble-sorts it by gRace (4 = kStunt: gStuntPlayer[].totalPoints,
 * gStuntPlayer stride 0x78 +0x14; 6 = kBattle: gGameCar[].battle.Points (+0x3A3), then gTeamPoints gTeamPoints
 * [gCarTeam gCarTeam[]]; else gGameCar[].place (+0xEE), higher first) and counts ties /
 * marks tied[] (tied) against the leader. N64 differences from GC: no `ties = 0; tied[] = 0` prologue,
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
typedef struct { u8 pad0[238]; s8 b238; u8 pad1[0x110 - 0xEF]; struct pc_visual visuals[21]; u8 pad1_308[0x380 - 0x308]; void *input; u8 pad1_384[0x3A3 - 0x384]; s8 b931; u8 pad2[952 - 932]; } Car;
extern s16 gNumWindows;
extern s8 gRankedPlayer[];
extern s32 gRace;
extern s16 gNumPlayers;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Ent gStuntPlayer[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gCarTeam[];
extern s8 gTeamPoints[];
extern s32 ties;
extern s8 tied[];

void FindWinner(void)
{
  s32 i;
  s32 j;
  s8 a;
  s8 b;
  /* Original game: ties and tied[] are never cleared here (MAT3 gamerecords.c starts with `ties = 0;` and clears
   * tied[]; the N64 has no such prologue) and nothing else writes them, so the ties counted by an earlier race of
   * the session add to this one's and can reach the results text and RecordBattle's wins. Kept as retail. */
  for (i = 0; i < gNumWindows; i++) {
    gRankedPlayer[i] = i;
  }
  if (gRace == 4) {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < gNumPlayers - 1; i++) {
      for (j = 0; j < gNumPlayers - 1; j++) {
        a = gRankedPlayer[j];
        b = gRankedPlayer[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (ia + ib) {}
        if (gStuntPlayer[ia].key < gStuntPlayer[ib].key) {
          gRankedPlayer[j + 1] = a & 0xFF;
          gRankedPlayer[j] = b;
        }
        if (ia + ib) {}
      }
    }
    for (i = 1; i < gNumPlayers; i++) {
      f = PC_MODEL_SLOT(gRankedPlayer[0]);
      c = PC_MODEL_SLOT(gRankedPlayer[i]);
      if (gStuntPlayer[f].key == gStuntPlayer[c].key) {
        ties++;
        tied[i] = 1;
      }
    }
  } else if (gRace == 6) {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < gNumPlayers - 1; i++) {
      for (j = 0; j < gNumPlayers - 1; j++) {
        a = gRankedPlayer[j];
        b = gRankedPlayer[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (PC_CAR_POINTS(ia) < PC_CAR_POINTS(ib)) {
          gRankedPlayer[j + 1] = a & 0xFF;
          gRankedPlayer[j] = b;
        }
      }
    }
    for (i = 0; i < gNumPlayers - 1; i++) {
      for (j = 0; j < gNumPlayers - 1; j++) {
        a = gRankedPlayer[j];
        b = gRankedPlayer[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (gTeamPoints[gCarTeam[ia]] < gTeamPoints[gCarTeam[ib]]) {
          gRankedPlayer[j + 1] = a & 0xFF;
          gRankedPlayer[j] = b;
        }
      }
    }
    for (i = 1; i < gNumPlayers; i++) {
      f = PC_MODEL_SLOT(gRankedPlayer[0]);
      c = PC_MODEL_SLOT(gRankedPlayer[i]);
      if (PC_CAR_POINTS(f) == PC_CAR_POINTS(c)) {
        ties++;
        tied[i] = 1;
      }
    }
  } else {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < gNumPlayers - 1; i++) {
      for (j = 0; j < gNumPlayers - 1; j++) {
        a = gRankedPlayer[j];
        b = gRankedPlayer[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (PC_CAR_PLACE(ib) < PC_CAR_PLACE(ia)) {
          gRankedPlayer[j + 1] = a & 0xFF;
          gRankedPlayer[j] = b;
        }
      }
    }
    for (i = 1; i < gNumPlayers; i++) {
      f = PC_MODEL_SLOT(gRankedPlayer[0]);
      c = PC_MODEL_SLOT(gRankedPlayer[i]);
      if (PC_CAR_PLACE(f) == PC_CAR_PLACE(c)) {
        ties++;
        tied[i] = 1;
      }
    }
  }
}
