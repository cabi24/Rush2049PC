typedef signed int s32;
typedef signed char s8;
typedef unsigned char u8;

extern s8 gCarHandling[];
extern s8 gCarEngines[];
extern s8 gCarTires[];
extern s8 gCarFrame[];
extern s8 gCarWings[];
extern s8 car_color_choice_a[];
extern s8 car_color_choice_b[];
extern s8 car_color_choice_c[];
extern s8 gTireRims[];
extern s8 gCarHorn[];
extern float gFrontTireScale[];
extern float gRearTireScale[];

extern void SavePlayerCarSetting(void *p, u8 idx, u8 slot, s8 val);

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

