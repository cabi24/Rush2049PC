/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ClearControls (historical label) -- controller-state reset.
 * Under the pad message-queue lock (osRecvMesg(&controlsys_lock_queue, NULL, OS_MESG_BLOCK) ...
 * osJamMesg(..., NULL, OS_MESG_NOBLOCK)) it clears the three combined pad masks
 * and, for each of the four pads, the stick vector and the four per-pad masks.
 * It is the same block UpdateControls runs in its skip-frames branch.
 * No arcade ancestor (N64 SI controller code).
 *
 * Shaping (both visible in the code):
 *  - the three combined masks are one chained assignment; the chain is what
 *    puts &gActiveLevels / &gActiveAutoEdges in v0/v1 (separate statements give lui at).
 *  - the stick vector array is *defined* (tentative/common definition) in this
 *    translation unit: as1 then shares one `lui at` between its two swc1 stores
 *    per pad; with an extern declaration every store gets its own lui.
 */
typedef float f32;
typedef int s32;
typedef struct OSMesgQueue OSMesgQueue;

extern OSMesgQueue controlsys_lock_queue;
extern int osRecvMesg(OSMesgQueue *, void **, int);
extern int osSendMesg(OSMesgQueue *, void *, int);

extern s32 gActiveLevels;      /* all pads: held mask */
extern s32 gActiveAutoEdges;      /* all pads: repeat-pressed mask */
extern s32 gActiveEdges;      /* all pads: pressed mask */
extern f32 gContStick[4][2];       /* per pad stick x, y (defined in this TU) */
extern s32 gNewEdges[4];   /* per pad pressed latch */
extern s32 gAutoEdges[4];   /* per pad repeat-pressed mask */
extern s32 gContLevels[4];   /* per pad held mask */
extern s32 gContEdges[4];   /* per pad pressed mask */

void ClearControls(void)
{
    s32 i;

    osRecvMesg(&controlsys_lock_queue, 0, 1);
    gActiveEdges = gActiveAutoEdges = gActiveLevels = 0;
    for (i = 0; i < 4; i++) {
        gContStick[i][0] = gContStick[i][1] = 0.0f;
        gNewEdges[i] = 0;
        gAutoEdges[i] = 0;
        gContLevels[i] = 0;
        gContEdges[i] = 0;
    }
    osSendMesg(&controlsys_lock_queue, 0, 0);
}
