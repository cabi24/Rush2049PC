#include "game/game.h"

typedef s16 Vertex[3];
typedef struct AltPath {
    u8 type;
    u8 unk01[9];
    u16 numPts;
    Vertex *pts;
} AltPath;
/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct ResPath {
    u16 numPrimary;
    u8 unk02[14];
    u8 numAlt;
    u8 unk09[3];
    AltPath *alt;
} ResPath;
VIEW(s16[3], gPathMin);
#define gPathMin gPathMin_view
VIEW(s16[3], gPathMax);
#define gPathMax gPathMax_view
VIEW(ResPath, gResPath);
#define gResPath gResPath_view

void CalcResPathBounds(void)
{
    int i, j, validPt;

    gPathMin[0] = gPathMin[1] = gPathMin[2] = 0x7FFF;
    gPathMax[0] = gPathMax[1] = gPathMax[2] = -0x7FFF;
    for (i = 0; i < gNumResPathPts; i++) {
        if (i < gResPath.numPrimary) {
            validPt = 1;
        } else {
            validPt = 0;
            for (j = 0; j < gResPath.numAlt; j++) {
                if (&gResPathPtList[i] >= gResPath.alt[j].pts &&
                    &gResPathPtList[i] < gResPath.alt[j].pts + gResPath.alt[j].numPts &&
                    gResPath.alt[j].type == 1) {
                    validPt = 1;
                }
            }
        }
        if (validPt == 1) {
            for (j = 0; j < 3; j++) {
                gPathMin[j] = (gPathMin[j] < gResPathPtList[i][j]) ? gPathMin[j] : gResPathPtList[i][j];
                gPathMax[j] = (gPathMax[j] > gResPathPtList[i][j]) ? gPathMax[j] : gResPathPtList[i][j];
            }
        }
    }
}
