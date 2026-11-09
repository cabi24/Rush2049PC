/* FindWinner: rank the players at a race's end (MAT3 gamerecords.c). */
#include "game/game.h"

#define kStunt 4      /* gRace (MAT3 enum kNumRaceTypes) */
#define kBattle 6

/* A player's stunt session as this reads it (MAT3 tStuntPlayer): its points. */
typedef struct { u8 pad0[20]; s32 totalPoints; u8 pad1[0x78 - 24]; } StuntPlayer;

VIEW(StuntPlayer[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view

/* Sorts the players into gRankedPlayer, the winner first (a bubble sort): by stunt points in a stunt race, by points
 * and then by team points in a battle, else by place; counts the players who tie with the winner (ties, tied[]). */
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
  if (gRace == kStunt) {
    s16 ia; s16 ib; s16 f; s16 c;
    for (i = 0; i < gNumPlayers - 1; i++) {
      for (j = 0; j < gNumPlayers - 1; j++) {
        a = gRankedPlayer[j];
        b = gRankedPlayer[j + 1];
        ia = PC_MODEL_SLOT(a);
        ib = PC_MODEL_SLOT(b);
        if (ia + ib) {}
        if (gStuntPlayer[ia].totalPoints < gStuntPlayer[ib].totalPoints) {
          gRankedPlayer[j + 1] = a & 0xFF;
          gRankedPlayer[j] = b;
        }
        if (ia + ib) {}
      }
    }
    for (i = 1; i < gNumPlayers; i++) {
      f = PC_MODEL_SLOT(gRankedPlayer[0]);
      c = PC_MODEL_SLOT(gRankedPlayer[i]);
      if (gStuntPlayer[f].totalPoints == gStuntPlayer[c].totalPoints) {
        ties++;
        tied[i] = 1;
      }
    }
  } else if (gRace == kBattle) {
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
