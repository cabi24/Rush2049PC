/* GetPlayerTrack: the track a profile last chose (MAT3 save.c). */
#include "game/game.h"

#define kStunt 4      /* the race types (MAT3 enum kNumRaceTypes) */
#define kObstacle 5
#define kBattle 6

/* The track (arena) the profile last chose for race type raceType: from the battle, stunt or obstacle settings, else
 * the race settings. The file must be read. */
u8 GetPlayerTrack(tProfile **profile, s32 raceType) {
    switch (raceType) {
    case kBattle: return (*(*profile)->datah)->battleSettings.track;
    case kStunt: return (*(*profile)->datah)->stuntSettings.track;
    case kObstacle: return (*(*profile)->datah)->obstacleSettings.track;
    default: return (*(*profile)->datah)->raceSettings.track;
    }
}
