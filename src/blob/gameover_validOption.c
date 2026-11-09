/* gameover_validOption: which rows of the game-over menu are offered (MAT3 gameoverscr.c validOption). */
#include "game/game.h"

/* Is the game-over menu's row `option` offered after this race? A circuit race offers only the score, the next
 * race (once the circuit's player has finished) and quit; practice offers the new track and car and quit; every
 * other race all but the next race and the new mode. 1 offered, 0 not. (MAT3 also has a ghost race's rule.) */
s32 gameover_validOption(s32 option)
{
  s32 result = 1;
  if (gRace == kCircuit) {
    if (option == kGameOverMainMenuRaceAgain || option == kGameOverMainMenuNewMode ||
        option == kGameOverMainMenuNewTrack || option == kGameOverMainMenuNewCar) {
      result = 0;
    }
    if (option == kGameOverMainMenuNextRace && (PC_CAR_FINISHED(gCircuitPlayer)) == 0) {
      result = 0;
    }
  } else if (gRace == kPractice) {
    if (option == kGameOverMainMenuScore || option == kGameOverMainMenuRaceAgain ||
        option == kGameOverMainMenuNewMode || option == kGameOverMainMenuNextRace) {
      result = 0;
    }
  } else {
    if (option == kGameOverMainMenuNextRace || option == kGameOverMainMenuNewMode) {
      result = 0;
    }
  }
  return result;
}
