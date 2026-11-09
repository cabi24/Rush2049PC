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

void SetDefaultCarSettings(void *p, s32 idx)
{
    SavePlayerCarSetting(p, (u8)idx, 0, gCarHandling[idx]);
    SavePlayerCarSetting(p, (u8)idx, 1, gCarEngines[idx]);
    SavePlayerCarSetting(p, (u8)idx, 2, gCarTires[idx]);
    SavePlayerCarSetting(p, (u8)idx, 3, gCarFrame[idx]);
    SavePlayerCarSetting(p, (u8)idx, 4, gCarWings[idx]);
    SavePlayerCarSetting(p, (u8)idx, 5, car_color_choice_a[idx]);
    SavePlayerCarSetting(p, (u8)idx, 6, car_color_choice_b[idx]);
    SavePlayerCarSetting(p, (u8)idx, 7, car_color_choice_c[idx]);
    SavePlayerCarSetting(p, (u8)idx, 8, gTireRims[idx]);
    SavePlayerCarSetting(p, (u8)idx, 9, gCarHorn[idx]);
    SavePlayerCarSetting(p, (u8)idx, 10, (s8)(gFrontTireScale[idx] * 100.0f - 75.0f));
    SavePlayerCarSetting(p, (u8)idx, 11, (s8)(gRearTireScale[idx] * 100.0f - 75.0f));
}

