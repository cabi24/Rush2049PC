/* RecordGameOver: record the race just finished (MAT3 gamerecords.c). */
#include "game/game.h"

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
