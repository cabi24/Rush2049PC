/* FindResPt: the point of the resurrection path a car on a branch is put back on (MAT3 maxpath.c). */
#include "game/game.h"

/* Finds where to put back a car whose last point was `inIdx` of alternative path `inPath`: with `honorAltRes`, a
 * branch cars may be put back on keeps the point; else FindResPtRecurse maps it onto the path the branch joins.
 * Sets *outPath and *outIdx; returns 0 if no point was found. */
s32 FindResPt(s32 inPath, s32 inIdx, s32 *outPath, s32 *outIdx, s32 honorAltRes) {
    if (honorAltRes && gResPath.alt[inPath].resurrect) {
        *outPath = inPath;
        *outIdx = inIdx;
        return 1;
    }
    return FindResPtRecurse(inPath, inIdx, outPath, outIdx, honorAltRes, 0);
}
