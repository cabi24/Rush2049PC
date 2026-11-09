#include "game/game.h"

void RecordGameOver(void)
{
    switch(gRace) {
    case 0:case 3:RecordRace();break;
    case 1:RecordPractice();break;
    case 2:RecordGhost();break;
    case 4:RecordStunt();break;
    case 5:RecordObstacle();break;
    case 6:RecordBattle();break;
    }
}
