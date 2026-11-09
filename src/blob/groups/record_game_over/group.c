/* RecordGameOver: record the race just finished (MAT3 gamerecords.c). */
#include "game/game.h"

#define kOneRace 0    /* the race types (MAT3 enum kNumRaceTypes) */
#define kPractice 1
#define kGhost 2
#define kCircuit 3
#define kStunt 4
#define kObstacle 5
#define kBattle 6

/* Records the results of the race just finished in the players' profiles, by its type (gRace). */
void RecordGameOver(void)
{
    switch(gRace) {
    case kOneRace:case kCircuit:RecordRace();break;
    case kPractice:RecordPractice();break;
    case kGhost:RecordGhost();break;
    case kStunt:RecordStunt();break;
    case kObstacle:RecordObstacle();break;
    case kBattle:RecordBattle();break;
    }
}
