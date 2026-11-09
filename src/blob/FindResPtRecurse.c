/* FindResPtRecurse: maps a point of a branch of the resurrection path onto the path it joins (MAT3 maxpath.c). */
#include "game/game.h"

/* Maps point `inIdx` of alternative path `inPath` onto the path the branch rejoins, in proportion: a branch whose
 * two ends join the same path maps onto the stretch between its ends; any other is traced back to the main path
 * (TraceToMain) and on to its end (TraceToEnd, stopping at a branch cars may be put back on when `honorAltRes`), and
 * the point placed that far back from the end, scaled to the stretch of main path the branch bypasses. Repeats on
 * the result until it is on the main path (or a branch cars may be put back on). Sets *outPath and *outIdx and
 * returns 1, or 0 if the paths loop (gPathSearch holds the walk, `depth` this call's step) or a branch can't be
 * placed. */
s32 FindResPtRecurse(s32 inPath, s32 inIdx, s32 *outPath, s32 *outIdx, s32 honorAltRes, s32 depth)
{
    s32 i;
    s32 unused0, unused1;
    s32 numPtsFromMain, numPtsOnShortcut, numPtsOnRes, startIndex, endPath, endIndex, endMainIndex;
    s32 numPts;

    if (depth == 0) {
        *outPath = inPath;
        *outIdx = inIdx;
    }
    gPathSearch[depth] = inPath;
    for (i = 0; i < depth; i++) {
        if (inPath == gPathSearch[i]) {
            return 0;
        }
    }
    numPts = gResPath.alt[inPath].firstPath;
    if (numPts == gResPath.alt[inPath].lastPath) {
        /* both ends on the same path: scale onto the stretch between them */
        if (gResPath.alt[inPath].firstIdx < gResPath.alt[inPath].lastIdx) {
            numPts = gResPath.alt[inPath].lastIdx - gResPath.alt[inPath].firstIdx + 1;
            *outIdx = gResPath.alt[inPath].firstIdx + inIdx * numPts / gResPath.alt[inPath].numPts;
        } else {
            /* the stretch wraps past the end of the main path's lap */
            if (numPts != -1) {
                return 0;
            }
            numPts = gResPath.alt[inPath].lastIdx - gCheckpoints.points[gCheckpoints.loopPt].resPathPt
                + gResPath.numPts - gResPath.alt[inPath].firstIdx + 1;
            *outIdx = gResPath.alt[inPath].firstIdx + inIdx * numPts / gResPath.alt[inPath].numPts;
            if (*outIdx >= gResPath.numPts) {
                *outIdx = *outIdx + gCheckpoints.points[gCheckpoints.loopPt].resPathPt - gResPath.numPts;
            }
        }
        *outPath = gResPath.alt[inPath].firstPath;
        if (*outPath < 0 || (honorAltRes && gResPath.alt[*outPath].resurrect != 0)) {
            return 1;
        }
        return FindResPtRecurse(*outPath, *outIdx, outPath, outIdx, honorAltRes, depth + 1);
    }
    if (!TraceToMain(inPath, inIdx, &startIndex, &numPtsFromMain, 0)) {
        return 0;
    }
    if (!TraceToEnd(inPath, inIdx, &endPath, &endIndex, &numPtsOnShortcut, honorAltRes, 0)) {
        return 0;
    }
    if (!TraceToMain(endPath, endIndex, &endMainIndex, &numPtsOnRes, 0)) {
        return 0;
    }
    /* the main path between where the branch leaves it and where it rejoins: the shorter way round the lap */
    if (endMainIndex < startIndex) {
        numPts = endMainIndex + gResPath.numPts - startIndex - gCheckpoints.points[gCheckpoints.loopPt].resPathPt;
        if (startIndex - endMainIndex < numPts) {
            numPtsFromMain = numPtsFromMain + startIndex - endMainIndex;
            startIndex = endMainIndex;
        } else {
            numPtsOnRes = numPtsOnRes + numPts;
        }
    } else if (endMainIndex - startIndex > startIndex + gResPath.numPts - endMainIndex) {
        numPtsFromMain = numPtsFromMain + startIndex + gResPath.numPts - endMainIndex;
        startIndex = endMainIndex;
    } else {
        numPtsOnRes = numPtsOnRes + endMainIndex - startIndex;
    }
    numPtsOnShortcut = numPtsOnShortcut + numPtsFromMain;
    TraceToIndex(endPath, endIndex, numPtsOnRes - numPtsFromMain * numPtsOnRes / numPtsOnShortcut, outPath, outIdx);
    return 1;
}
