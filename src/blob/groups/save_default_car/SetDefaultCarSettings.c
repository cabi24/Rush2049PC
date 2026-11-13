/* SetDefaultCarSettings: a car's settings as the game ships them (MAT3 save.c's _SetDefaultCarSettings). */
#include "game/game.h"

/* Gives car car of the profile its default settings (handling, engine, tyres, frame, wings, the three colours, rims,
 * horn, and the front and rear tyre sizes as percent less 75), each through SavePlayerCarSetting. */
void SetDefaultCarSettings(void *profile, s32 car)
{
    SavePlayerCarSetting(profile, (u8)car, 0, gCarHandling[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 1, gCarEngines[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 2, gCarTires[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 3, gCarFrame[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 4, gCarWings[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 5, car_color_choice_a[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 6, car_color_choice_b[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 7, car_color_choice_c[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 8, gTireRims[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 9, gCarHorn[0][car]);
    SavePlayerCarSetting(profile, (u8)car, 10, (s8)(gFrontTireScale[0][car] * 100.0f - 75.0f));
    SavePlayerCarSetting(profile, (u8)car, 11, (s8)(gRearTireScale[0][car] * 100.0f - 75.0f));
}
