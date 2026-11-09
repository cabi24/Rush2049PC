/* SetDefaultCarSettings: a car's settings as the game ships them (MAT3 save.c's _SetDefaultCarSettings). */
#include "game/game.h"

VIEW(s8[], gCarHandling);
#define gCarHandling gCarHandling_view
VIEW(s8[], gCarEngines);
#define gCarEngines gCarEngines_view
VIEW(s8[], gCarTires);
#define gCarTires gCarTires_view
VIEW(s8[], gCarFrame);
#define gCarFrame gCarFrame_view
VIEW(s8[], gCarWings);
#define gCarWings gCarWings_view

VIEW(s8[], gCarHorn);
#define gCarHorn gCarHorn_view
VIEW(float[], gFrontTireScale);
#define gFrontTireScale gFrontTireScale_view
VIEW(float[], gRearTireScale);
#define gRearTireScale gRearTireScale_view

/* Gives car car of the profile its default settings (handling, engine, tyres, frame, wings, the three colours, rims,
 * horn, and the front and rear tyre sizes as percent less 75), each through SavePlayerCarSetting. */
void SetDefaultCarSettings(void *profile, s32 car)
{
    SavePlayerCarSetting(profile, (u8)car, 0, gCarHandling[car]);
    SavePlayerCarSetting(profile, (u8)car, 1, gCarEngines[car]);
    SavePlayerCarSetting(profile, (u8)car, 2, gCarTires[car]);
    SavePlayerCarSetting(profile, (u8)car, 3, gCarFrame[car]);
    SavePlayerCarSetting(profile, (u8)car, 4, gCarWings[car]);
    SavePlayerCarSetting(profile, (u8)car, 5, car_color_choice_a[car]);
    SavePlayerCarSetting(profile, (u8)car, 6, car_color_choice_b[car]);
    SavePlayerCarSetting(profile, (u8)car, 7, car_color_choice_c[car]);
    SavePlayerCarSetting(profile, (u8)car, 8, gTireRims[car]);
    SavePlayerCarSetting(profile, (u8)car, 9, gCarHorn[car]);
    SavePlayerCarSetting(profile, (u8)car, 10, (s8)(gFrontTireScale[car] * 100.0f - 75.0f));
    SavePlayerCarSetting(profile, (u8)car, 11, (s8)(gRearTireScale[car] * 100.0f - 75.0f));
}
