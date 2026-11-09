/* GetDefaultOption: an option's default (MAT3 save.c). */
#include "game/game.h"


#define OS_TV_PAL 0
#define DEBUG_FORCE_PAL 0 /* compiled-out development switch */
extern unsigned int osTvType;
/* The default of option selector (0..20 the game options, 21..40 a race type's settings: SavePlayerOption's
 * numbering): option 9 is on for a PAL console. Original game: for any other selector it falls off the end without
 * a return value (MAT3 alike); nothing asks for one. */
s8 GetDefaultOption(int selector)
{
    switch(selector) {
    case kOptLaps:return 3;
    case kOptDrones:return 5;
    case kOptTrackMap:return 1;
    case kOptGravity:return 1;
    case kOptRadar:return 1;
    case kOptElapsed:return 1;
    case kOptRemaining:return 1;
    case kOptTach:return 1;
    case kOptSpeed:return 1;
    case kOptOdometer:return 1;
    case kOptPlace:return 1;
    case kOptShifter:return 1;
    case kOptDifficulty:return 2;
    case kOptGhostSelect:return 0;
    case kOptCatchup:return 2;
    case kOptBackwards:return 0;
    case kOptMirror:return 0;
    case kOptFog:return 0;
    case kOptWind:return 0;
    case kOptMetric:return osTvType == OS_TV_PAL || DEBUG_FORCE_PAL;
    case kOptDeathMode:return 0;
    case kOptPlayerArrows:return 1;
    case kOptCoins:return 1;
    case kOptMusicVolume:return 10;
    case kOptEffectsVolume:return 10;
    case kOptSong:return 12;
    case kOptMono:return 0;
    case kOptLanguage:return 0;
    case 17:return 0;
    case kOptWrongWay:return 1;
    case kOptScreenX:return 0;
    case kOptScreenY:return 0;
    case kOptStuntMode:return 0;
    case kOptStuntFlow:return 0;
    case kOptStuntMinutes:return 5;
    case kOptStuntPoints:return 69;
    case kOptStuntDeaths:return 30;
    case kOptBattleMode:return 0;
    case kOptBattleMinutes:return 8;
    case kOptBattlePoints:return 10;
    case kOptBattleNeg:return 0;
    default:break;
    }
}
