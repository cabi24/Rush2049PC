/* ControlName: high-score name entry for one player (MAT3 highscorescr.c ControlName): a grid of letters
 * (gPlayerNameChars, 9 a row) with "space", "delete" and "end" under it. */
#include "game/game.h"

/* A player's pad record (MAT3 tPadState, gPlayers). */
typedef struct InputRecord {
    u8 slot, controller;
    u8 unk2[2];
    u32 pressed;            /* 0x04 edges */
    u32 levels;             /* 0x08 */
    u32 repeated;           /* 0x0C the auto-repeating presses */
    u8 unk10[0x48 - 0x10]; void *ref;
} InputRecord;
extern InputRecord gPlayers[];

VIEW(intptr_t[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view
VIEW(s8[3][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view


/* One frame of player's name entry. The d-pad (auto-repeating) moves over the grid, wrapping round its rows and
 * columns, the bottom row's three buttons under the grid's columns 0-2, 3-5 and 6-8. A adds the letter (or a space,
 * not first), deletes the last letter, or ends; Start or the time limit ends at once; B deletes. Ending trims the
 * trailing spaces, censors the name, files it as a temporary name (FindTempNameIdx) for the player's controller,
 * and writes it into the track's tables wherever this player's new scores are. A full name moves to "end". */
void ControlName(s32 player)
{
    s32 row, place;
    InputRecord *input;
    u8 *name;

    input = &gPlayers[player];
    if (input->repeated & BUTTON_UP) {
        PlayUDSound(input->repeated);
        if (highscore_gNameSelect[player] == NAME_SEL_END) highscore_gNameSelect[player] = 52;
        else if (highscore_gNameSelect[player] == NAME_SEL_DELETE) highscore_gNameSelect[player] = 49;
        else if (highscore_gNameSelect[player] == NAME_SEL_SPACE) highscore_gNameSelect[player] = 46;
        else if (highscore_gNameSelect[player] < 3) highscore_gNameSelect[player] = NAME_SEL_SPACE;
        else if (highscore_gNameSelect[player] < 6) highscore_gNameSelect[player] = NAME_SEL_DELETE;
        else if (highscore_gNameSelect[player] < 9) highscore_gNameSelect[player] = NAME_SEL_END;
        else highscore_gNameSelect[player] -= NAME_GRID_COLUMNS;
    } else if (input->repeated & BUTTON_DOWN) {
        PlayUDSound(input->repeated);
        if (highscore_gNameSelect[player] == NAME_SEL_END) highscore_gNameSelect[player] = 7;
        else if (highscore_gNameSelect[player] == NAME_SEL_DELETE) highscore_gNameSelect[player] = 4;
        else if (highscore_gNameSelect[player] == NAME_SEL_SPACE) highscore_gNameSelect[player] = 1;
        else if (highscore_gNameSelect[player] < 45) highscore_gNameSelect[player] += NAME_GRID_COLUMNS;
        else if (highscore_gNameSelect[player] < 48) highscore_gNameSelect[player] = NAME_SEL_SPACE;
        else if (highscore_gNameSelect[player] < 51) highscore_gNameSelect[player] = NAME_SEL_DELETE;
        else highscore_gNameSelect[player] = NAME_SEL_END;
    } else if (input->repeated & BUTTON_LEFT) {
        PlayLRSound(input->repeated);
        if (highscore_gNameSelect[player] == NAME_SEL_END) highscore_gNameSelect[player] = NAME_SEL_DELETE;
        else if (highscore_gNameSelect[player] == NAME_SEL_DELETE) highscore_gNameSelect[player] = NAME_SEL_SPACE;
        else if (highscore_gNameSelect[player] == NAME_SEL_SPACE) highscore_gNameSelect[player] = NAME_SEL_END;
        else if (highscore_gNameSelect[player] % 9 == 0) highscore_gNameSelect[player] += 8;
        else highscore_gNameSelect[player]--;
    } else if (input->repeated & BUTTON_RIGHT) {
        PlayLRSound(input->repeated);
        if (highscore_gNameSelect[player] == NAME_SEL_END) highscore_gNameSelect[player] = NAME_SEL_SPACE;
        else if (highscore_gNameSelect[player] == NAME_SEL_DELETE) highscore_gNameSelect[player] = NAME_SEL_END;
        else if (highscore_gNameSelect[player] == NAME_SEL_SPACE) highscore_gNameSelect[player] = NAME_SEL_DELETE;
        else if (highscore_gNameSelect[player] % 9 == 8) highscore_gNameSelect[player] -= 8;
        else highscore_gNameSelect[player]++;
    }
    if ((input->pressed & BUTTON_ACCEPT) || CountdownTimeOut()) {
        PlayStartABSound(input->pressed);
        if ((input->pressed & BUTTON_START) || CountdownTimeOut()) {
            highscore_gNameSelect[player] = NAME_SEL_END;
        }
        if (highscore_gNameSelect[player] == NAME_SEL_DELETE) {
            if (highscore_gNameLen[player] > 0) {
                highscore_gNameLen[player]--;
                gScoreName[player][highscore_gNameLen[player]] = 0;
            }
        } else if (highscore_gNameSelect[player] == NAME_SEL_END) {
            while (gScoreName[player][highscore_gNameLen[player] - 1] == ' ') {
                highscore_gNameLen[player]--;
                gScoreName[player][highscore_gNameLen[player]] = 0;
            }
            name = gScoreName[player];
            censor(name, (u8 *)gScoreName[4]);   /* the search stops at the table's end */
            gDrawName[player] = 0;
            gThisPlayer[player] = gLastPlayer[input->controller] = FindTempNameIdx(name);
            for (row = 0; row < 3; row++) {
                for (place = 0; place < 5; place++) {
                    if (gNewTotalScore[row][place] == player) {
                        gTotalNames[gThisTrack][row][place] = gThisPlayer[player];
                    }
                }
            }
        } else if (highscore_gNameLen[player] < NAME_MAX &&
                   (highscore_gNameSelect[player] != NAME_SEL_SPACE || highscore_gNameLen[player] > 0)) {
            if (highscore_gNameSelect[player] == NAME_SEL_SPACE) {
                gScoreName[player][highscore_gNameLen[player]] = ' ';
            } else {
                gScoreName[player][highscore_gNameLen[player]] = gPlayerNameChars[highscore_gNameSelect[player]];
            }
            highscore_gNameLen[player]++;
            gScoreName[player][highscore_gNameLen[player]] = 0;
            if (highscore_gNameLen[player] == NAME_MAX) highscore_gNameSelect[player] = NAME_SEL_END;
        }
    } else if (input->pressed & BUTTON_B) {
        PlayStartABSound(input->pressed);
        if (highscore_gNameLen[player] > 0) {
            highscore_gNameLen[player]--;
            gScoreName[player][highscore_gNameLen[player]] = 0;
        }
    }
}
