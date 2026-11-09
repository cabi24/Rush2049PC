#include "game/game.h"

typedef struct InputRecord {
    u8 unk0, player_id;
    u8 unk2[2];
    u32 pressed;
    u32 unk8;
    u32 repeated;
    u8 unk10[0x48 - 0x10]; void *ref;
} InputRecord;
extern InputRecord gPlayers[];

VIEW(u8[][13], gScoreName);
#define gScoreName gScoreName_view

VIEW(intptr_t[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view
VIEW(s8[3][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view

extern s32 CountdownTimeOut(void);

void ControlName(s32 player)
{
    s32 row, place;
    InputRecord *input;
    u8 *name;

    input = &gPlayers[player];
    if (input->repeated & 0x400) {
        PlayUDSound(input->repeated);
        if (highscore_gNameSelect[player] == -1) highscore_gNameSelect[player] = 52;
        else if (highscore_gNameSelect[player] == -2) highscore_gNameSelect[player] = 49;
        else if (highscore_gNameSelect[player] == -3) highscore_gNameSelect[player] = 46;
        else if (highscore_gNameSelect[player] < 3) highscore_gNameSelect[player] = -3;
        else if (highscore_gNameSelect[player] < 6) highscore_gNameSelect[player] = -2;
        else if (highscore_gNameSelect[player] < 9) highscore_gNameSelect[player] = -1;
        else highscore_gNameSelect[player] -= 9;
    } else if (input->repeated & 0x800) {
        PlayUDSound(input->repeated);
        if (highscore_gNameSelect[player] == -1) highscore_gNameSelect[player] = 7;
        else if (highscore_gNameSelect[player] == -2) highscore_gNameSelect[player] = 4;
        else if (highscore_gNameSelect[player] == -3) highscore_gNameSelect[player] = 1;
        else if (highscore_gNameSelect[player] < 45) highscore_gNameSelect[player] += 9;
        else if (highscore_gNameSelect[player] < 48) highscore_gNameSelect[player] = -3;
        else if (highscore_gNameSelect[player] < 51) highscore_gNameSelect[player] = -2;
        else highscore_gNameSelect[player] = -1;
    } else if (input->repeated & 0x1000) {
        PlayLRSound(input->repeated);
        if (highscore_gNameSelect[player] == -1) highscore_gNameSelect[player] = -2;
        else if (highscore_gNameSelect[player] == -2) highscore_gNameSelect[player] = -3;
        else if (highscore_gNameSelect[player] == -3) highscore_gNameSelect[player] = -1;
        else if (highscore_gNameSelect[player] % 9 == 0) highscore_gNameSelect[player] += 8;
        else highscore_gNameSelect[player]--;
    } else if (input->repeated & 0x2000) {
        PlayLRSound(input->repeated);
        if (highscore_gNameSelect[player] == -1) highscore_gNameSelect[player] = -3;
        else if (highscore_gNameSelect[player] == -2) highscore_gNameSelect[player] = -1;
        else if (highscore_gNameSelect[player] == -3) highscore_gNameSelect[player] = -2;
        else if (highscore_gNameSelect[player] % 9 == 8) highscore_gNameSelect[player] -= 8;
        else highscore_gNameSelect[player]++;
    }
    if ((input->pressed & 3) || CountdownTimeOut()) {
        PlayStartABSound(input->pressed);
        if ((input->pressed & 1) || CountdownTimeOut()) {
            highscore_gNameSelect[player] = -1;
        }
        if (highscore_gNameSelect[player] == -2) {
            if (highscore_gNameLen[player] > 0) {
                highscore_gNameLen[player]--;
                gScoreName[player][highscore_gNameLen[player]] = 0;
            }
        } else if (highscore_gNameSelect[player] == -1) {
            while (gScoreName[player][highscore_gNameLen[player] - 1] == ' ') {
                highscore_gNameLen[player]--;
                gScoreName[player][highscore_gNameLen[player]] = 0;
            }
            name = gScoreName[player];
            censor((intptr_t)name);
            gDrawName[player] = 0;
            gThisPlayer[player] = gLastPlayer[input->player_id] = FindTempNameIdx(name);
            for (row = 0; row < 3; row++) {
                for (place = 0; place < 5; place++) {
                    if (gNewTotalScore[row][place] == player) {
                        gTotalNames[gThisTrack][row][place] = gThisPlayer[player];
                    }
                }
            }
        } else if (highscore_gNameLen[player] < 12 &&
                   (highscore_gNameSelect[player] != -3 || highscore_gNameLen[player] > 0)) {
            if (highscore_gNameSelect[player] == -3) {
                gScoreName[player][highscore_gNameLen[player]] = ' ';
            } else {
                gScoreName[player][highscore_gNameLen[player]] = gPlayerNameChars[highscore_gNameSelect[player]];
            }
            highscore_gNameLen[player]++;
            gScoreName[player][highscore_gNameLen[player]] = 0;
            if (highscore_gNameLen[player] == 12) highscore_gNameSelect[player] = -1;
        }
    } else if (input->pressed & 4) {
        PlayStartABSound(input->pressed);
        if (highscore_gNameLen[player] > 0) {
            highscore_gNameLen[player]--;
            gScoreName[player][highscore_gNameLen[player]] = 0;
        }
    }
}
